#include "hotline/protocol/tracker.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "appwarrior/testing.h"

using namespace hotline::protocol;
using namespace aw::test;

AW_TEST_CASE("registration: golden UDP add message") {
  TrackerRegistration registration;
  registration.port = 5500;
  registration.user_count = 3;
  registration.flags = 1;
  registration.pass_id = 0x11223344U;
  registration.name = "MyServ";
  registration.description = "desc";
  registration.password = "pw";

  const std::vector<std::byte> encoded = encode_tracker_registration(kRegistrationTypeAdd, registration);
  AW_REQUIRE_BYTES_MSG(
      encoded,
      "00 01 15 7c 00 03 00 01 11 22 33 44 06 4d 79 53 65 72 76 04 64 65 73 63 02 70 77",
      "registration message");

  const auto decoded = try_decode_tracker_registration(encoded);
  AW_CHECK(decoded.has_value());
  AW_CHECK(decoded->type == kRegistrationTypeAdd);
  AW_CHECK(decoded->registration.port == 5500);
  AW_CHECK(decoded->registration.user_count == 3);
  AW_CHECK(decoded->registration.flags == 1);
  AW_CHECK(decoded->registration.pass_id == 0x11223344U);
  AW_CHECK(decoded->registration.name == "MyServ");
  AW_CHECK(decoded->registration.description == "desc");
  AW_CHECK(decoded->registration.password == "pw");
}

AW_TEST_CASE("registration: remove type and shape errors") {
  TrackerRegistration registration;
  registration.name = "x";
  registration.description = "y";
  registration.password = "z";
  const std::vector<std::byte> remove_msg =
      encode_tracker_registration(kRegistrationTypeRemove, registration);
  AW_REQUIRE_BYTES_MSG(std::span<const std::byte>(remove_msg).first<2>(), "00 02", "remove type");

  const auto short_form = try_decode_tracker_registration(bytes_from_hex("00 01 00 00 00 00 00 00"));
  AW_CHECK(!short_form.has_value());
  AW_CHECK(short_form.error() == DecodeError::truncated);

  const auto bad_type = try_decode_tracker_registration(bytes_from_hex(
      "00 09 15 7c 00 03 00 01 11 22 33 44 01 78 01 79 01 7a"));
  AW_CHECK(!bad_type.has_value());
  AW_CHECK(bad_type.error() == DecodeError::wrong_format_tag);
}

AW_TEST_CASE("handshake: v1 golden and reply") {
  const std::vector<std::byte> handshake = encode_tracker_handshake(kTrackerHandshakeVersion1, "", "");
  AW_REQUIRE_BYTES(handshake, "48 54 52 4b 00 01");

  const auto decoded = try_decode_tracker_handshake(handshake);
  AW_CHECK(decoded.has_value());
  AW_CHECK(decoded->version == 1);
  AW_CHECK(decoded->login.empty());

  const auto reply = encode_tracker_handshake_reply(kTrackerHandshakeVersion1);
  AW_REQUIRE_BYTES(reply, "48 54 52 4b 00 01");
  const auto decoded_reply = try_decode_tracker_handshake_reply(reply);
  AW_CHECK(decoded_reply.has_value());
  AW_CHECK(*decoded_reply == 1);
}

AW_TEST_CASE("handshake: v2 golden with 32-byte padded login fields") {
  const std::vector<std::byte> handshake =
      encode_tracker_handshake(kTrackerHandshakeVersion2, "alice", "secret");
  // 'HTRK' + version 2 + login (len 5 + "alice" + 26 zeros) + password
  // (len 6 + "secret" + 25 zeros).
  AW_REQUIRE_BYTES_MSG(
      std::span<const std::byte>(handshake).first<70>(),
      "48 54 52 4b 00 02"
      " 05 61 6c 69 63 65 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"
      " 06 73 65 63 72 65 74 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00",
      "v2 handshake");

  const auto decoded = try_decode_tracker_handshake(handshake);
  AW_CHECK(decoded.has_value());
  AW_CHECK(decoded->version == 2);
  AW_CHECK(decoded->login == "alice");
  AW_CHECK(decoded->password == "secret");

  // Long credentials are clamped to 31 characters (legacy behavior).
  const std::vector<std::byte> clamped = encode_tracker_handshake(
      kTrackerHandshakeVersion2, std::string(40, 'a'), std::string(40, 'b'));
  const auto decoded_clamped = try_decode_tracker_handshake(clamped);
  AW_CHECK(decoded_clamped.has_value());
  AW_CHECK(decoded_clamped->login.size() == 31U);
  AW_CHECK(decoded_clamped->password.size() == 31U);
}

