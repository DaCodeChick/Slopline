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
  auto listener = Listener::create_tcp();
  AW_CHECK(listener.has_value());
  auto listener_value = std::move(*listener);

  const auto bound = listener_value.listen(IpAddress{127, 0, 0, 1, 0});
  AW_CHECK(bound.has_value());
  const auto address = listener_value.local_address();
  AW_CHECK(address.has_value());
  AW_CHECK(address->port() != 0);

  auto client = Socket::create_tcp();
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
