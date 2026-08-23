#include "hotline/server/connection.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "hotline/protocol/handshake.h"

namespace hotline::server {

using hotline::net::ConnectionState;
using hotline::protocol::ClientHandshakeValidation;
using hotline::protocol::ServerHandshakeReply;
using hotline::protocol::encode_server_handshake_reply;
using hotline::protocol::kClientHandshakeSize;
using hotline::protocol::kServerHandshakeReplySize;
using hotline::protocol::normalize_reject_reason;
using hotline::protocol::try_decode_client_handshake;
using hotline::protocol::validate_client_handshake;

ServerConnection::ServerConnection(ConnectionConfig config, ConnectionEvents events)
    : ConnectionBase(std::move(config), std::move(events)) {
  expected_bytes_ = kClientHandshakeSize;
}

void ServerConnection::start(aw::net::Socket socket) {
  socket_ = std::move(socket);
  state_ = ConnectionState::awaiting_handshake;
}

auto ServerConnection::remote_sub_protocol() const noexcept -> std::uint32_t {
  return remote_sub_protocol_;
}

auto ServerConnection::remote_version() const noexcept -> std::uint16_t {
  return remote_version_;
}

void ServerConnection::handle_handshake() {
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
  mark_established();
  flush_sends();
}

}  // namespace hotline::server
