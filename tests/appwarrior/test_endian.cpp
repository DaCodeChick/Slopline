#include "appwarrior/core/endian.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "appwarrior/testing.h"

using namespace appwarrior::endian;
using namespace appwarrior::test;

AW_TEST_CASE("u16/u32 big-endian golden encodings") {
  std::array<std::byte, 2> w16{};
  write_u16be(static_cast<std::uint16_t>(0x01F4U), w16);  // transaction type 500
  AW_REQUIRE_BYTES(w16, "01 f4");

  write_u16be(static_cast<std::uint16_t>(0x00C5U), w16);  // myField_Vers = 197
  AW_REQUIRE_BYTES(w16, "00 c5");

  std::array<std::byte, 4> w32{};
  write_u32be(0x54525450U, w32);  // 'TRTP'
  AW_REQUIRE_BYTES(w32, "54 52 54 50");

  const std::vector<std::byte> g16 = bytes_from_hex("00 c5");
  AW_CHECK(read_u16be(std::span<const std::byte>(g16).first<2>()) == 197);

  const std::vector<std::byte> g32 = bytes_from_hex("00 00 00 64");
  AW_CHECK(read_u32be(std::span<const std::byte>(g32).first<4>()) == 100U);
}

AW_TEST_CASE("u16 round-trips across boundary values") {
  const std::vector<std::uint16_t> values{0, 1, 0x7FFF, 0x8000, 0xFFFF};
  for (const std::uint16_t value : values) {
    std::array<std::byte, 2> encoded{};
    write_u16be(value, encoded);
    AW_CHECK(read_u16be(encoded) == value);
  }
}

AW_TEST_CASE("u32 round-trips across boundary values") {
  const std::vector<std::uint32_t> values{0, 1, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF};
  for (const std::uint32_t value : values) {
    std::array<std::byte, 4> encoded{};
    write_u32be(value, encoded);
    AW_CHECK(read_u32be(encoded) == value);
  }
}

AW_TEST_CASE("four_cc encodes a tag as a big-endian u32") {
  AW_CHECK(four_cc('T', 'R', 'T', 'P') == 0x54525450U);
  AW_CHECK(four_cc('H', 'O', 'T', 'L') == 0x484F544CU);
  AW_CHECK(four_cc('N', 'I', 'C', 'K') == 0x4E49434BU);
  AW_CHECK(four_cc('H', 'T', 'X', 'F') == 0x48545846U);

  std::array<std::byte, 4> encoded{};
  write_u32be(four_cc('T', 'R', 'T', 'P'), encoded);
  AW_REQUIRE_BYTES(encoded, "54 52 54 50");
}
