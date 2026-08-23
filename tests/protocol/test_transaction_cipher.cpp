#include "hotline/protocol/transaction_cipher.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <span>
#include <string>
#include <vector>

#include "appwarrior/crypto/sha1.h"
#include "appwarrior/testing.h"

using namespace hotline::protocol;
using namespace hotline::protocol::auth;
using namespace aw::test;

namespace {

auto make_header(std::uint8_t flag, TransactionType type, std::uint32_t id) -> TransactionHeader {
  TransactionHeader header;
  header.flag = flag;
  header.type = type;
  header.id = id;
  return header;
}

auto encode_header_bytes(const TransactionHeader& header) -> std::array<std::byte, 20> {
  std::array<std::byte, 20> bytes{};
  hotline::protocol::encode_header(header, bytes);
  return bytes;
}

}  // namespace

AW_TEST_CASE("legacy flag quirk: deterministic reproduction of the 2/7/13 re-roll") {
  using C = TransactionCipher<aw::crypto::Sha1>;
  // r0 >> 4 not in {2,7,13} -> 0.
  AW_CHECK(C::legacy_flag_quirk(0x00, 0x00, 0x00) == 0);
  AW_CHECK(C::legacy_flag_quirk(0x50, 0xFF, 0xFF) == 0);  // 5
  // r0 >> 4 == 2 -> r1 >> 2 (non-zero path).
  AW_CHECK(C::legacy_flag_quirk(0x20, 0x40, 0x00) == 16);
  // r0 >> 4 == 7 -> r1 >> 2.
  AW_CHECK(C::legacy_flag_quirk(0x70, 0x0C, 0x00) == 3);
  // r0 >> 4 == 13, r1 >> 2 == 0 -> (r2 >> 3) + 1.
  AW_CHECK(C::legacy_flag_quirk(0xD0, 0x00, 0x18) == 4);
  // The re-roll can still produce 2/7/13 — the value is then used as-is
  // (the branch does not loop).
  AW_CHECK(C::legacy_flag_quirk(0x20, 0x08, 0x00) == 2);
}

AW_TEST_CASE("encrypted transactions round-trip for every flag class") {
  const std::vector<std::byte> session_key = bytes_from_hex("01 02 03 04 05 06 07 08");
  const std::vector<std::byte> password = bytes_from_ascii("secret");
  const std::vector<std::byte> data = bytes_from_ascii("hello world");

  TransactionCipher<aw::crypto::Sha1> client(password, session_key, true);
  TransactionCipher<aw::crypto::Sha1> server(password, session_key, false);

  const std::vector<std::uint8_t> flags{0, 1, 2, 7, 13, 32};
  for (const std::uint8_t flag : flags) {
    auto header = encode_header_bytes(make_header(flag, TransactionType::ChatMessage, 42));
    std::vector<std::byte> encoded_data = data;
    client.encode(header, encoded_data, flag);

    const std::uint8_t received_flag = server.decode(header, encoded_data);
    AW_CHECK(received_flag == flag);
    AW_CHECK(encoded_data == data);
    AW_CHECK(decode_header(header).type == TransactionType::ChatMessage);
    AW_CHECK(decode_header(header).id == 42);
  }
}

AW_TEST_CASE("encrypted transactions round-trip in the reverse direction") {
  const std::vector<std::byte> session_key = bytes_from_hex("01 02 03 04 05 06 07 08");
  const std::vector<std::byte> password = bytes_from_ascii("secret");
  const std::vector<std::byte> data = bytes_from_ascii("server reply");

  TransactionCipher<aw::crypto::Sha1> client(password, session_key, true);
  TransactionCipher<aw::crypto::Sha1> server(password, session_key, false);

  auto header = encode_header_bytes(make_header(7, TransactionType::ServerMessage, 9));
  std::vector<std::byte> encoded = data;
  server.encode(header, encoded, 7);

  const std::uint8_t flag = client.decode(header, encoded);
  AW_CHECK(flag == 7);
  AW_CHECK(encoded == data);
}

AW_TEST_CASE("OFB stream state persists across consecutive transactions") {
  const std::vector<std::byte> session_key = bytes_from_hex("01 02 03 04 05 06 07 08");
  const std::vector<std::byte> password = bytes_from_ascii("secret");

  TransactionCipher<aw::crypto::Sha1> client(password, session_key, true);
  TransactionCipher<aw::crypto::Sha1> server(password, session_key, false);

  for (std::uint32_t id = 1; id <= 4; ++id) {
    auto header = encode_header_bytes(make_header(0, TransactionType::Login, id));
    const std::string text = std::format("packet-{}", id);
    std::vector<std::byte> data = bytes_from_ascii(text);

    client.encode(header, data, 0);
    const std::uint8_t flag = server.decode(header, data);
    AW_CHECK(flag == 0);
    AW_CHECK(decode_header(header).id == id);
    AW_CHECK(data == bytes_from_ascii(text));
  }
}

AW_TEST_CASE("flag != 0 with a one-byte payload decodes correctly (legacy-bug fix)") {
  // Legacy: with flag != 0 and a 1-byte payload the receiver decoded
  // nothing (s >= 2 guard) and left the byte encrypted. The modern codec
  // decodes the single byte under the old key — a deliberate hardening
  // divergence (documented in the modernization ledger).
  const std::vector<std::byte> session_key = bytes_from_hex("01 02 03 04 05 06 07 08");
  const std::vector<std::byte> password = bytes_from_ascii("secret");

  TransactionCipher<aw::crypto::Sha1> client(password, session_key, true);
  TransactionCipher<aw::crypto::Sha1> server(password, session_key, false);

  auto header = encode_header_bytes(make_header(3, TransactionType::Agreed, 5));
  std::vector<std::byte> data = bytes_from_hex("aa");
  client.encode(header, data, 3);

  const std::uint8_t flag = server.decode(header, data);
  AW_CHECK(flag == 3);
  AW_CHECK(data == bytes_from_hex("aa"));
}

AW_TEST_CASE("ciphertext is actually encrypted") {
  const std::vector<std::byte> session_key = bytes_from_hex("01 02 03 04 05 06 07 08");
  const std::vector<std::byte> password = bytes_from_ascii("secret");

  TransactionCipher<aw::crypto::Sha1> client(password, session_key, true);

  auto header = encode_header_bytes(make_header(0, TransactionType::ChatSend, 1));
  std::vector<std::byte> data = bytes_from_ascii("hello world");
  const std::vector<std::byte> plain = data;
  client.encode(header, data, 0);
  AW_CHECK(data != plain);

  // The decoded header no longer equals the plaintext encoding.
  AW_CHECK(decode_header(header).type != TransactionType::ChatSend);
}
