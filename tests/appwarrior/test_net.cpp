#include "appwarrior/net/address.h"
#include "appwarrior/net/poller.h"
#include "appwarrior/net/socket.h"

#include <chrono>
#include <cstddef>
#include <span>
#include <utility>
#include <vector>

#include "appwarrior/testing.h"

using namespace aw::net;
using namespace aw::test;

AW_TEST_CASE("IpAddress: text round-trip and octet form") {
  const auto parsed = IpAddress::from_text("192.168.1.10:5500");
  AW_CHECK(parsed.has_value());
  AW_CHECK(parsed->to_text() == "192.168.1.10:5500");
  AW_CHECK(parsed->octets() == (std::array<std::uint8_t, 4>{192, 168, 1, 10}));
  AW_CHECK(parsed->port() == 5500);
  AW_CHECK(parsed->network_address() == 0xC0A8010AU);

  const auto no_port = IpAddress::from_text("10.0.0.1");
  AW_CHECK(no_port.has_value());
  AW_CHECK(no_port->port() == 0);

  AW_CHECK(!IpAddress::from_text("256.1.1.1").has_value());
  AW_CHECK(!IpAddress::from_text("1.2.3").has_value());
  AW_CHECK(!IpAddress::from_text("1.2.3.4:70000").has_value());
  AW_CHECK(!IpAddress::from_text("1.2.3.4:").has_value());
  AW_CHECK(!IpAddress::from_text("abc").has_value());
}

AW_TEST_CASE("IpAddress: IPv6 parse and text round-trip") {
  const auto loopback = IpAddress::from_text("::1");
  AW_CHECK(loopback.has_value());
  AW_CHECK(loopback->family() == IpAddress::Family::ipv6);
  AW_CHECK(loopback->port() == 0);
  AW_CHECK(loopback->to_text() == "[::1]:0");

  const auto ported = IpAddress::from_text("[2001:db8::1]:5500");
  AW_CHECK(ported.has_value());
  AW_CHECK(ported->family() == IpAddress::Family::ipv6);
  AW_CHECK(ported->port() == 5500);
  AW_CHECK(ported->to_text() == "[2001:db8::1]:5500");
  AW_CHECK(ported->ipv6_bytes()[15] == 1);

  const auto full = IpAddress::from_text("2001:0db8:0000:0000:0000:0000:0000:0001");
  AW_CHECK(full.has_value());
  AW_CHECK(full->to_text() == "[2001:db8::1]:0");

  AW_CHECK(!IpAddress::from_text("2001:::1").has_value());
  AW_CHECK(!IpAddress::from_text("2001:0db8:0:0:0:0:0:0:1").has_value());  // 9 groups
  AW_CHECK(!IpAddress::from_text("[::1]garbage").has_value());
  // A trailing all-digit group stays part of the bare IPv6 host, so a
  // five-digit "port" is rejected as an oversized hex group (ports on
  // IPv6 require brackets).
  AW_CHECK(!IpAddress::from_text("::1:70000").has_value());
}

AW_TEST_CASE("IPv6 loopback listener accepts connections (when available)") {
  auto listener = Listener::create_tcp(IpAddress::Family::ipv6);
  AW_CHECK(listener.has_value());
  auto listener_value = std::move(*listener);
  const auto bound = listener_value.listen(IpAddress::from_text("[::1]:0").value());
  if (!bound.has_value()) {
    // Environment without IPv6 loopback: nothing to test beyond parsing.
    return;
  }
  const auto address = listener_value.local_address();
  AW_CHECK(address.has_value());
  AW_CHECK(address->family() == IpAddress::Family::ipv6);
  AW_CHECK(address->port() != 0);

  auto client = Socket::create_tcp(IpAddress::Family::ipv6);
  AW_CHECK(client.has_value());
  auto client_value = std::move(*client);
  auto connecting = client_value.connect(*address);
  if (!connecting.has_value() && connecting.error() == NetError::would_block) {
    Poller poller;
    poller.add(client_value.descriptor(), PollInterest::write);
    AW_CHECK(poller.wait(std::chrono::milliseconds{2000}).has_value());
    connecting = client_value.connect(*address);
  }
  AW_CHECK(connecting.has_value());

  Poller accept_poller;
  accept_poller.add(listener_value.descriptor(), PollInterest::read);
  AW_CHECK(accept_poller.wait(std::chrono::milliseconds{2000}).has_value());
  const auto accepted = listener_value.accept();
  AW_CHECK(accepted.has_value());
}

AW_TEST_CASE("socket pair: echo round-trip and would-block") {
  auto pair = make_socket_pair();
  AW_CHECK(pair.has_value());
  Socket first = std::move(pair->first);
  Socket second = std::move(pair->second);

  std::vector<std::byte> buffer(8);
  const auto empty_read = second.receive(buffer);
  AW_CHECK(!empty_read.has_value());
  AW_CHECK(empty_read.error() == NetError::would_block);

  const std::vector<std::byte> payload = bytes_from_ascii("ping");
  const auto sent = first.send(payload);
  AW_CHECK(sent.has_value());
  AW_CHECK(*sent == payload.size());

  const auto received = second.receive(buffer);
  AW_CHECK(received.has_value());
  AW_CHECK(*received == payload.size());
  AW_REQUIRE_BYTES(std::span<const std::byte>(buffer).first(*received), "70 69 6e 67");
}

