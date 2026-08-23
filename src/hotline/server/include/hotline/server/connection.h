// Hotline server application: the server-side connection.
//
// ServerConnection derives the role-neutral hotline::net::ConnectionBase
// and owns the server establish path: it waits for the client's 12-byte
// handshake, accepts 'TRTP' or the legacy 'NICK' alias, replies with the
// 8-byte 'TRTP' + error reply (rejecting version != 1 with reason 1),
// and records the client's sub-protocol/version. Everything else —
// transaction framing and reassembly, the receive policy,
// encrypted-transaction hooks, the send queue — is inherited from the
// base in hotline::net.
//
// This is the Hotline server's connection entry point. The client-side
// equivalent lives in the client application project (src/hotline/client):
// each application compiles exactly one role's establish path.

#pragma once

#include "hotline/net/connection.h"

namespace hotline::server {

using hotline::net::ConnectionConfig;
using hotline::net::ConnectionEvents;

class ServerConnection final : public hotline::net::ConnectionBase {
 public:
  ServerConnection(ConnectionConfig config = {}, ConnectionEvents events = {});
  ServerConnection(ServerConnection&&) noexcept = default;
  auto operator=(ServerConnection&&) noexcept -> ServerConnection& = default;

  ServerConnection(const ServerConnection&) = delete;
  auto operator=(const ServerConnection&) = delete;

  // Attaches a socket accepted from a listener; the handshake reply is
  // queued once the client's handshake validates.
  void start(aw::net::Socket socket);

  [[nodiscard]] auto remote_sub_protocol() const noexcept -> std::uint32_t;
  [[nodiscard]] auto remote_version() const noexcept -> std::uint16_t;

 private:
  void handle_handshake() override;

  std::uint32_t remote_sub_protocol_ = 0;
  std::uint16_t remote_version_ = 0;
};

}  // namespace hotline::server
