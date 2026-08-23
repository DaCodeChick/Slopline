// Cross-role integration tests: a client and a server connection driven
// over one socket pair. Built only when BOTH BUILD_CLIENT and
// BUILD_SERVER are ON.

#include "hotline/client/connection.h"
#include "hotline/server/connection.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <utility>
#include <vector>

#include "appwarrior/crypto/sha1.h"
#include "appwarrior/net/poller.h"
#include "appwarrior/net/socket.h"
#include "appwarrior/testing.h"
#include "hotline/protocol/constants.h"
#include "hotline/protocol/transaction_cipher.h"

using namespace hotline::client;
using namespace hotline::server;
using namespace hotline::net;
using namespace hotline::protocol;
using namespace aw::net;
using namespace aw::test;

namespace {

// Drives both ends of a connection until `done` returns true or the
// timeout expires. Returns false on timeout.
template <typename Done>
auto drive(ClientConnection& a, ServerConnection& b, Poller& poller, Done done,
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
      if (event.descriptor == a.descriptor() && (event.readable || event.closed)) {
        a.service_readable();
      }
      if (event.descriptor == b.descriptor() && (event.readable || event.closed)) {
        b.service_readable();
      }
      if (event.descriptor == a.descriptor() && event.writable) {
        a.service_writable();
      }
      if (event.descriptor == b.descriptor() && event.writable) {
        b.service_writable();
      }
    }
  }
}

auto make_pair_of_connections(ConnectionEvents& client_events, ConnectionEvents& server_events)
    -> std::pair<std::unique_ptr<ClientConnection>, std::unique_ptr<ServerConnection>> {
  auto pair = make_socket_pair();
  AW_CHECK(pair.has_value());
  auto client = std::make_unique<ClientConnection>(ConnectionConfig{}, client_events);
  auto server = std::make_unique<ServerConnection>(ConnectionConfig{}, server_events);
  client->start(std::move(pair->first));
  server->start(std::move(pair->second));
  return {std::move(client), std::move(server)};
}

}  // namespace

AW_TEST_CASE("establish: TRTP handshake completes in both roles") {
  bool client_established = false;
  bool server_established = false;
  ConnectionEvents client_events;
  client_events.on_established = [&] { client_established = true; };
  ConnectionEvents server_events;
  server_events.on_established = [&] { server_established = true; };

  auto [client, server] = make_pair_of_connections(client_events, server_events);

  Poller poller;
  poller.add(client->descriptor(), PollInterest::read_write);
  poller.add(server->descriptor(), PollInterest::read_write);
  const bool done = drive(*client, *server, poller,
                          [&] { return client_established && server_established; });
  AW_CHECK(done);
  AW_CHECK(client->state() == ConnectionState::established);
  AW_CHECK(server->state() == ConnectionState::established);
  AW_CHECK(server->remote_sub_protocol() == kSubProtocolHotl);
  AW_CHECK(server->remote_version() == kClientSubVersion);
}

AW_TEST_CASE("establish: HTXF transfer handshake carries subVersion 3") {
  auto pair = make_socket_pair();
  AW_CHECK(pair.has_value());
  bool server_established = false;
  ConnectionEvents server_events;
  server_events.on_established = [&] { server_established = true; };
  ClientConnection client;
  ServerConnection server(ConnectionConfig{}, server_events);
  client.start(std::move(pair->first), kSubProtocolHtxf, kTransferSubVersion);
  server.start(std::move(pair->second));

  Poller poller;
  poller.add(client.descriptor(), PollInterest::read_write);
  poller.add(server.descriptor(), PollInterest::read_write);
  AW_CHECK(drive(client, server, poller, [&] { return server_established; }));
  AW_CHECK(server.remote_sub_protocol() == kSubProtocolHtxf);
  AW_CHECK(server.remote_version() == kTransferSubVersion);
}

AW_TEST_CASE("transaction round-trip: client request, server reply") {
  bool established = false;
  ConnectionEvents client_events;
  client_events.on_established = [&] { established = true; };
  ConnectionEvents server_events;

  ReceivedTransaction received_request;
  bool server_got_request = false;
  server_events.on_transaction = [&](const ReceivedTransaction& transaction) {
    server_got_request = true;
    received_request = transaction;
  };
  ReceivedTransaction received_reply;
  bool client_got_reply = false;
  client_events.on_transaction = [&](const ReceivedTransaction& transaction) {
    client_got_reply = true;
    received_reply = transaction;
  };

  auto [client, server] = make_pair_of_connections(client_events, server_events);
  Poller poller;
  poller.add(client->descriptor(), PollInterest::read_write);
  poller.add(server->descriptor(), PollInterest::read_write);
  AW_CHECK(drive(*client, *server, poller, [&] { return established; }));

  const std::vector<std::byte> payload = bytes_from_ascii("hello server");
  client->queue_transaction(TransactionType::ChatSend, 42, payload);
  AW_CHECK(drive(*client, *server, poller, [&] { return server_got_request; }));
  AW_CHECK(received_request.header.type == TransactionType::ChatSend);
  AW_CHECK(received_request.header.id == 42U);
  AW_REQUIRE_BYTES(received_request.data, "68 65 6c 6c 6f 20 73 65 72 76 65 72");
  const std::vector<std::byte> reply_payload = bytes_from_ascii("roger");
  server->queue_transaction(TransactionType::ChatSend, received_request.header.id, reply_payload,
                            true, 0);
  AW_CHECK(drive(*client, *server, poller, [&] { return client_got_reply; }));
  AW_CHECK(received_reply.header.is_reply == 1);
  AW_CHECK(received_reply.header.id == received_request.header.id);
  AW_REQUIRE_BYTES(received_reply.data, "72 6f 67 65 72");
}

