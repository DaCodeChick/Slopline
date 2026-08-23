#include "hotline/net/connection.h"

#include <algorithm>
#include <array>
#include <deque>
#include <map>
#include <ranges>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "hotline/protocol/field_list.h"
#include "hotline/protocol/handshake.h"

namespace hotline::net {

using namespace hotline::protocol;

enum class CoreRole : std::uint8_t { client, server };

// The shared state machine behind ClientConnection and ServerConnection.
class ConnectionCore {
 public:
  ConnectionCore(CoreRole role, ConnectionConfig config, ConnectionEvents events);

  void start_client(aw::net::Socket socket, std::uint32_t sub_protocol,
                    std::uint16_t sub_version);
  void start_server(aw::net::Socket socket);

  [[nodiscard]] auto state() const noexcept -> ConnectionState;
  [[nodiscard]] auto descriptor() const noexcept -> aw::net::NativeSocket;
  [[nodiscard]] auto is_dead() const noexcept -> bool;
  [[nodiscard]] auto remote_sub_protocol() const noexcept -> std::uint32_t;
  [[nodiscard]] auto remote_version() const noexcept -> std::uint16_t;

  void service_readable();
  void service_writable();

  void queue_transaction(TransactionType type, std::uint32_t id,
                         std::span<const std::byte> data, bool is_reply = false,
                         std::uint32_t error = 0);
  void queue_keepalive();
  void set_crypto(ConnectionCryptoHooks hooks);
  void close() noexcept;

 private:
  enum class ReceiveStage : std::uint8_t { handshake, header, data };

  struct PendingSend {
    std::vector<std::byte> bytes;
    std::size_t offset = 0;
  };

  struct PartialReceive {
    TransactionHeader header;
    std::vector<std::byte> data;
  };

  void queue_bytes(std::vector<std::byte> bytes);
  void mark_dead() noexcept;
  void parse_inbound();
  void complete_transaction(std::vector<std::byte> data);
  void flush_sends();

  CoreRole role_;
  ConnectionConfig config_;
  ConnectionEvents events_;
  aw::net::Socket socket_;
  ConnectionState state_ = ConnectionState::idle;
  ConnectionCryptoHooks crypto_;

  ReceiveStage receive_stage_ = ReceiveStage::handshake;
  std::size_t expected_bytes_ = 0;
  std::vector<std::byte> inbound_;
  std::array<std::byte, kTransactionHeaderSize> header_bytes_{};
  TransactionHeader pending_header_{};
  std::uint8_t pending_flag_ = 0;
  std::vector<std::byte> pending_data_;
  std::map<std::pair<std::uint8_t, std::uint32_t>, PartialReceive> partial_;

  std::deque<PendingSend> send_queue_;

