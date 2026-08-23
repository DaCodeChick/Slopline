#include "hotline/client/connection.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "hotline/protocol/handshake.h"

namespace hotline::client {

using hotline::net::ConnectionState;
using hotline::protocol::ClientHandshake;
using hotline::protocol::ServerHandshakeReplyValidation;
using hotline::protocol::encode_client_handshake;
using hotline::protocol::kClientHandshakeSize;
using hotline::protocol::kProtocolTrTp;
using hotline::protocol::kProtocolVersion;
using hotline::protocol::kServerHandshakeReplySize;
using hotline::protocol::try_decode_server_handshake_reply;
using hotline::protocol::validate_server_handshake_reply;

ClientConnection::ClientConnection(ConnectionConfig config, ConnectionEvents events)
    : ConnectionBase(std::move(config), std::move(events)) {
  expected_bytes_ = kServerHandshakeReplySize;
}

void ClientConnection::start(aw::net::Socket socket, std::uint32_t sub_protocol,
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

void ClientConnection::handle_handshake() {
  const auto reply = try_decode_server_handshake_reply(
      std::span<const std::byte>(inbound_).first(kServerHandshakeReplySize));
  if (!reply.has_value()) {
    mark_dead();
    return;
  }
  inbound_.erase(inbound_.begin(),
                 inbound_.begin() + static_cast<std::ptrdiff_t>(kServerHandshakeReplySize));
  if (validate_server_handshake_reply(*reply) != ServerHandshakeReplyValidation::accepted) {
    mark_dead();
    return;
  }
  mark_established();
}

}  // namespace hotline::client