AW_TEST_CASE("socket pair: peer close reports connection_closed") {
  auto pair = make_socket_pair();
  AW_CHECK(pair.has_value());
  Socket first = std::move(pair->first);
  Socket second = std::move(pair->second);
  first.close();

  std::vector<std::byte> buffer(8);
  const auto received = second.receive(buffer);
  AW_CHECK(!received.has_value());
  AW_CHECK(received.error() == NetError::connection_closed);
}

AW_TEST_CASE("listener: connect, accept, and echo over loopback") {
  auto listener = Listener::create_tcp(IpAddress::Family::ipv4);
  AW_CHECK(listener.has_value());
  auto listener_value = std::move(*listener);

  const auto bound = listener_value.listen(IpAddress{127, 0, 0, 1, 0});
  AW_CHECK(bound.has_value());
  const auto address = listener_value.local_address();
  AW_CHECK(address.has_value());
  AW_CHECK(address->port() != 0);

  auto client = Socket::create_tcp(IpAddress::Family::ipv4);
  AW_CHECK(client.has_value());
  auto client_value = std::move(*client);

  // Non-blocking connect: loopback may complete instantly or report
  // would_block (in progress); poll writability until it completes.
  auto connecting = client_value.connect(*address);
  if (!connecting.has_value() && connecting.error() == NetError::would_block) {
    Poller poller;
    poller.add(client_value.descriptor(), PollInterest::write);
    const auto ready = poller.wait(std::chrono::milliseconds{2000});
    AW_CHECK(ready.has_value());
    AW_CHECK(!ready->empty());
    connecting = client_value.connect(*address);
  }
  AW_CHECK(connecting.has_value());

  // Accept on the listener side.
  Poller accept_poller;
  accept_poller.add(listener_value.descriptor(), PollInterest::read);
  const auto accept_ready = accept_poller.wait(std::chrono::milliseconds{2000});
  AW_CHECK(accept_ready.has_value());
  AW_CHECK(!accept_ready->empty());
  auto accepted = listener_value.accept();
  AW_CHECK(accepted.has_value());
  auto server_value = std::move(*accepted);

  // Echo: client -> server -> client.
  const std::vector<std::byte> payload = bytes_from_ascii("loopback");
  const auto sent = client_value.send(payload);
  AW_CHECK(sent.has_value());

  std::vector<std::byte> echo_buffer(payload.size());
  const auto received = server_value.receive(echo_buffer);
  AW_CHECK(received.has_value());
  AW_REQUIRE_BYTES(std::span<const std::byte>(echo_buffer).first(*received),
                   "6c 6f 6f 70 62 61 63 6b");
  const auto echoed = server_value.send(echo_buffer);
  AW_CHECK(echoed.has_value());

  std::vector<std::byte> back(payload.size());
  const auto received_back = client_value.receive(back);
  AW_CHECK(received_back.has_value());
  AW_REQUIRE_BYTES(std::span<const std::byte>(back).first(*received_back),
                   "6c 6f 6f 70 62 61 63 6b");
}

AW_TEST_CASE("poller: reports readability, writability, and closure") {
  auto pair = make_socket_pair();
  AW_CHECK(pair.has_value());
  Socket first = std::move(pair->first);
  Socket second = std::move(pair->second);

  Poller poller;
  poller.add(second.descriptor(), PollInterest::read);
  const auto before = poller.wait(std::chrono::milliseconds{20});
  AW_CHECK(before.has_value());
  AW_CHECK(before->empty());

  const std::vector<std::byte> payload = bytes_from_ascii("x");
  AW_CHECK(first.send(payload).has_value());

  const auto after = poller.wait(std::chrono::milliseconds{2000});
  AW_CHECK(after.has_value());
  AW_CHECK(!after->empty());
  AW_CHECK(after->front().descriptor == second.descriptor());
  AW_CHECK(after->front().readable);

  first.close();
  const auto closed = poller.wait(std::chrono::milliseconds{2000});
  AW_CHECK(closed.has_value());
  AW_CHECK(!closed->empty());
  AW_CHECK(closed->front().closed);
}

// Binds a UDP socket to an ephemeral IPv4 loopback port and returns it
// with its local address (loopback IPv4 is assumed available).
auto bind_udp_loopback() -> std::pair<Socket, IpAddress> {
  auto socket = unwrap(Socket::create_udp(IpAddress::Family::ipv4));
  AW_CHECK(socket.bind(IpAddress{127, 0, 0, 1, 0}).has_value());
  auto address = unwrap(socket.local_address());
  AW_CHECK(address.port() != 0);
  return {std::move(socket), std::move(address)};
}

