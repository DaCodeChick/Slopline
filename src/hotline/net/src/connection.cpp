#include "hotline/net/connection.h"

#include <algorithm>
#include <array>
#include <ranges>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "hotline/protocol/field_list.h"
#include "hotline/protocol/handshake.h"

namespace hotline::net {

using namespace hotline::protocol;

Connection::Connection(ConnectionRole role, ConnectionConfig config, ConnectionEvents events)
    : role_(role), config_(config), events_(std::move(events)) {
  switch (role_) {
    case ConnectionRole::client:
      receive_stage_ = ReceiveStage::handshake;
      expected_bytes_ = kServerHandshakeReplySize;
      break;
    case ConnectionRole::server:
      receive_stage_ = ReceiveStage::handshake;
      expected_bytes_ = kClientHandshakeSize;
      break;
  }
}

void Connection::start_client(aw::net::Socket socket, std::uint32_t sub_protocol,
                              std::uint16_t sub_version) {
  socket_ = std::move(socket);
  state_ = ConnectionState::awaiting_handshake_reply;

  ClientHandshake handshake;
  handshake.protocol = kProtocolTrTp;
  handshake.sub_protocol = sub_protocol;
  handshake.version = kProtocolVersion;
  handshake.sub_version = sub_version;
  std::array<std::byte, kClientHandshakeSize> bytes{};
  encode_client_handshake(handshake, bytes);
  queue_bytes(std::vector<std::byte>(bytes.begin(), bytes.end()));
  flush_sends();
}

void Connection::start_server(aw::net::Socket socket) {
  socket_ = std::move(socket);
  state_ = ConnectionState::awaiting_handshake;
}

auto Connection::state() const noexcept -> ConnectionState { return state_; }

auto Connection::descriptor() const noexcept -> int { return socket_.descriptor(); }

auto Connection::is_dead() const noexcept -> bool { return state_ == ConnectionState::dead; }

auto Connection::remote_sub_protocol() const noexcept -> std::uint32_t {
  return remote_sub_protocol_;
}

auto Connection::remote_version() const noexcept -> std::uint16_t { return remote_version_; }

void Connection::set_crypto(CryptoHooks hooks) { crypto_ = std::move(hooks); }

void Connection::close() noexcept { socket_.close(); }

void Connection::queue_bytes(std::vector<std::byte> bytes) {
  if (bytes.empty()) {
    return;
  }
  send_queue_.push_back(PendingSend{std::move(bytes), 0});
}

void Connection::mark_dead() noexcept {
  if (state_ == ConnectionState::dead) {
    return;
  }
  const bool was_dead = state_ == ConnectionState::dead;
  state_ = ConnectionState::dead;
  socket_.close();
  if (!was_dead && events_.on_closed) {
    events_.on_closed();
  }
}

void Connection::queue_transaction(TransactionType type, std::uint32_t id,
                                   std::span<const std::byte> data, bool is_reply,
                                   std::uint32_t error) {
  TransactionHeader header;
  header.is_reply = is_reply ? 1 : 0;
  header.type = type;
  header.id = id;
  header.error = error;
  header.total_size = static_cast<std::uint32_t>(data.size());
  header.data_size = static_cast<std::uint32_t>(data.size());

  std::array<std::byte, kTransactionHeaderSize> header_bytes{};
  std::vector<std::byte> data_bytes(data.begin(), data.end());

  if (crypto_.choose_flag && crypto_.encode) {
    const std::uint8_t flag = crypto_.choose_flag();
    header.flag = flag;
    encode_header(header, header_bytes);
    crypto_.encode(header_bytes, data_bytes, flag);
  } else {
    header.flag = 0;
    encode_header(header, header_bytes);
  }

  std::vector<std::byte> bytes;
  bytes.reserve(kTransactionHeaderSize + data_bytes.size());
  bytes.insert(bytes.end(), header_bytes.begin(), header_bytes.end());
  bytes.insert(bytes.end(), data_bytes.begin(), data_bytes.end());
  queue_bytes(std::move(bytes));
  flush_sends();
}

void Connection::queue_keepalive() {
  // The historical keepalive: transaction 500 with a 2-byte empty field
  // list body (UFieldData::GetDataHandle creates the 2-byte zero count).
  const FieldList empty;
  const auto fields = encode_field_list(empty);
  if (fields.has_value()) {
    queue_transaction(TransactionType::KeepConnectionAlive, 0, *fields);
  }
}

void Connection::service_readable() {
  if (state_ == ConnectionState::dead) {
    return;
  }

  std::array<std::byte, 4096> scratch{};
  for (;;) {
    const auto received = socket_.receive(scratch);
    if (!received.has_value()) {
      if (received.error() == aw::net::NetError::would_block) {
        break;
      }
      if (received.error() == aw::net::NetError::interrupted) {
        continue;
      }
      mark_dead();
      return;
    }
    inbound_.insert(inbound_.end(), scratch.begin(), scratch.begin() + static_cast<std::ptrdiff_t>(*received));
  }
  parse_inbound();
}

void Connection::service_writable() {
  if (state_ == ConnectionState::dead) {
    return;
  }
  flush_sends();
}

void Connection::parse_inbound() {
  for (;;) {
    if (state_ == ConnectionState::dead || state_ == ConnectionState::closing) {
      return;
    }

    switch (receive_stage_) {
      case ReceiveStage::handshake: {
        if (inbound_.size() < expected_bytes_) {
          return;
        }
        if (role_ == ConnectionRole::server) {
          const auto handshake = try_decode_client_handshake(
              std::span<const std::byte>(inbound_).first(kClientHandshakeSize));
          if (!handshake.has_value()) {
            mark_dead();
            return;
          }
          remote_sub_protocol_ = handshake->sub_protocol;
          remote_version_ = handshake->sub_version;

          const ClientHandshakeValidation validation = validate_client_handshake(*handshake);
          if (validation == ClientHandshakeValidation::not_transaction_client) {
            inbound_.erase(inbound_.begin(),
                           inbound_.begin() + static_cast<std::ptrdiff_t>(kClientHandshakeSize));
            mark_dead();
            return;
          }
          if (validation == ClientHandshakeValidation::incompatible_version) {
            ServerHandshakeReply reject;
            reject.error = normalize_reject_reason(1);
            std::array<std::byte, kServerHandshakeReplySize> bytes{};
            encode_server_handshake_reply(reject, bytes);
            queue_bytes(std::vector<std::byte>(bytes.begin(), bytes.end()));
            inbound_.erase(inbound_.begin(),
                           inbound_.begin() + static_cast<std::ptrdiff_t>(kClientHandshakeSize));
            state_ = ConnectionState::closing;
            flush_sends();
            return;
          }

          ServerHandshakeReply accept;
          std::array<std::byte, kServerHandshakeReplySize> bytes{};
          encode_server_handshake_reply(accept, bytes);
          queue_bytes(std::vector<std::byte>(bytes.begin(), bytes.end()));
          inbound_.erase(inbound_.begin(),
                         inbound_.begin() + static_cast<std::ptrdiff_t>(kClientHandshakeSize));
          state_ = ConnectionState::established;
          flush_sends();
          if (events_.on_established) {
            events_.on_established();
          }
        } else {
          const auto reply = try_decode_server_handshake_reply(
              std::span<const std::byte>(inbound_).first(kServerHandshakeReplySize));
          if (!reply.has_value()) {
            mark_dead();
            return;
          }
          inbound_.erase(inbound_.begin(),
                         inbound_.begin() + static_cast<std::ptrdiff_t>(kServerHandshakeReplySize));
          if (validate_server_handshake_reply(*reply) !=
              ServerHandshakeReplyValidation::accepted) {
            mark_dead();
            return;
          }
          state_ = ConnectionState::established;
          if (events_.on_established) {
            events_.on_established();
          }
        }
        receive_stage_ = ReceiveStage::header;
        expected_bytes_ = kTransactionHeaderSize;
        break;
      }

      case ReceiveStage::header: {
        if (inbound_.size() < kTransactionHeaderSize) {
          return;
        }
        std::ranges::copy(inbound_ | std::views::take(kTransactionHeaderSize), header_bytes_.begin());
        inbound_.erase(inbound_.begin(),
                       inbound_.begin() + static_cast<std::ptrdiff_t>(kTransactionHeaderSize));

        // Legacy order: decode the header first (crypto may be active),
        // THEN enforce the receive policy on the decoded sizes.
        pending_flag_ = 0;
        if (crypto_.decode_header) {
          pending_flag_ = crypto_.decode_header(header_bytes_);
        }
        pending_header_ = decode_header(header_bytes_);

        // Historical receive policy: zero or oversized sizes kill the
        // connection (UTransact.cpp _TNProcessIncomingData).
        if (pending_header_.total_size == 0 || pending_header_.data_size == 0 ||
            pending_header_.total_size > config_.max_receive_size ||
            pending_header_.data_size > config_.max_receive_size) {
          mark_dead();
          return;
        }

        pending_data_.clear();
        pending_data_.reserve(pending_header_.data_size);
        receive_stage_ = ReceiveStage::data;
        expected_bytes_ = pending_header_.data_size;
        break;
      }

      case ReceiveStage::data: {
        if (inbound_.size() < expected_bytes_) {
          return;
        }
        pending_data_.insert(pending_data_.end(), inbound_.begin(),
                             inbound_.begin() + static_cast<std::ptrdiff_t>(expected_bytes_));
        inbound_.erase(inbound_.begin(),
                       inbound_.begin() + static_cast<std::ptrdiff_t>(expected_bytes_));
        complete_transaction(std::move(pending_data_));
        receive_stage_ = ReceiveStage::header;
        expected_bytes_ = kTransactionHeaderSize;
        break;
      }
    }
  }
}

void Connection::complete_transaction(std::vector<std::byte> data) {
  if (crypto_.decode_data) {
    // The header was already decoded when it arrived; the data is
    // decoded now, under the flag from the decoded header.
    crypto_.decode_data(data, pending_flag_);
  }

  if (pending_header_.total_size == pending_header_.data_size) {
    if (events_.on_transaction) {
      ReceivedTransaction transaction;
      transaction.header = pending_header_;
      transaction.data = std::move(data);
      events_.on_transaction(transaction);
    }
    return;
  }

  // Multi-part reassembly by (isReply, id).
  const auto key = std::pair<std::uint8_t, std::uint32_t>{pending_header_.is_reply,
                                                          pending_header_.id};
  auto& partial = partial_[key];
  partial.header = pending_header_;
  partial.data.insert(partial.data.end(), data.begin(), data.end());
  if (partial.data.size() >= partial.header.total_size) {
    if (events_.on_transaction) {
      ReceivedTransaction transaction;
      transaction.header = partial.header;
      transaction.data = std::move(partial.data);
      events_.on_transaction(transaction);
    }
    partial_.erase(key);
  }
}

void Connection::flush_sends() {
  while (!send_queue_.empty()) {
    PendingSend& pending = send_queue_.front();
    const auto sent = socket_.send(std::span<const std::byte>(pending.bytes).subspan(pending.offset));
    if (!sent.has_value()) {
      if (sent.error() == aw::net::NetError::would_block) {
        return;
      }
      if (sent.error() == aw::net::NetError::interrupted) {
        continue;
      }
      mark_dead();
      return;
    }
    pending.offset += *sent;
    if (pending.offset >= pending.bytes.size()) {
      send_queue_.pop_front();
    }
  }

  if (state_ == ConnectionState::closing && send_queue_.empty()) {
    mark_dead();
  }
}

}  // namespace hotline::net
