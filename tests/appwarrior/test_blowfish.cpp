#include "appwarrior/crypto/blowfish.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "appwarrior/core/endian.h"
#include "appwarrior/testing.h"
#include "blowfish_vectors.h"

using namespace aw::crypto;
using namespace aw::crypto::test_vectors;
using namespace aw::test;

namespace {

auto bytes8(const std::array<std::uint8_t, 8>& values) -> std::array<std::byte, 8> {
  std::array<std::byte, 8> out{};
  for (std::size_t i = 0; i < 8; ++i) {
    out[i] = static_cast<std::byte>(values[i]);
  }
  return out;
}

auto to_words(std::span<const std::byte, 8> bytes) -> std::pair<std::uint32_t, std::uint32_t> {
  return {aw::endian::read_u32be(bytes.first<4>()), aw::endian::read_u32be(bytes.subspan<4>().first<4>())};
}

}  // namespace

AW_TEST_CASE("Blowfish: Eric Young variable-key ECB vectors") {
  Blowfish cipher;
  for (std::size_t i = 0; i < kVariableKeyTests; ++i) {
    const auto key = bytes8(kVariableKey[i]);
    cipher.set_encode_key(key);
    cipher.set_decode_key(key);

    std::array<std::byte, 8> block{};
    aw::endian::write_u32be(kPlaintextLeft[i], std::span<std::byte, 8>(block).first<4>());
    aw::endian::write_u32be(kPlaintextRight[i], std::span<std::byte, 8>(block).subspan<4>().first<4>());

    const auto encrypted = cipher.encrypt_block(block);
    const auto [cl, cr] = to_words(encrypted);
    AW_CHECK(cl == kCiphertextLeft[i]);
    AW_CHECK(cr == kCiphertextRight[i]);

    const auto decrypted = cipher.decrypt_block(encrypted);
    AW_CHECK(decrypted == block);
  }
}

AW_TEST_CASE("Blowfish: Eric Young variable-length-key ECB vectors") {
  Blowfish cipher;
  for (std::size_t i = 0; i < kSetKeyTests; ++i) {
    const std::size_t key_length = i + 1;
    std::vector<std::byte> key;
    key.reserve(key_length);
    for (std::size_t j = 0; j < key_length; ++j) {
      key.push_back(static_cast<std::byte>(kSetKey[j]));
    }
    cipher.set_encode_key(key);
    cipher.set_decode_key(key);

    std::array<std::byte, 8> block{};
    aw::endian::write_u32be(kPlaintextLeft[kVariableKeyTests + i], std::span<std::byte, 8>(block).first<4>());
    aw::endian::write_u32be(kPlaintextRight[kVariableKeyTests + i],
                            std::span<std::byte, 8>(block).subspan<4>().first<4>());

    const auto encrypted = cipher.encrypt_block(block);
    const auto [cl, cr] = to_words(encrypted);
    AW_CHECK(cl == kCiphertextLeft[kVariableKeyTests + i]);
    AW_CHECK(cr == kCiphertextRight[kVariableKeyTests + i]);
  }
}

AW_TEST_CASE("Blowfish OFB-64: zero-IV keystream golden (independent OpenSSL oracle)") {
  {
    Blowfish cipher;
    cipher.set_encode_key(bytes8({0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}));
    Blowfish::Ofb64 stream;
    std::vector<std::byte> buffer(16, std::byte{0});
    cipher.encode_stream(stream, buffer);
    AW_REQUIRE_BYTES(buffer, "4e f9 97 45 61 98 dd 78 e1 c0 30 e7 4c 14 d2 61");
  }
}

AW_TEST_CASE("Blowfish OFB-64: keystream is the ECB feedback chain") {
  // For key 0x0123456789ABCDEF: block 1 = ECB(0) = 245946885754369a — the
  // Eric Young variable-key vector #33 (proven by the ECB suite above);
  // block 2 = ECB(block 1). Note: OpenSSL 3's legacy-provider Blowfish
  // deviates from the official test vectors (its `enc -bf-ecb` fails
  // vectors #2/#33/#34), so it is not used as an oracle here — only the
  // key-0 16-byte keystream above agreed with it and is kept.
  Blowfish cipher;
  cipher.set_encode_key(bytes8({0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF}));

  Blowfish::Ofb64 stream;
  std::vector<std::byte> buffer(16, std::byte{0});
  cipher.encode_stream(stream, buffer);

  const auto zero = bytes8({0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
  const auto block1 = cipher.encrypt_block(zero);
  AW_REQUIRE_BYTES_MSG(block1, "24 59 46 88 57 54 36 9a", "Eric Young vector #33");
  AW_CHECK(std::ranges::equal(std::span<const std::byte>(buffer).first<8>(), block1));

  const auto block2 = cipher.encrypt_block(block1);
  AW_CHECK(std::ranges::equal(std::span<const std::byte>(buffer).subspan(8), block2));
}

AW_TEST_CASE("Blowfish OFB-64: stream state persists across calls and reset restarts") {
  Blowfish cipher;
  cipher.set_encode_key(bytes8({0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}));

  std::vector<std::byte> one_shot(16, std::byte{0});
  Blowfish::Ofb64 one_stream;
  cipher.encode_stream(one_stream, one_shot);

  std::vector<std::byte> chunked(16, std::byte{0});
  Blowfish::Ofb64 chunked_stream;
  cipher.encode_stream(chunked_stream, std::span<std::byte>(chunked).first(5));
  cipher.encode_stream(chunked_stream, std::span<std::byte>(chunked).subspan(5, 3));
  cipher.encode_stream(chunked_stream, std::span<std::byte>(chunked).subspan(8, 8));
  AW_CHECK(chunked == one_shot);

  chunked_stream.reset();
  std::vector<std::byte> restarted(16, std::byte{0});
  cipher.encode_stream(chunked_stream, restarted);
  AW_CHECK(restarted == one_shot);
}

AW_TEST_CASE("Blowfish OFB-64: XOR round-trip with non-zero data") {
  Blowfish cipher;
  cipher.set_encode_key(bytes8({0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF}));
  cipher.set_decode_key(bytes8({0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF}));

  std::vector<std::byte> plain = bytes_from_ascii("The quick brown fox!");
  std::vector<std::byte> encoded = plain;
  Blowfish::Ofb64 encode_stream;
  cipher.encode_stream(encode_stream, encoded);

  // OFB decryption applies the same keystream under the decode schedule
  // (set to the same key here).
  std::vector<std::byte> decoded = encoded;
  Blowfish::Ofb64 decode_stream;
  cipher.decode_stream(decode_stream, decoded);
  AW_CHECK(decoded == plain);
  AW_CHECK(encoded != plain);
}