// Waits until the poller reports the descriptor readable (or closed).
auto wait_for_readable(Poller& poller, NativeSocket descriptor) -> bool {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds{2000};
  while (std::chrono::steady_clock::now() < deadline) {
    const auto events = poller.wait(std::chrono::milliseconds{50});
    if (!events.has_value()) {
      continue;
    }
    for (const PollEvent& event : *events) {
      if (event.descriptor == descriptor && (event.readable || event.closed)) {
        return true;
      }
    }
  }
  return false;
}

AW_TEST_CASE("udp: loopback datagram exchange carries the sender address") {
  auto [first, first_address] = bind_udp_loopback();
  auto [second, second_address] = bind_udp_loopback();
  AW_CHECK(first_address != second_address);

  Poller poller;
  poller.add(first.descriptor(), PollInterest::read);
  poller.add(second.descriptor(), PollInterest::read);

  const std::vector<std::byte> payload = bytes_from_ascii("datagram");
  const auto sent = first.send_to(second_address, payload);
  AW_CHECK(sent.has_value());
  AW_CHECK(*sent == payload.size());

  AW_CHECK(wait_for_readable(poller, second.descriptor()));
  std::vector<std::byte> buffer(64);
  const auto received = second.receive_from(buffer);
  AW_CHECK(received.has_value());
  AW_CHECK(received->bytes_received == payload.size());
  AW_CHECK(received->from == first_address);
  AW_REQUIRE_BYTES(std::span<const std::byte>(buffer).first(received->bytes_received),
                   "64 61 74 61 67 72 61 6d");

  // And back the other way.
  const auto reply = second.send_to(first_address, payload);
  AW_CHECK(reply.has_value());
  AW_CHECK(wait_for_readable(poller, first.descriptor()));
  const auto received_back = first.receive_from(buffer);
  AW_CHECK(received_back.has_value());
  AW_CHECK(received_back->bytes_received == payload.size());
  AW_CHECK(received_back->from == second_address);
}

AW_TEST_CASE("udp: zero-length datagram is valid, not connection_closed") {
  auto [first, first_address] = bind_udp_loopback();
  auto [second, second_address] = bind_udp_loopback();
  AW_CHECK(first_address != second_address);

  const auto sent = first.send_to(second_address, {});
  AW_CHECK(sent.has_value());
  AW_CHECK(*sent == 0);

  Poller poller;
  poller.add(second.descriptor(), PollInterest::read);
  AW_CHECK(wait_for_readable(poller, second.descriptor()));
  std::vector<std::byte> buffer(8);
  const auto received = second.receive_from(buffer);
  AW_CHECK(received.has_value());
  AW_CHECK(received->bytes_received == 0);
  AW_CHECK(received->from == first_address);
}

AW_TEST_CASE("udp: empty queue reports would_block; closed socket is invalid_argument") {
  auto [socket, address] = bind_udp_loopback();
  (void)address;
  std::vector<std::byte> buffer(8);
  const auto received = socket.receive_from(buffer);
  AW_CHECK(!received.has_value());
  AW_CHECK(received.error() == NetError::would_block);

  socket.close();
  const auto after_close = socket.receive_from(buffer);
  AW_CHECK(!after_close.has_value());
  AW_CHECK(after_close.error() == NetError::invalid_argument);
  const auto send_closed = socket.send_to(IpAddress{127, 0, 0, 1, 9}, buffer);
  AW_CHECK(!send_closed.has_value());
  AW_CHECK(send_closed.error() == NetError::invalid_argument);
}

AW_TEST_CASE("udp: IPv6 loopback exchange (when available)") {
  auto first_result = Socket::create_udp(IpAddress::Family::ipv6);
  if (!first_result.has_value()) {
    return;  // no IPv6 in this environment
  }
  auto first = std::move(*first_result);
  const auto bound_first = first.bind(IpAddress::from_text("[::1]:0").value());
  if (!bound_first.has_value()) {
    return;  // no IPv6 loopback in this environment
  }
  auto first_address = unwrap(first.local_address());

  auto second_result = Socket::create_udp(IpAddress::Family::ipv6);
  if (!second_result.has_value()) {
    return;
  }
  auto second = std::move(*second_result);
  const auto bound_second = second.bind(IpAddress::from_text("[::1]:0").value());
  if (!bound_second.has_value()) {
    return;
  }
  auto second_address = unwrap(second.local_address());
  AW_CHECK(first_address.family() == IpAddress::Family::ipv6);
  AW_CHECK(first_address != second_address);

  Poller poller;
  poller.add(second.descriptor(), PollInterest::read);
  const std::vector<std::byte> payload = bytes_from_ascii("v6");
  const auto sent = first.send_to(second_address, payload);
  AW_CHECK(sent.has_value());
  AW_CHECK(wait_for_readable(poller, second.descriptor()));
  std::vector<std::byte> buffer(16);
  const auto received = second.receive_from(buffer);
  AW_CHECK(received.has_value());
  AW_CHECK(received->bytes_received == payload.size());
  AW_CHECK(received->from == first_address);
  AW_REQUIRE_BYTES(std::span<const std::byte>(buffer).first(received->bytes_received),
                   "76 36");
}
