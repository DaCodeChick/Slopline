#include "hotline/crypto/sha1.h"

#include <cstddef>
#include <span>
#include <vector>

#include "appwarrior/testing.h"

using namespace hotline::crypto;
using namespace appwarrior::test;

AW_TEST_CASE("SHA-1: FIPS 180-1 test vectors") {
  AW_REQUIRE_BYTES(Sha1::digest({}), "da 39 a3 ee 5e 6b 4b 0d 32 55 bf ef 95 60 18 90 af d8 07 09");

  const std::vector<std::byte> abc = bytes_from_ascii("abc");
  AW_REQUIRE_BYTES(Sha1::digest(abc),
                "a9 99 3e 36 47 06 81 6a ba 3e 25 71 78 50 c2 6c 9c d0 d8 9d");

  const std::vector<std::byte> fox = bytes_from_ascii("The quick brown fox jumps over the lazy dog");
  AW_REQUIRE_BYTES(Sha1::digest(fox),
                "2f d4 e1 c6 7a 2d 28 fc ed 84 9e e1 bb 76 e7 39 1b 93 eb 12");
}

AW_TEST_CASE("SHA-1: block-boundary lengths (cross-checked vs python)") {
  const std::vector<std::byte> fifty_six = bytes_from_ascii(
      "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVW");
  AW_REQUIRE_BYTES(Sha1::digest(fifty_six),
                "2f 29 fa e9 8e 19 70 94 69 3d fd 1b f8 37 91 d6 11 27 99 67");

  const std::vector<std::byte> sixty_four = bytes_from_ascii(
      "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ01");
  AW_REQUIRE_BYTES(Sha1::digest(sixty_four),
                "83 b1 3a d6 3e 31 a3 fe 3f d8 9f 5d 7d ec ae 0c 74 c1 1a 66");
}