AW_TEST_CASE("keepalive carries the historical 2-byte empty field list") {
  bool established = false;
  ConnectionEvents client_events;
  client_events.on_established = [&] { established = true; };
  ConnectionEvents server_events;

  ReceivedTransaction keepalive;
  bool server_got = false;
  server_events.on_transaction = [&](const ReceivedTransaction& transaction) {
    server_got = true;
    keepalive = transaction;
  };

  auto [client, server] = make_pair_of_connections(client_events, server_events);
  Poller poller;
  poller.add(client->descriptor(), PollInterest::read_write);
  poller.add(server->descriptor(), PollInterest::read_write);
  AW_CHECK(drive(*client, *server, poller, [&] { return established; }));

  client->queue_keepalive();
  AW_CHECK(drive(*client, *server, poller, [&] { return server_got; }));
  AW_CHECK(keepalive.header.type == TransactionType::KeepConnectionAlive);
  AW_REQUIRE_BYTES(keepalive.data, "00 00");
}

AW_TEST_CASE("encrypted transactions round-trip through TransactionCipher hooks") {
  const std::vector<std::byte> session_key = bytes_from_hex("01 02 03 04 05 06 07 08");
  const std::vector<std::byte> password = bytes_from_ascii("secret");

  using Cipher = hotline::protocol::auth::TransactionCipher<aw::crypto::Sha1>;

  ConnectionCryptoHooks client_crypto;
  ConnectionCryptoHooks server_crypto;
  {
    const auto cipher = std::make_shared<Cipher>(password, session_key, true);
    client_crypto.choose_flag = [] { return 5; };
    client_crypto.encode = [cipher](std::span<std::byte, 20> header, std::span<std::byte> data,
                                    std::uint8_t flag) { cipher->encode(header, data, flag); };
    client_crypto.decode_header = [cipher](std::span<std::byte, 20> header) {
      return cipher->decode_header(header);
    };
    client_crypto.decode_data = [cipher](std::span<std::byte> data, std::uint8_t flag) {
      cipher->decode_data(data, flag);
    };
  }
  {
    const auto cipher = std::make_shared<Cipher>(password, session_key, false);
    server_crypto.choose_flag = [] { return 0; };
    server_crypto.encode = [cipher](std::span<std::byte, 20> header, std::span<std::byte> data,
                                    std::uint8_t flag) { cipher->encode(header, data, flag); };
    server_crypto.decode_header = [cipher](std::span<std::byte, 20> header) {
      return cipher->decode_header(header);
    };
    server_crypto.decode_data = [cipher](std::span<std::byte> data, std::uint8_t flag) {
      cipher->decode_data(data, flag);
    };
  }

  bool established = false;
  ConnectionEvents client_events;
  client_events.on_established = [&] { established = true; };
  ConnectionEvents server_events;
  ReceivedTransaction received;
  bool server_got = false;
  server_events.on_transaction = [&](const ReceivedTransaction& transaction) {
    server_got = true;
    received = transaction;
  };

  auto [client, server] = make_pair_of_connections(client_events, server_events);
  client->set_crypto(std::move(client_crypto));
  server->set_crypto(std::move(server_crypto));

  Poller poller;
  poller.add(client->descriptor(), PollInterest::read_write);
  poller.add(server->descriptor(), PollInterest::read_write);
  AW_CHECK(drive(*client, *server, poller, [&] { return established; }));

  const std::vector<std::byte> payload = bytes_from_ascii("secret message");
  client->queue_transaction(TransactionType::ChatSend, 9, payload);
  AW_CHECK(drive(*client, *server, poller, [&] { return server_got; }));
  AW_CHECK(received.header.type == TransactionType::ChatSend);
  AW_CHECK(received.header.flag == 5);
  AW_REQUIRE_BYTES(received.data, "73 65 63 72 65 74 20 6d 65 73 73 61 67 65");
}
