#include "appwarrior/crypto/hmac.h"
#include "appwarrior/crypto/md5.h"
#include "appwarrior/crypto/sha1.h"

#include <cstddef>
#include <span>
#include <vector>

#include "appwarrior/testing.h"

using namespace aw::crypto;
using namespace aw::test;

AW_TEST_CASE("HMAC-MD5: RFC 2202 test vectors") {
  const std::vector<std::byte> key_0b(16, std::byte{0x0B});
  const std::vector<std::byte> hi_there = bytes_from_ascii("Hi There");
  AW_REQUIRE_BYTES(hmac<Md5>(key_0b, hi_there), "92 94 72 7a 36 38 bb 1c 13 f4 8e f8 15 8b fc 9d");

  const std::vector<std::byte> jefe = bytes_from_ascii("Jefe");
  const std::vector<std::byte> what = bytes_from_ascii("what do ya want for nothing?");
  AW_REQUIRE_BYTES(hmac<Md5>(jefe, what), "75 0c 78 3e 6a b0 b5 03 ea a8 6e 31 0a 5d b7 38");

  // audit/06 §11.1: HMAC-MD5 with empty key and text.
  AW_REQUIRE_BYTES(hmac<Md5>({}, {}), "74 e6 f7 29 8a 9c 2d 16 89 35 f5 8c 00 1b ad 88");
}

AW_TEST_CASE("HMAC-SHA1: RFC 2202 test vectors") {
  const std::vector<std::byte> key_0b(20, std::byte{0x0B});
  const std::vector<std::byte> hi_there = bytes_from_ascii("Hi There");
  AW_REQUIRE_BYTES(hmac<Sha1>(key_0b, hi_there),
                "b6 17 31 86 55 05 72 64 e2 8b c0 b6 fb 37 8c 8e f1 46 be 00");

  const std::vector<std::byte> jefe = bytes_from_ascii("Jefe");
  const std::vector<std::byte> what = bytes_from_ascii("what do ya want for nothing?");
  AW_REQUIRE_BYTES(hmac<Sha1>(jefe, what),
                "ef fc df 6a e5 eb 2f a2 d2 74 16 d5 f1 84 df 9c 25 9a 7c 79");

  const std::vector<std::byte> key_aa(20, std::byte{0xAA});
  const std::vector<std::byte> dd(50, std::byte{0xDD});
  AW_REQUIRE_BYTES(hmac<Sha1>(key_aa, dd),
                "12 5d 73 42 b9 ac 11 cd 91 a3 9a f4 8a a1 7b 4f 63 f1 75 d3");
}

AW_TEST_CASE("HMAC: keys longer than the block size are hashed first") {
  // Key longer than 64 bytes forces the RFC 2104 pre-hash path
  // (cross-checked against python hmac).
  const std::vector<std::byte> long_key = bytes_from_ascii(
      "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdef");
  const std::vector<std::byte> data = bytes_from_ascii("payload");
  AW_REQUIRE_BYTES(hmac<Md5>(long_key, data), "95 7d 28 c2 22 b3 c2 56 9c 70 87 46 70 b6 55 63");
  AW_REQUIRE_BYTES(hmac<Sha1>(long_key, data),
                "4d b0 2c 24 51 94 90 94 3e 74 f0 0b 99 25 a6 ff 92 2f bd 78");
}

