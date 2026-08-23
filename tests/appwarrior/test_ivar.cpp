#include "appwarrior/core/ivar_array.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "appwarrior/testing.h"

using namespace aw::ivar;
using namespace aw::test;

// Hand-built golden: 3 items with IDs 100/101/102 and offsets 0/2/5 (+
// sentinel 7) over the data "hello!!".
AW_TEST_CASE("golden: three-item IVA1 array") {
  const std::vector<std::byte> bytes = bytes_from_hex(
      "49 56 41 31 00 00 00 00 00 00 00 00 00 00 00 03"
      " 00 00 00 64 00 00 00 00"
      " 00 00 00 65 00 00 00 02"
      " 00 00 00 66 00 00 00 05"
      " 00 00 00 00 00 00 00 07"
      " 68 65 6c 6c 6f 21 21");

  const auto decoded = decode(bytes);
  AW_CHECK(decoded.has_value());
  AW_CHECK(decoded->text_encoding == 0U);
  AW_CHECK(decoded->items.size() == 3U);

  AW_CHECK(decoded->items[0].id == 100U);
  AW_REQUIRE_BYTES_MSG(decoded->items[0].data, "68 65", "item 100");
  AW_CHECK(decoded->items[1].id == 101U);
  AW_REQUIRE_BYTES_MSG(decoded->items[1].data, "6c 6c 6f", "item 101");
  AW_CHECK(decoded->items[2].id == 102U);
  AW_REQUIRE_BYTES_MSG(decoded->items[2].data, "21 21", "item 102");

  AW_REQUIRE_BYTES_MSG(item_data(*decoded, 100), "68 65", "lookup 100");
  AW_CHECK(item_data(*decoded, 999).empty());
  AW_CHECK(find(*decoded, 999) == nullptr);
}

AW_TEST_CASE("golden: empty IVA1 array") {
  const auto decoded = decode(bytes_from_hex(
      "49 56 41 31 00 00 00 00 00 00 00 00 00 00 00 00"
      " 00 00 00 00 00 00 00 00"));
  AW_CHECK(decoded.has_value());
  AW_CHECK(decoded->items.empty());
}

// Real shipped asset: legacy/AppWarrior/Error Msgs/UMemory(3).dat (179
// bytes) — the memory error catalog.
AW_TEST_CASE("golden: shipped UMemory(3).dat catalog") {
  const std::vector<std::byte> bytes = bytes_from_hex(
      "49 56 41 31 00 00 00 00 00 00 00 00 00 00 00 03"
      " 00 00 00 64 00 00 00 00 00 00 00 65 00 00 00 2c"
      " 00 00 00 66 00 00 00 69 00 00 00 00 00 00 00 83"
      " 41 6e 20 75 6e 6b 6e 6f 77 6e 20 6d 65 6d 6f 72"
      " 79 20 72 65 6c 61 74 65 64 20 65 72 72 6f 72 20"
      " 68 61 73 20 6f 63 63 75 72 65 64 2e 4e 6f 74 20"
      " 65 6e 6f 75 67 68 20 6d 65 6d 6f 72 79 2e 20 20"
      " 54 72 79 20 63 6c 6f 73 69 6e 67 20 77 69 6e 64"
      " 6f 77 73 20 61 6e 64 20 73 61 76 69 6e 67 20 64"
      " 6f 63 75 6d 65 6e 74 73 2e 4d 65 6d 6f 72 79 20"
      " 62 6c 6f 63 6b 20 69 73 20 6e 6f 74 20 76 61 6c"
      " 69 64 2e");

  const auto decoded = decode(bytes);
  AW_CHECK(decoded.has_value());
  AW_CHECK(decoded->items.size() == 3U);
  AW_CHECK(decoded->items[0].id == 100U);
  AW_CHECK(decoded->items[0].data.size() == 44U);
  AW_CHECK(decoded->items[1].id == 101U);
  AW_CHECK(decoded->items[1].data.size() == 61U);
  AW_CHECK(decoded->items[2].id == 102U);
  AW_CHECK(decoded->items[2].data.size() == 26U);
  AW_REQUIRE_BYTES_MSG(std::span<const std::byte>(decoded->items[0].data).first<8>(),
                    "41 6e 20 75 6e 6b 6e 6f", "first string prefix");
}

