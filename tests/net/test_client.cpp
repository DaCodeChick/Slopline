// Client-role tests: a hotline::client::ClientConnection driven against
// a raw peer socket. Built only when BUILD_CLIENT is ON.

#include "hotline/client/connection.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

#include "appwarrior/net/poller.h"
#include "appwarrior/net/socket.h"
#include "appwarrior/testing.h"
#include "hotline/protocol/constants.h"
#include "hotline/protocol/handshake.h"

using namespace hotline::client;
using namespace hotline::net;
using namespace hotline::protocol;
using namespace aw::net;
using namespace aw::test;

AW_TEST_CASE("establish: client completes handshake against a raw peer") {
  auto pair = make_socket_pair();
  AW_CHECK(pair.has_value());
  bool client_established = false;
  ConnectionEvents client_events;
  client_events.on_established = [&] { client_established = true; };
  ClientConnection client(ConnectionConfig{}, std::move(client_events));
  client.start(std::move(pair->first));
  Socket& raw = pair->second;

  Poller poller;
  poller.add(raw.descriptor(), PollInterest::read_write);
  poller.add(client.descriptor(), PollInterest::read_write);

  std::array<std::byte, kClientHandshakeSize> hello{};
  std::size_t hello_bytes = 0;
  bool replied = false;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds{5000};
  while ((!client_established || !replied) && std::chrono::steady_clock::now() < deadline) {
    const auto events = poller.wait(std::chrono::milliseconds{50});
    if (!events.has_value()) {
      continue;
    }
    for (const PollEvent& event : *events) {
      if (event.descriptor == client.descriptor() && (event.readable || event.closed)) {
        client.service_readable();
      }
      if (event.descriptor == client.descriptor() && event.writable) {
        client.service_writable();
      }
      if (event.descriptor == raw.descriptor() && event.readable) {
        const auto received = raw.receive(std::span<std::byte>(hello).subspan(hello_bytes));
        if (received.has_value()) {
          hello_bytes += *received;
          if (hello_bytes == kClientHandshakeSize && !replied) {
            const auto handshake = decode_client_handshake(hello);
            AW_CHECK(handshake.protocol == kProtocolTrTp);
            AW_CHECK(handshake.sub_protocol == kSubProtocolHotl);
            ServerHandshakeReply reply;
            reply.error = 0;
            std::array<std::byte, kServerHandshakeReplySize> reply_bytes{};
            encode_server_handshake_reply(reply, reply_bytes);
            AW_CHECK(raw.send(reply_bytes).has_value());
            replied = true;
          }
        }
      }
    }
  }
  AW_CHECK(replied);
  AW_CHECK(client_established);
  AW_CHECK(client.state() == ConnectionState::established);
}
