// Server-role tests: a hotline::server::ServerConnection driven against
// a raw peer socket. Built only when BUILD_SERVER is ON.

#include "hotline/server/connection.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include "appwarrior/net/poller.h"
#include "appwarrior/net/socket.h"
#include "appwarrior/testing.h"
#include "hotline/protocol/constants.h"
#include "hotline/protocol/handshake.h"
#include "hotline/protocol/transaction.h"

using namespace hotline::server;
using namespace hotline::net;
using namespace hotline::protocol;
using namespace aw::net;
using namespace aw::test;

namespace {

// Pumps one connection and one raw socket (no Connection wrapper) until
// `done` returns true or the timeout expires.
template <typename Done>
auto pump(ServerConnection& connection, Socket& raw, Poller& poller, Done done,
          std::chrono::milliseconds timeout = std::chrono::milliseconds{5000}) -> bool {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  for (;;) {
    if (done()) {
      return true;
    }
    if (std::chrono::steady_clock::now() >= deadline) {
      return false;
    }
    const auto events = poller.wait(std::chrono::milliseconds{50});
    if (!events.has_value()) {
      continue;
    }
    for (const PollEvent& event : *events) {
      if (event.descriptor == connection.descriptor()) {
        connection.service_readable();
        connection.service_writable();
      }
      (void)raw;
      (void)event;
    }
  }
}

// Sends a raw client handshake over an unwrapped socket.
void send_raw_handshake(Socket& raw, std::uint16_t version = kProtocolVersion,
                        std::uint32_t protocol = kProtocolTrTp) {
  ClientHandshake handshake;
  handshake.protocol = protocol;
  handshake.version = version;
  std::array<std::byte, kClientHandshakeSize> bytes{};
  encode_client_handshake(handshake, bytes);
  AW_CHECK(raw.send(bytes).has_value());
}

}  // namespace

AW_TEST_CASE("establish: legacy NICK handshake alias is accepted") {
  auto pair = make_socket_pair();
  AW_CHECK(pair.has_value());
  bool server_established = false;
  ConnectionEvents server_events;
  server_events.on_established = [&] { server_established = true; };
  ServerConnection server(ConnectionConfig{}, std::move(server_events));
  server.start(std::move(pair->second));
  Socket& raw = pair->first;

  send_raw_handshake(raw, kProtocolVersion, kProtocolNick);

  Poller poller;
  poller.add(server.descriptor(), PollInterest::read_write);
  AW_CHECK(pump(server, raw, poller, [&] { return server_established; }));
  AW_CHECK(server.state() == ConnectionState::established);
}

AW_TEST_CASE("multi-part transactions reassemble by (isReply, id)") {
  auto pair = make_socket_pair();
  AW_CHECK(pair.has_value());
  bool server_established = false;
  ReceivedTransaction reassembled;
  bool server_got = false;
  ConnectionEvents server_events;
  server_events.on_established = [&] { server_established = true; };
  server_events.on_transaction = [&](const ReceivedTransaction& transaction) {
    server_got = true;
    reassembled = transaction;
  };
  ServerConnection server(ConnectionConfig{}, std::move(server_events));
  server.start(std::move(pair->second));
  Socket& raw = pair->first;

  send_raw_handshake(raw);

  Poller poller;
  poller.add(raw.descriptor(), PollInterest::read_write);
  poller.add(server.descriptor(), PollInterest::read_write);
  AW_CHECK(pump(server, raw, poller, [&] { return server_established; }));

  // Two raw parts of one transaction: totalSize 6, dataSize 3 each.
  const auto make_part = [](std::string_view text) {
    TransactionHeader header;
    header.type = TransactionType::ServerMessage;
    header.id = 7;
    header.total_size = 6;
    header.data_size = 3;
    std::array<std::byte, kTransactionHeaderSize> header_bytes{};
    encode_header(header, header_bytes);
    std::vector<std::byte> bytes(header_bytes.begin(), header_bytes.end());
    for (const char character : text) {
      bytes.push_back(static_cast<std::byte>(static_cast<unsigned char>(character)));
    }
    return bytes;
  };
  AW_CHECK(raw.send(make_part("abc")).has_value());
  AW_CHECK(raw.send(make_part("def")).has_value());

  AW_CHECK(pump(server, raw, poller, [&] { return server_got; }));
  AW_CHECK(reassembled.header.total_size == 6U);
  AW_REQUIRE_BYTES(reassembled.data, "61 62 63 64 65 66");
}