AW_TEST_CASE("handshake: validation errors") {
  const auto wrong_tag = try_decode_tracker_handshake(bytes_from_hex("00 00 00 00 00 01"));
  AW_CHECK(!wrong_tag.has_value());
  AW_CHECK(wrong_tag.error() == DecodeError::wrong_format_tag);

  const auto bad_version = try_decode_tracker_handshake(bytes_from_hex("48 54 52 4b 00 03"));
  AW_CHECK(!bad_version.has_value());
  AW_CHECK(bad_version.error() == DecodeError::unsupported_version);

  // v2 handshake cut short: tag + version + only 63 of the 64 credential bytes.
  std::vector<std::byte> truncated_v2 = bytes_from_hex("48 54 52 4b 00 02");
  truncated_v2.insert(truncated_v2.end(), 63, std::byte{0});
  const auto decoded_truncated_v2 = try_decode_tracker_handshake(truncated_v2);
  AW_CHECK(!decoded_truncated_v2.has_value());
  AW_CHECK(decoded_truncated_v2.error() == DecodeError::truncated);

  const auto bad_reply = try_decode_tracker_handshake_reply(bytes_from_hex("48 54 52 4b"));
  AW_CHECK(!bad_reply.has_value());
  AW_CHECK(bad_reply.error() == DecodeError::truncated);
}

AW_TEST_CASE("server list: golden single-entry message") {
  TrackerServerListMessage message;
  message.total_count = 1;
  TrackerServerEntry entry;
  entry.address = {192, 168, 1, 10};
  entry.port = 5500;
  entry.user_count = 3;
  entry.flags = 0;
  entry.name = "MyServ";
  entry.description = "hi";
  message.servers.push_back(entry);

  const std::vector<std::byte> encoded = encode_tracker_server_list(message);
  AW_REQUIRE_BYTES_MSG(
      encoded,
      "00 01 00 18 00 01 00 01"
      " c0 a8 01 0a 15 7c 00 03 00 00 06 4d 79 53 65 72 76 02 68 69",
      "server list");

  const auto decoded = try_decode_tracker_server_list(encoded);
  AW_CHECK(decoded.has_value());
  AW_CHECK(decoded->total_count == 1);
  AW_CHECK(decoded->servers.size() == 1U);
  AW_CHECK(decoded->servers[0].address == (std::array<std::uint8_t, 4>{192, 168, 1, 10}));
  AW_CHECK(decoded->servers[0].port == 5500);
  AW_CHECK(decoded->servers[0].user_count == 3);
  AW_CHECK(decoded->servers[0].name == "MyServ");
  AW_CHECK(decoded->servers[0].description == "hi");
}

AW_TEST_CASE("server list: multi-entry message round-trips") {
  TrackerServerListMessage message;
  message.total_count = 2;
  TrackerServerEntry first;
  first.address = {10, 0, 0, 1};
  first.port = 5500;
  first.name = "Alpha";
  first.description = "";
  TrackerServerEntry second;
  second.address = {10, 0, 0, 2};
  second.port = 5501;
  second.user_count = 5;
  second.name = "Beta";
  second.description = "the second one";
  message.servers.push_back(first);
  message.servers.push_back(second);

  const auto decoded = try_decode_tracker_server_list(encode_tracker_server_list(message));
  AW_CHECK(decoded.has_value());
  AW_CHECK(decoded->servers.size() == 2U);
  AW_CHECK(decoded->servers[1].name == "Beta");
  AW_CHECK(decoded->servers[1].user_count == 5);
}

AW_TEST_CASE("lookup: found and not-found goldens") {
  TrackerLookupReply found;
  found.found = true;
  found.entry.address = {192, 168, 1, 10};
  found.entry.port = 5500;
  found.entry.user_count = 3;
  found.entry.name = "MyServ";
  found.entry.description = "hi";
  AW_REQUIRE_BYTES_MSG(
      encode_tracker_lookup_reply(found),
      "00 04 00 14 c0 a8 01 0a 15 7c 00 03 00 00 06 4d 79 53 65 72 76 02 68 69",
      "found reply");

  TrackerLookupReply not_found;
  AW_REQUIRE_BYTES_MSG(encode_tracker_lookup_reply(not_found), "00 05 00 00", "not-found reply");

  const auto decoded_found = try_decode_tracker_lookup_reply(bytes_from_hex(
      "00 04 00 14 c0 a8 01 0a 15 7c 00 03 00 00 06 4d 79 53 65 72 76 02 68 69"));
  AW_CHECK(decoded_found.has_value());
  AW_CHECK(decoded_found->found);
  AW_CHECK(decoded_found->entry.name == "MyServ");

  const auto decoded_missing = try_decode_tracker_lookup_reply(bytes_from_hex("00 05 00 00"));
  AW_CHECK(decoded_missing.has_value());
  AW_CHECK(!decoded_missing->found);

  const auto bad_type = try_decode_tracker_lookup_reply(bytes_from_hex("00 09 00 00"));
  AW_CHECK(!bad_type.has_value());
  AW_CHECK(bad_type.error() == DecodeError::wrong_format_tag);
}
