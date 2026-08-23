#include "appwarrior/core/bits.h"

#include <array>
#include <cstddef>
#include <span>

#include "appwarrior/testing.h"

using namespace aw::bits;
using namespace aw::test;

AW_TEST_CASE("MSB-first order: bit 0 is the MSB of byte 0") {
  std::array<std::byte, 1> data{};
  set_bit(data, 0);
  AW_REQUIRE_BYTES(data, "80");
  AW_CHECK(get_bit(data, 0));
  AW_CHECK(!get_bit(data, 1));
}

AW_TEST_CASE("bits map byte-major with 7-(index%8) bit position") {
  std::array<std::byte, 2> data{};
  set_bit(data, 0);   // byte 0, MSB
  set_bit(data, 3);   // byte 0, bit 4
  set_bit(data, 7);   // byte 0, LSB
  set_bit(data, 8);   // byte 1, MSB
  set_bit(data, 15);  // byte 1, LSB
  AW_REQUIRE_BYTES(data, "91 81");
  AW_CHECK(get_bit(data, 0));
  AW_CHECK(get_bit(data, 3));
  AW_CHECK(get_bit(data, 7));
  AW_CHECK(get_bit(data, 8));
  AW_CHECK(get_bit(data, 15));
  AW_CHECK(!get_bit(data, 1));
  AW_CHECK(!get_bit(data, 6));
  AW_CHECK(!get_bit(data, 9));
}

AW_TEST_CASE("clear and invert round-trip") {
  std::array<std::byte, 1> data{std::byte{0xFF}};
  clear_bit(data, 0);
  AW_REQUIRE_BYTES(data, "7f");
  invert_bit(data, 7);
  AW_REQUIRE_BYTES(data, "7e");
  set_bit(data, 0, false);
  AW_CHECK(!get_bit(data, 0));
  set_bit(data, 0, true);
  AW_CHECK(get_bit(data, 0));
}

AW_TEST_CASE("privilege-style indexing matches SMyUserAccess usage") {
  // AccessPrivilege::DownloadFile == 2 must land on byte 0, bit position 5
  // (MSB-first) — the order the legacy SMyUserAccess::SetPriv uses.
  std::array<std::byte, 8> data{};
  set_bit(data, 2);
  AW_REQUIRE_BYTES_MSG(std::span<const std::byte>(data).first<1>(), "20", "priv 2 -> byte0 0x20");
  AW_CHECK(!get_bit(data, 3));
  set_bit(data, 32);  // bit 32 -> byte 4, MSB
  AW_REQUIRE_BYTES_MSG(std::span<const std::byte>(data).subspan(4).first<1>(), "80",
                    "bit 32 -> byte4 0x80");
}