  std::uint32_t remote_sub_protocol_ = 0;
  std::uint16_t remote_version_ = 0;
};

ConnectionCore::ConnectionCore(CoreRole role, ConnectionConfig config, ConnectionEvents events)
    : role_(role), config_(config), events_(std::move(events)) {
  switch (role_) {
    case CoreRole::client:
      receive_stage_ = ReceiveStage::handshake;
      expected_bytes_ = kServerHandshakeReplySize;
      break;
    case CoreRole::server:
      receive_stage_ = ReceiveStage::handshake;
      expected_bytes_ = kClientHandshakeSize;
      break;
  }
}

void ConnectionCore::start_client(aw::net::Socket socket, std::uint32_t sub_protocol,
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

void ConnectionCore::start_server(aw::net::Socket socket) {
  socket_ = std::move(socket);
  state_ = ConnectionState::awaiting_handshake;
}

auto ConnectionCore::state() const noexcept -> ConnectionState { return state_; }

auto ConnectionCore::descriptor() const noexcept -> aw::net::NativeSocket { return socket_.descriptor(); }

auto ConnectionCore::is_dead() const noexcept -> bool { return state_ == ConnectionState::dead; }

auto ConnectionCore::remote_sub_protocol() const noexcept -> std::uint32_t {
  return remote_sub_protocol_;
}

auto ConnectionCore::remote_version() const noexcept -> std::uint16_t { return remote_version_; }

void ConnectionCore::set_crypto(ConnectionCryptoHooks hooks) { crypto_ = std::move(hooks); }

void ConnectionCore::close() noexcept { socket_.close(); }

void ConnectionCore::queue_bytes(std::vector<std::byte> bytes) {
  if (bytes.empty()) {
    return;
  }
  send_queue_.push_back(PendingSend{std::move(bytes), 0});
}

void ConnectionCore::mark_dead() noexcept {
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

void ConnectionCore::queue_transaction(TransactionType type, std::uint32_t id,
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

void ConnectionCore::queue_keepalive() {
  // The historical keepalive: transaction 500 with a 2-byte empty field
  // list body (UFieldData::GetDataHandle creates the 2-byte zero count).
  const FieldList empty;
  const auto fields = encode_field_list(empty);
  if (fields.has_value()) {
    queue_transaction(TransactionType::KeepConnectionAlive, 0, *fields);
  }
}

void ConnectionCore::service_readable() {
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

void ConnectionCore::service_writable() {
  if (state_ == ConnectionState::dead) {
    return;
  }
  flush_sends();
}

void ConnectionCore::parse_inbound() {
  for (;;) {
    if (state_ == ConnectionState::dead || state_ == ConnectionState::closing) {
      return;
    }

    switch (receive_stage_) {
      case ReceiveStage::handshake: {
        if (inbound_.size() < expected_bytes_) {
          return;
        }
        if (role_ == CoreRole::server) {
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

void ConnectionCore::complete_transaction(std::vector<std::byte> data) {
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

void ConnectionCore::flush_sends() {
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

// ---------------------------------------------------------------------------
// Role-specific public classes (thin wrappers over the shared core)
// ---------------------------------------------------------------------------

ClientConnection::ClientConnection(ConnectionConfig config, ConnectionEvents events)
    : core_(std::make_unique<ConnectionCore>(CoreRole::client, std::move(config),
                                             std::move(events))) {}
ClientConnection::~ClientConnection() = default;
ClientConnection::ClientConnection(ClientConnection&&) noexcept = default;
auto ClientConnection::operator=(ClientConnection&&) noexcept -> ClientConnection& = default;

void ClientConnection::start(aw::net::Socket socket, std::uint32_t sub_protocol,
                             std::uint16_t sub_version) {
  core_->start_client(std::move(socket), sub_protocol, sub_version);
}

auto ClientConnection::state() const noexcept -> ConnectionState { return core_->state(); }
auto ClientConnection::descriptor() const noexcept -> aw::net::NativeSocket {
  return core_->descriptor();
}
auto ClientConnection::is_dead() const noexcept -> bool { return core_->is_dead(); }
void ClientConnection::service_readable() { core_->service_readable(); }
void ClientConnection::service_writable() { core_->service_writable(); }
void ClientConnection::queue_transaction(TransactionType type, std::uint32_t id,
                                         std::span<const std::byte> data, bool is_reply,
                                         std::uint32_t error) {
  core_->queue_transaction(type, id, data, is_reply, error);
}
void ClientConnection::queue_keepalive() { core_->queue_keepalive(); }
void ClientConnection::set_crypto(ConnectionCryptoHooks hooks) {
  core_->set_crypto(std::move(hooks));
}
void ClientConnection::close() noexcept { core_->close(); }

ServerConnection::ServerConnection(ConnectionConfig config, ConnectionEvents events)
    : core_(std::make_unique<ConnectionCore>(CoreRole::server, std::move(config),
                                             std::move(events))) {}
ServerConnection::~ServerConnection() = default;
ServerConnection::ServerConnection(ServerConnection&&) noexcept = default;
auto ServerConnection::operator=(ServerConnection&&) noexcept -> ServerConnection& = default;

void ServerConnection::start(aw::net::Socket socket) { core_->start_server(std::move(socket)); }

auto ServerConnection::state() const noexcept -> ConnectionState { return core_->state(); }
auto ServerConnection::descriptor() const noexcept -> aw::net::NativeSocket {
  return core_->descriptor();
}
auto ServerConnection::is_dead() const noexcept -> bool { return core_->is_dead(); }
auto ServerConnection::remote_sub_protocol() const noexcept -> std::uint32_t {
  return core_->remote_sub_protocol();
}
auto ServerConnection::remote_version() const noexcept -> std::uint16_t {
  return core_->remote_version();
}
void ServerConnection::service_readable() { core_->service_readable(); }
void ServerConnection::service_writable() { core_->service_writable(); }
void ServerConnection::queue_transaction(TransactionType type, std::uint32_t id,
                                         std::span<const std::byte> data, bool is_reply,
                                         std::uint32_t error) {
  core_->queue_transaction(type, id, data, is_reply, error);
}
void ServerConnection::queue_keepalive() { core_->queue_keepalive(); }
void ServerConnection::set_crypto(ConnectionCryptoHooks hooks) {
  core_->set_crypto(std::move(hooks));
}
void ServerConnection::close() noexcept { core_->close(); }

}  // namespace hotline::net