// Real shipped asset: legacy/AppWarrior/Error Msgs/UError(1).dat (529
// bytes). Its offset table is ..., 9, 13, 11, 12, 13 — duplicate ID 13, out
// of order — so it violates the documented sorted-ID invariant and the
// legacy Unflatten would reject it. The decoder preserves table order and
// first-match lookup (documented leniency).
AW_TEST_CASE("golden: shipped UError(1).dat catalog with duplicate ID 13") {
  const std::vector<std::byte> bytes = bytes_from_hex(
      "49 56 41 31 00 00 00 00 00 00 00 00 00 00 00 0d"
      " 00 00 00 01 00 00 00 00 00 00 00 02 00 00 00 1d"
      " 00 00 00 03 00 00 00 4a 00 00 00 04 00 00 00 79"
      " 00 00 00 05 00 00 00 94 00 00 00 06 00 00 00 a8"
      " 00 00 00 07 00 00 00 be 00 00 00 08 00 00 00 dd"
      " 00 00 00 09 00 00 00 fb 00 00 00 0d 00 00 01 0b"
      " 00 00 00 0b 00 00 01 2d 00 00 00 0c 00 00 01 43"
      " 00 00 00 0d 00 00 01 61 00 00 00 00 00 00 01 91"
      " 41 6e 20 75 6e 6b 6e 6f 77 6e 20 65 72 72 6f 72"
      " 20 68 61 73 20 6f 63 63 75 72 65 64 2e 41 6e 20"
      " 75 6e 6b 6e 6f 77 6e 20 65 72 72 6f 72 20 68 61"
      " 73 20 6f 63 63 75 72 65 64 20 28 69 6e 76 61 6c"
      " 69 64 20 76 61 6c 75 65 29 2e 41 6e 20 75 6e 6b"
      " 6e 6f 77 6e 20 65 72 72 6f 72 20 68 61 73 20 6f"
      " 63 63 75 72 65 64 20 28 70 72 6f 74 6f 63 6f 6c"
      " 20 62 72 65 61 63 68 29 2e 4f 70 65 72 61 74 69"
      " 6f 6e 20 69 73 20 75 6e 69 6d 70 6c 65 6d 65 6e"
      " 74 65 64 2e 4f 70 65 72 61 74 69 6f 6e 20 74 69"
      " 6d 65 64 20 6f 75 74 2e 4f 70 65 72 61 74 69 6f"
      " 6e 20 77 61 73 20 61 62 6f 72 74 65 64 2e 44 61"
      " 74 61 20 76 65 72 73 69 6f 6e 20 69 73 20 6e 6f"
      " 74 20 63 6f 6d 70 61 74 69 62 6c 65 2e 44 61 74"
      " 61 20 66 6f 72 6d 61 74 20 69 73 20 6e 6f 74 20"
      " 63 6f 6d 70 61 74 69 62 6c 65 2e 44 61 74 61 20"
      " 69 73 20 63 6f 72 72 75 70 74 2e 49 6e 74 65 72"
      " 6e 61 6c 20 6c 69 6d 69 74 20 63 61 6e 6e 6f 74"
      " 20 62 65 20 65 78 63 65 65 64 65 64 2e 56 61 6c"
      " 75 65 20 69 73 20 6f 75 74 20 6f 66 20 72 61 6e"
      " 67 65 2e 53 70 65 63 69 66 69 65 64 20 69 74 65"
      " 6d 20 64 6f 65 73 20 6e 6f 74 20 65 78 69 73 74"
      " 2e 41 6e 20 69 74 65 6d 20 77 69 74 68 20 74 68"
      " 65 20 73 61 6d 65 20 69 64 65 6e 74 69 66 69 65"
      " 72 20 61 6c 72 65 61 64 79 20 65 78 69 73 74 73"
      " 2e");

  const auto decoded = decode(bytes);
  AW_CHECK(decoded.has_value());
  AW_CHECK(decoded->items.size() == 13U);

  const std::vector<std::uint32_t> expected_ids{1, 2, 3, 4, 5, 6, 7, 8, 9, 13, 11, 12, 13};
  for (std::size_t i = 0; i < expected_ids.size(); ++i) {
    AW_CHECK(decoded->items[i].id == expected_ids[i]);
  }

  // First match wins for the duplicated ID 13 (table order preserved).
  const Item* first_thirteen = find(*decoded, 13);
  AW_CHECK(first_thirteen == &decoded->items[9]);
  AW_CHECK(item_data(*decoded, 13).size() == 34U);
  AW_CHECK(item_data(*decoded, 11).size() == 22U);

  // Item 1 carries the full "An unknown error has occured." string.
  AW_REQUIRE_BYTES_MSG(decoded->items[0].data,
                    "41 6e 20 75 6e 6b 6e 6f 77 6e 20 65 72 72 6f 72 20 68 61 73 20 6f 63 63"
                    " 75 72 65 64 2e",
                    "item 1 text");
}

AW_TEST_CASE("unsorted and duplicate IDs are accepted (documented leniency)") {
  const auto decoded = decode(bytes_from_hex(
      "49 56 41 31 00 00 00 00 00 00 00 00 00 00 00 03"
      " 00 00 00 64 00 00 00 00"
      " 00 00 00 67 00 00 00 02"
      " 00 00 00 65 00 00 00 04"
      " 00 00 00 00 00 00 00 07"
      " 61 62 63 64 65 66 67"));
  AW_CHECK(decoded.has_value());
  AW_CHECK(decoded->items.size() == 3U);
  AW_CHECK(decoded->items[0].id == 100U);
  AW_CHECK(decoded->items[1].id == 103U);
  AW_CHECK(decoded->items[2].id == 101U);
  AW_REQUIRE_BYTES_MSG(item_data(*decoded, 101), "65 66 67", "out-of-order id lookup");
}

