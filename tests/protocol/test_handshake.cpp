#include "hotline/protocol/handshake.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "appwarrior/testing.h"

using namespace hotline::protocol;
using namespace aw::test;

// Golden vector (audit/06 §11.1): the historical client's establish message.
AW_TEST_CASE("golden: TRTP establish (client to server)") {
  ClientHandshake handshake;  // defaults: 'TRTP' 'HOTL' version 1 subVersion 2
  std::array<std::byte, kClientHandshakeSize> out{};
  encode_client_handshake(handshake, out);
  AW_REQUIRE_BYTES(out, "54 52 54 50 48 4f 54 4c 00 01 00 02");
}

AW_TEST_CASE("client handshake decode round-trip") {
  const std::vector<std::byte> bytes =
      bytes_from_hex("54 52 54 50 48 4f 54 4c 00 01 00 02");
  const auto decoded = try_decode_client_handshake(std::span<const std::byte>(bytes));
  AW_CHECK(decoded.has_value());
  AW_CHECK(decoded->protocol == kProtocolTrTp);
  AW_CHECK(decoded->sub_protocol == kSubProtocolHotl);
  AW_CHECK(decoded->version == 1);
  AW_CHECK(decoded->sub_version == 2);
}

AW_TEST_CASE("transfer-channel handshake carries subVersion 3") {
  ClientHandshake handshake;
  handshake.sub_protocol = kSubProtocolHtxf;
  handshake.sub_version = kTransferSubVersion;
  std::array<std::byte, kClientHandshakeSize> out{};
  encode_client_handshake(handshake, out);
  AW_REQUIRE_BYTES(out, "54 52 54 50 48 54 58 46 00 01 00 03");
}

// Golden vector (audit/06 §11.1): server accept.
AW_TEST_CASE("golden: TRTP establish accept (server to client)") {
  ServerHandshakeReply reply;  // defaults: 'TRTP', error 0
  std::array<std::byte, kServerHandshakeReplySize> out{};
  encode_server_handshake_reply(reply, out);
  AW_REQUIRE_BYTES(out, "54 52 54 50 00 00 00 00");
}

AW_TEST_CASE("server reject carries the reason, round-trip") {
  ServerHandshakeReply reply;
  reply.error = 5U;
  std::array<std::byte, kServerHandshakeReplySize> out{};
  encode_server_handshake_reply(reply, out);
  AW_REQUIRE_BYTES(out, "54 52 54 50 00 00 00 05");

  const auto decoded = try_decode_server_handshake_reply(
      std::span<const std::byte>(bytes_from_hex("54 52 54 50 00 00 00 05")));
  AW_CHECK(decoded.has_value());
  AW_CHECK(decoded->error == 5U);
}

AW_TEST_CASE("legacy 'NICK' alias is accepted on both sides") {
  ClientHandshake handshake;
  handshake.protocol = kProtocolNick;
  std::array<std::byte, kClientHandshakeSize> out{};
  encode_client_handshake(handshake, out);
  AW_REQUIRE_BYTES_MSG(std::span<const std::byte>(out).first<4>(), "4e 49 43 4b", "NICK tag");

  const auto decoded = try_decode_client_handshake(
      std::span<const std::byte>(bytes_from_hex("4e 49 43 4b 48 4f 54 4c 00 01 00 02")));
  AW_CHECK(decoded.has_value());
  AW_CHECK(validate_client_handshake(*decoded) == ClientHandshakeValidation::accepted);

  ServerHandshakeReply reply;
  reply.protocol = kProtocolNick;
  AW_CHECK(validate_server_handshake_reply(reply) == ServerHandshakeReplyValidation::accepted);
}

AW_TEST_CASE("server validation rejects unknown protocol tags") {
  ClientHandshake handshake;
  handshake.protocol = aw::endian::four_cc('H', 'T', 'R', 'K');
  AW_CHECK(validate_client_handshake(handshake) ==
        ClientHandshakeValidation::not_transaction_client);
}

AW_TEST_CASE("server validation rejects version != 1") {
  ClientHandshake handshake;
  handshake.version = 2;
  AW_CHECK(validate_client_handshake(handshake) ==
        ClientHandshakeValidation::incompatible_version);
}

AW_TEST_CASE("client reply validation mirrors the historical GetConnectStatus") {
  ServerHandshakeReply ok;
  AW_CHECK(validate_server_handshake_reply(ok) == ServerHandshakeReplyValidation::accepted);

  ServerHandshakeReply unknown_protocol;
  unknown_protocol.protocol = aw::endian::four_cc('H', 'T', 'R', 'K');
  AW_CHECK(validate_server_handshake_reply(unknown_protocol) ==
        ServerHandshakeReplyValidation::format_unknown);

  ServerHandshakeReply version_error;
  version_error.error = 3U;
  AW_CHECK(validate_server_handshake_reply(version_error) ==
        ServerHandshakeReplyValidation::version_unknown);
}

AW_TEST_CASE("reject reason 0 is normalized to 1 (UTransact::RejectEstablish)") {
  AW_CHECK(normalize_reject_reason(0U) == 1U);
  AW_CHECK(normalize_reject_reason(1U) == 1U);
  AW_CHECK(normalize_reject_reason(7U) == 7U);
}

AW_TEST_CASE("handshake decode rejects truncation and trailing bytes") {
  const std::vector<std::byte> client = bytes_from_hex("54 52 54 50 48 4f 54 4c 00 01 00 02");
  for (std::size_t prefix = 0; prefix < kClientHandshakeSize; ++prefix) {
    const auto decoded =
        try_decode_client_handshake(std::span<const std::byte>(client).first(prefix));
    AW_CHECK(!decoded.has_value());
    AW_CHECK(decoded.error() == DecodeError::truncated);
  }
  std::vector<std::byte> oversized = client;
  oversized.push_back(std::byte{0});
  const auto trailing = try_decode_client_handshake(std::span<const std::byte>(oversized));
  AW_CHECK(!trailing.has_value());
  AW_CHECK(trailing.error() == DecodeError::trailing_bytes);

  const auto short_reply =
      try_decode_server_handshake_reply(bytes_from_hex("54 52 54 50 00 00 00"));
  AW_CHECK(!short_reply.has_value());
  AW_CHECK(short_reply.error() == DecodeError::truncated);
}