AW_TEST_CASE("receive policy: dataSize zero kills the connection") {
  auto pair = make_socket_pair();
  AW_CHECK(pair.has_value());
  bool server_closed = false;
  ConnectionEvents server_events;
  server_events.on_closed = [&] { server_closed = true; };
  ServerConnection server(ConnectionConfig{}, std::move(server_events));
  server.start(std::move(pair->second));
  Socket& raw = pair->first;

  send_raw_handshake(raw);

  Poller poller;
  poller.add(server.descriptor(), PollInterest::read_write);
  AW_CHECK(pump(server, raw, poller,
                [&] { return server.state() == ConnectionState::established; }));

  TransactionHeader empty;
  empty.type = TransactionType::ChatSend;
  empty.id = 1;
  std::array<std::byte, kTransactionHeaderSize> empty_bytes{};
  encode_header(empty, empty_bytes);
  AW_CHECK(raw.send(empty_bytes).has_value());

  AW_CHECK(pump(server, raw, poller, [&] { return server_closed; }));
  AW_CHECK(server.state() == ConnectionState::dead);
}

AW_TEST_CASE("receive policy: oversized dataSize kills the connection") {
  auto pair = make_socket_pair();
  AW_CHECK(pair.has_value());
  bool server_closed = false;
  ConnectionEvents server_events;
  server_events.on_closed = [&] { server_closed = true; };
  ServerConnection server(ConnectionConfig{}, std::move(server_events));
  server.start(std::move(pair->second));
  Socket& raw = pair->first;

  send_raw_handshake(raw);

  Poller poller;
  poller.add(server.descriptor(), PollInterest::read_write);
  AW_CHECK(pump(server, raw, poller,
                [&] { return server.state() == ConnectionState::established; }));

  TransactionHeader oversized;
  oversized.type = TransactionType::ChatSend;
  oversized.id = 2;
  oversized.total_size = 3;
  oversized.data_size = kFrameworkMaxTransactionReceiveSize + 1;
  std::array<std::byte, kTransactionHeaderSize> oversized_bytes{};
  encode_header(oversized, oversized_bytes);
  AW_CHECK(raw.send(oversized_bytes).has_value());

  AW_CHECK(pump(server, raw, poller, [&] { return server_closed; }));
  AW_CHECK(server.state() == ConnectionState::dead);
}

AW_TEST_CASE("incompatible handshake version is rejected with reason 1") {
  auto pair = make_socket_pair();
  AW_CHECK(pair.has_value());
  bool server_closed = false;
  ConnectionEvents server_events;
  server_events.on_closed = [&] { server_closed = true; };
  ServerConnection server(ConnectionConfig{}, std::move(server_events));
  server.start(std::move(pair->second));
  Socket& raw = pair->first;

  send_raw_handshake(raw, 2);  // version 2 — incompatible

  Poller poller;
  poller.add(raw.descriptor(), PollInterest::read_write);
  poller.add(server.descriptor(), PollInterest::read_write);
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds{5000};
  std::array<std::byte, kServerHandshakeReplySize> reply{};
  std::size_t reply_bytes = 0;
  while ((!server_closed || reply_bytes < kServerHandshakeReplySize) &&
         std::chrono::steady_clock::now() < deadline) {
    const auto events = poller.wait(std::chrono::milliseconds{50});
    for (const PollEvent& event : *events) {
      if (event.descriptor == server.descriptor()) {
        server.service_readable();
        server.service_writable();
      }
      if (event.descriptor == raw.descriptor() && event.readable) {
        const auto received = raw.receive(std::span<std::byte>(reply).subspan(reply_bytes));
        if (received.has_value()) {
          reply_bytes += *received;
        }
      }
    }
  }
  AW_CHECK(server_closed);
  AW_CHECK(reply_bytes == kServerHandshakeReplySize);
  AW_REQUIRE_BYTES(reply, "54 52 54 50 00 00 00 01");
}
