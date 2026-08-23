// Tests for the role-neutral hotline::net::ConnectionBase, using a
// minimal test-local role (a 4-byte "ping" handshake) so the shared
// machinery is exercised independently of the client/server application
// projects.

#include "hotline/net/connection.h"

#include <algorithm>
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
#include "hotline/protocol/transaction.h"

using namespace hotline::net;
using namespace hotline::protocol;
using namespace aw::net;
using namespace aw::test;

namespace {

// A minimal test role proving ConnectionBase is a usable extension point
// outside the application projects: a 4-byte "ping" handshake that both
// ends send on start() and validate on receipt.
class EchoCore final : public ConnectionBase {
 public:
  EchoCore(ConnectionConfig config, ConnectionEvents events)
      : ConnectionBase(std::move(config), std::move(events)) {
    expected_bytes_ = 4;
  }

  void start(aw::net::Socket socket) {
    socket_ = std::move(socket);
    state_ = ConnectionState::awaiting_handshake;
    queue_bytes(bytes_from_ascii("ping"));
    flush_sends();
  }

 private:
  void handle_handshake() override {
    const auto handshake = std::span<const std::byte>(inbound_).first(4);
    const std::vector<std::byte> ping = bytes_from_ascii("ping");
    const bool valid = std::ranges::equal(handshake, ping);
    inbound_.erase(inbound_.begin(), inbound_.begin() + 4);
    if (valid) {
      mark_established();
    } else {
      mark_dead();
    }
  }
};

// Drives two connections until `done` returns true or the timeout
// expires. Returns false on timeout.
template <typename Done>
auto drive(EchoCore& a, EchoCore& b, Poller& poller, Done done,
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
      if (event.descriptor == a.descriptor()) {
        a.service_readable();
        a.service_writable();
      }
      if (event.descriptor == b.descriptor()) {
        b.service_readable();
        b.service_writable();
      }
    }
  }
}

}  // namespace

AW_TEST_CASE("base: custom roles establish and round-trip transactions") {
  auto pair = make_socket_pair();
  AW_CHECK(pair.has_value());
  bool first_established = false;
  bool second_established = false;
  ConnectionEvents first_events;
  first_events.on_established = [&] { first_established = true; };
  ConnectionEvents second_events;
  second_events.on_established = [&] { second_established = true; };

  std::vector<ReceivedTransaction> received;
  second_events.on_transaction = [&](const ReceivedTransaction& transaction) {
    received.push_back(transaction);
  };

  EchoCore first(ConnectionConfig{}, std::move(first_events));
  EchoCore second(ConnectionConfig{}, std::move(second_events));
  first.start(std::move(pair->first));
  second.start(std::move(pair->second));

  Poller poller;
  poller.add(first.descriptor(), PollInterest::read_write);
  poller.add(second.descriptor(), PollInterest::read_write);
  AW_CHECK(drive(first, second, poller, [&] { return first_established && second_established; }));
  AW_CHECK(first.state() == ConnectionState::established);
  AW_CHECK(second.state() == ConnectionState::established);

  const std::vector<std::byte> payload = bytes_from_ascii("through the base");
  first.queue_transaction(TransactionType::ChatSend, 7, payload);
  // The shared keepalive still carries the historical 2-byte body.
  first.queue_keepalive();
  AW_CHECK(drive(first, second, poller, [&] { return received.size() == 2; }));
  AW_CHECK(received.size() == 2);
  AW_CHECK(received[0].header.type == TransactionType::ChatSend);
  AW_CHECK(received[0].header.id == 7U);
  AW_REQUIRE_BYTES(received[0].data, "74 68 72 6f 75 67 68 20 74 68 65 20 62 61 73 65");
  AW_CHECK(received[1].header.type == TransactionType::KeepConnectionAlive);
  AW_REQUIRE_BYTES(received[1].data, "00 00");
}

AW_TEST_CASE("base: receive policy kills the connection on an oversized dataSize") {
  auto pair = make_socket_pair();
  AW_CHECK(pair.has_value());
  bool closed = false;
  ConnectionEvents events;
  events.on_closed = [&] { closed = true; };
  EchoCore connection(ConnectionConfig{}, std::move(events));
  connection.start(std::move(pair->first));
  Socket& raw = pair->second;

  Poller poller;
  poller.add(raw.descriptor(), PollInterest::read_write);
  poller.add(connection.descriptor(), PollInterest::read_write);

  std::array<std::byte, 4> handshake{};
  std::size_t handshake_bytes = 0;
  bool replied = false;
  bool oversized_sent = false;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds{5000};
  while (!closed && std::chrono::steady_clock::now() < deadline) {
    const auto ready = poller.wait(std::chrono::milliseconds{50});
    if (!ready.has_value()) {
      continue;
    }
    for (const PollEvent& event : *ready) {
      if (event.descriptor == connection.descriptor()) {
        connection.service_readable();
        connection.service_writable();
      }
      if (event.descriptor == raw.descriptor() && event.readable) {
        if (handshake_bytes < handshake.size()) {
          const auto received =
              raw.receive(std::span<std::byte>(handshake).subspan(handshake_bytes));
          if (received.has_value()) {
            handshake_bytes += *received;
            if (handshake_bytes == handshake.size() && !replied) {
              AW_CHECK(raw.send(handshake).has_value());  // echo the ping back
              replied = true;
            }
          }
        }
      }
    }
    if (connection.state() == ConnectionState::established && !oversized_sent) {
      TransactionHeader oversized;
      oversized.type = TransactionType::ChatSend;
      oversized.id = 2;
      oversized.total_size = 3;
      oversized.data_size = kFrameworkMaxTransactionReceiveSize + 1;
      std::array<std::byte, kTransactionHeaderSize> oversized_bytes{};
      encode_header(oversized, oversized_bytes);
      AW_CHECK(raw.send(oversized_bytes).has_value());
      oversized_sent = true;
    }
  }
  AW_CHECK(oversized_sent);
  AW_CHECK(closed);
  AW_CHECK(connection.state() == ConnectionState::dead);
}

AW_TEST_CASE("base: peer close reports on_closed") {
  auto pair = make_socket_pair();
  AW_CHECK(pair.has_value());
  bool closed = false;
  ConnectionEvents events;
  events.on_closed = [&] { closed = true; };
  EchoCore connection(ConnectionConfig{}, std::move(events));
  connection.start(std::move(pair->first));
  pair->second.close();

  connection.service_readable();
  AW_CHECK(closed);
  AW_CHECK(connection.state() == ConnectionState::dead);
}
