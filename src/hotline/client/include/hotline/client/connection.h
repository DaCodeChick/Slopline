// Hotline client application: the client-side connection.
//
// ClientConnection derives the role-neutral hotline::net::ConnectionBase
// and owns the client establish path: it sends the 'TRTP' + sub-protocol
// handshake immediately on start() and validates the server's 8-byte
// reply. Everything else — transaction framing and reassembly, the
// receive policy, encrypted-transaction hooks, the send queue — is
// inherited from the base in hotline::net.
//
// This is the Hotline client's connection entry point. The server-side
// equivalent lives in the server application project (src/hotline/server):
// each application compiles exactly one role's establish path.

#pragma once

#include "hotline/net/connection.h"

namespace hotline::client {

using hotline::net::ConnectionConfig;
using hotline::net::ConnectionEvents;
using hotline::net::kClientSubVersion;
using hotline::net::kSubProtocolHotl;

class ClientConnection final : public hotline::net::ConnectionBase {
 public:
  ClientConnection(ConnectionConfig config = {}, ConnectionEvents events = {});
  ClientConnection(ClientConnection&&) noexcept = default;
  auto operator=(ClientConnection&&) noexcept -> ClientConnection& = default;

  ClientConnection(const ClientConnection&) = delete;
  auto operator=(const ClientConnection&) = delete;

  // Attaches a connected socket and sends the handshake immediately.
  void start(aw::net::Socket socket, std::uint32_t sub_protocol = kSubProtocolHotl,
             std::uint16_t sub_version = kClientSubVersion);

 private:
  void handle_handshake() override;
};

}  // namespace hotline::client
