#include "hotline/crypto/md5.h"

#include <cstddef>
#include <span>
#include <vector>

#include "appwarrior/testing.h"

using namespace hotline::crypto;
using namespace appwarrior::test;

AW_TEST_CASE("MD5: RFC 1321 test vectors") {
  AW_REQUIRE_BYTES(Md5::digest({}), "d4 1d 8c d9 8f 00 b2 04 e9 80 09 98 ec f8 42 7e");

  const std::vector<std::byte> abc = bytes_from_ascii("abc");
  AW_REQUIRE_BYTES(Md5::digest(abc), "90 01 50 98 3c d2 4f b0 d6 96 3f 7d 28 e1 7f 72");

  const std::vector<std::byte> fox = bytes_from_ascii("The quick brown fox jumps over the lazy dog");
  AW_REQUIRE_BYTES(Md5::digest(fox), "9e 10 7d 9d 37 2b b6 82 6b d8 1d 35 42 a4 19 d6");
}

AW_TEST_CASE("MD5: block-boundary lengths") {
  // 55 and 56 bytes straddle the padding decision (n > 56 pushes a second
  // block); cross-checked against python hashlib.
  const std::vector<std::byte> fifty_five = bytes_from_ascii(
      "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUV");
  AW_REQUIRE_BYTES(Md5::digest(fifty_five), "b5 c2 f1 ac 16 6f ef fa 0a 6d 2b 93 b0 55 12 81");

  const std::vector<std::byte> fifty_six = bytes_from_ascii(
      "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVW");
  AW_REQUIRE_BYTES(Md5::digest(fifty_six), "10 4c 4f 9e ec de 08 e0 33 7b 88 01 bb de d8 16");
}
