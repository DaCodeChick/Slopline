#include "appwarrior/core/align.h"

#include <cstdint>

#include "appwarrior/testing.h"

using namespace aw::align;
using namespace aw::test;

AW_TEST_CASE("round up to power-of-two alignment") {
  AW_CHECK(up(0U, 8U) == 0U);
  AW_CHECK(up(1U, 8U) == 8U);
  AW_CHECK(up(7U, 8U) == 8U);
  AW_CHECK(up(8U, 8U) == 8U);
  AW_CHECK(up(9U, 8U) == 16U);
  AW_CHECK(up(5001U, 8U) == 5008U);
  AW_CHECK(up(1023U, 64U) == 1024U);
  AW_CHECK(up(0x12345678U, 4096U) == 0x12346000U);
}

AW_TEST_CASE("round down to power-of-two alignment") {
  AW_CHECK(down(0U, 8U) == 0U);
  AW_CHECK(down(7U, 8U) == 0U);
  AW_CHECK(down(8U, 8U) == 8U);
  AW_CHECK(down(9U, 8U) == 8U);
  AW_CHECK(down(5001U, 8U) == 5000U);
  AW_CHECK(down(1023U, 64U) == 960U);
}

AW_TEST_CASE("alignment rounding works across widths") {
  AW_CHECK(up(static_cast<std::uint16_t>(3), static_cast<std::uint16_t>(4)) ==
        static_cast<std::uint16_t>(4));
  AW_CHECK(up(3ULL, 4ULL) == 4ULL);
  AW_CHECK(down(static_cast<std::uint32_t>(1023), static_cast<std::uint32_t>(64)) ==
        static_cast<std::uint32_t>(960));
}
