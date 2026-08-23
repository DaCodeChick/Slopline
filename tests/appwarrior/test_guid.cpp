#include "appwarrior/core/guid.h"

#include <array>
#include <cstddef>
#include <cstdint>

#include "appwarrior/testing.h"

using namespace aw::guid;
using namespace aw::test;

AW_TEST_CASE("Guid: golden 16-byte Microsoft UUID network form") {
  Guid guid;
  guid.time_low = 0x11223344U;
  guid.time_mid = 0x5566;
  guid.time_hi_and_version = 0x7788;
  guid.clock_seq_hi_and_reserved = 0x99;
  guid.clock_seq_low = 0xAA;
  guid.node = {1, 2, 3, 4, 5, 6};

  std::array<std::byte, kSize> encoded{};
  encode(guid, encoded);
  AW_REQUIRE_BYTES(encoded, "11 22 33 44 55 66 77 88 99 aa 01 02 03 04 05 06");

  const auto decoded = try_decode(encoded);
  AW_CHECK(decoded.has_value());
  AW_CHECK(*decoded == guid);

  const auto truncated = try_decode(bytes_from_hex("11 22 33 44 55 66"));
  AW_CHECK(!truncated.has_value());
  AW_CHECK(truncated.error() == aw::DecodeError::truncated);
  const auto trailing =
      try_decode(bytes_from_hex("11 22 33 44 55 66 77 88 99 aa 01 02 03 04 05 06 00"));
  AW_CHECK(!trailing.has_value());
  AW_CHECK(trailing.error() == aw::DecodeError::trailing_bytes);
}