AW_TEST_CASE("wrong format tag is rejected") {
  const auto decoded = decode(bytes_from_hex(
      "48 4c 4e 5a 00 00 00 00 00 00 00 00 00 00 00 00"
      " 00 00 00 00 00 00 00 00"));
  AW_CHECK(!decoded.has_value());
  AW_CHECK(decoded.error() == DecodeError::wrong_format_tag);
}

AW_TEST_CASE("every truncation prefix of a valid array is rejected") {
  const std::vector<std::byte> full = bytes_from_hex(
      "49 56 41 31 00 00 00 00 00 00 00 00 00 00 00 03"
      " 00 00 00 64 00 00 00 00 00 00 00 65 00 00 00 02"
      " 00 00 00 66 00 00 00 05 00 00 00 00 00 00 00 07"
      " 68 65 6c 6c 6f 21 21");
  AW_CHECK(full.size() == 55U);

  for (std::size_t prefix = 0; prefix < full.size(); ++prefix) {
    const auto decoded = decode(std::span<const std::byte>(full).first(prefix));
    AW_CHECK(!decoded.has_value());
    if (prefix < 48U) {
      // Below the header (24 bytes) or a partial offset table.
      AW_CHECK(decoded.error() == DecodeError::truncated);
    } else {
      // Header + table present but the data area is too small for the
      // sentinel offset (7).
      AW_CHECK(decoded.error() == DecodeError::offset_out_of_range);
    }
  }

  AW_CHECK(decode(std::span<const std::byte>(full)).has_value());
}

AW_TEST_CASE("the legacy impossible-item-count guard is preserved") {
  const auto decoded = decode(bytes_from_hex(
      "49 56 41 31 00 00 00 00 00 00 00 00 ff 00 00 01"
      " 00 00 00 00 00 00 00 00"));
  AW_CHECK(!decoded.has_value());
  AW_CHECK(decoded.error() == DecodeError::impossible_item_count);
}

AW_TEST_CASE("declared table larger than the buffer is rejected") {
  const auto decoded = decode(bytes_from_hex(
      "49 56 41 31 00 00 00 00 00 00 00 00 00 00 00 03"
      " 00 00 00 64 00 00 00 00"));
  AW_CHECK(!decoded.has_value());
  AW_CHECK(decoded.error() == DecodeError::truncated);
}

AW_TEST_CASE("decreasing offsets are rejected") {
  const auto decoded = decode(bytes_from_hex(
      "49 56 41 31 00 00 00 00 00 00 00 00 00 00 00 02"
      " 00 00 00 64 00 00 00 05"
      " 00 00 00 65 00 00 00 03"
      " 00 00 00 00 00 00 00 05"
      " 68 65 6c 6c 6f"));
  AW_CHECK(!decoded.has_value());
  AW_CHECK(decoded.error() == DecodeError::offset_out_of_range);
}

AW_TEST_CASE("offsets beyond the data are rejected") {
  const auto decoded = decode(bytes_from_hex(
      "49 56 41 31 00 00 00 00 00 00 00 00 00 00 00 02"
      " 00 00 00 64 00 00 00 00"
      " 00 00 00 65 00 00 00 09"
      " 00 00 00 00 00 00 00 09"
      " 61 62 63"));
  AW_CHECK(!decoded.has_value());
  AW_CHECK(decoded.error() == DecodeError::offset_out_of_range);
}

AW_TEST_CASE("sentinel offset beyond the data is rejected") {
  const auto decoded = decode(bytes_from_hex(
      "49 56 41 31 00 00 00 00 00 00 00 00 00 00 00 02"
      " 00 00 00 64 00 00 00 00"
      " 00 00 00 65 00 00 00 03"
      " 00 00 00 00 00 00 00 05"
      " 61 62 63"));
  AW_CHECK(!decoded.has_value());
  AW_CHECK(decoded.error() == DecodeError::offset_out_of_range);
}

AW_TEST_CASE("slack bytes beyond the sentinel offset are tolerated (legacy behavior)") {
  const auto decoded = decode(bytes_from_hex(
      "49 56 41 31 00 00 00 00 00 00 00 00 00 00 00 01"
      " 00 00 00 64 00 00 00 00"
      " 00 00 00 00 00 00 00 02"
      " 61 62 63"));
  AW_CHECK(decoded.has_value());
  AW_CHECK(decoded->items.size() == 1U);
  AW_CHECK(decoded->items[0].id == 100U);
  AW_REQUIRE_BYTES_MSG(decoded->items[0].data, "61 62", "item ends at sentinel offset");
}
