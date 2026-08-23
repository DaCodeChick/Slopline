#include "hotline/protocol/field_list.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

#include "appwarrior/testing.h"

using namespace hotline::protocol;
using namespace aw::test;

// Golden vector (audit/06 §11.1): one field — id 100 (ErrorText), data "x".
AW_TEST_CASE("golden: single-field body") {
  const std::vector<std::byte> bytes = bytes_from_hex("00 01 00 64 00 01 78");

  const auto decoded = decode_field_list(bytes);
  AW_CHECK(decoded.has_value());
  AW_CHECK(decoded->fields.size() == 1U);
  AW_CHECK(decoded->fields[0].id == FieldId::ErrorText);
  AW_REQUIRE_BYTES_MSG(decoded->fields[0].data, "78", "single field data");

  AW_REQUIRE_BYTES_MSG(unwrap(encode_field_list(*decoded)), "00 01 00 64 00 01 78",
                    "single-field re-encode");
}

AW_TEST_CASE("multi-field list with duplicate IDs 112 round-trips verbatim") {
  FieldList list;
  list.fields.push_back(Field{FieldId::UserFlags, bytes_from_hex("aa")});  // id 112
  list.fields.push_back(Field{FieldId::Options, bytes_from_hex("bb cc")});  // id 113
  list.fields.push_back(Field{FieldId::Visible, bytes_from_hex("dd")});     // id 112 again

  const std::vector<std::byte> encoded = unwrap(encode_field_list(list));
  AW_REQUIRE_BYTES_MSG(encoded, "00 03 00 70 00 01 aa 00 71 00 02 bb cc 00 70 00 01 dd",
                    "duplicate-ID encode");

  const auto decoded = decode_field_list(encoded);
  AW_CHECK(decoded.has_value());
  AW_CHECK(decoded->fields.size() == 3U);
  AW_CHECK(decoded->fields[0].id == FieldId::UserFlags);
  AW_CHECK(decoded->fields[1].id == FieldId::Options);
  AW_CHECK(decoded->fields[2].id == FieldId::Visible);
}

AW_TEST_CASE("lookup follows legacy first-match semantics for duplicate IDs") {
  FieldList list;
  list.fields.push_back(Field{FieldId::Visible, bytes_from_hex("aa")});
  list.fields.push_back(Field{FieldId::Options, bytes_from_hex("bb")});
  list.fields.push_back(Field{FieldId::UserFlags, bytes_from_hex("cc")});  // id 112 again

  const Field* first = find_field(list, FieldId::UserFlags);
  AW_CHECK(first != nullptr);
  AW_REQUIRE_BYTES_MSG(first->data, "aa", "first match wins");
  AW_REQUIRE_BYTES_MSG(field_data(list, FieldId::Visible), "aa", "same ID, same first match");
  AW_REQUIRE_BYTES_MSG(field_data(list, FieldId::Options), "bb", "distinct ID");
  AW_CHECK(field_data(list, FieldId::ServerBanner).empty());
  AW_CHECK(find_field(list, FieldId::ServerBanner) == nullptr);
}

AW_TEST_CASE("empty list encodes as a zero count") {
  const FieldList empty;
  AW_REQUIRE_BYTES_MSG(unwrap(encode_field_list(empty)), "00 00", "empty list");

  const auto decoded = decode_field_list(bytes_from_hex("00 00"));
  AW_CHECK(decoded.has_value());
  AW_CHECK(decoded->fields.empty());
}

AW_TEST_CASE("zero-size fields are legal on the wire") {
  FieldList list;
  list.fields.push_back(Field{FieldId::Data, {}});
  const std::vector<std::byte> encoded = unwrap(encode_field_list(list));
  AW_REQUIRE_BYTES_MSG(encoded, "00 01 00 65 00 00", "zero-size field");

  const auto decoded = decode_field_list(encoded);
  AW_CHECK(decoded.has_value());
  AW_CHECK(decoded->fields.size() == 1U);
  AW_CHECK(decoded->fields[0].data.empty());
}

AW_TEST_CASE("every truncation prefix of a valid list is rejected") {
  const std::vector<std::byte> full = bytes_from_hex("00 01 00 64 00 01 78");
  for (std::size_t prefix = 0; prefix < full.size(); ++prefix) {
    const auto decoded = decode_field_list(std::span<const std::byte>(full).first(prefix));
    AW_CHECK(!decoded.has_value());
    AW_CHECK(decoded.error() == DecodeError::truncated);
  }
}

AW_TEST_CASE("declared size longer than the available bytes is rejected") {
  const auto decoded = decode_field_list(bytes_from_hex("00 01 00 64 00 05 78"));
  AW_CHECK(!decoded.has_value());
  AW_CHECK(decoded.error() == DecodeError::truncated);
}

AW_TEST_CASE("declared count longer than the available entries is rejected") {
  const auto decoded = decode_field_list(bytes_from_hex("00 02 00 64 00 01 78"));
  AW_CHECK(!decoded.has_value());
  AW_CHECK(decoded.error() == DecodeError::truncated);
}

AW_TEST_CASE("trailing bytes after the last field are rejected") {
  const auto decoded = decode_field_list(bytes_from_hex("00 01 00 64 00 01 78 00"));
  AW_CHECK(!decoded.has_value());
  AW_CHECK(decoded.error() == DecodeError::trailing_bytes);
}

// --- integer fields (UFieldData::AddInteger / GetInteger semantics) ------

AW_TEST_CASE("integer encode: 2 bytes for 0..65535, 4 bytes otherwise") {
  AW_REQUIRE_BYTES_MSG(make_integer_field(FieldId::Vers, 0).data, "00 00", "0");
  AW_REQUIRE_BYTES_MSG(make_integer_field(FieldId::Vers, 197).data, "00 c5", "197");
  AW_REQUIRE_BYTES_MSG(make_integer_field(FieldId::Vers, 32767).data, "7f ff", "32767");
  AW_REQUIRE_BYTES_MSG(make_integer_field(FieldId::Vers, 32768).data, "80 00", "32768 (2 bytes!)");
  AW_REQUIRE_BYTES_MSG(make_integer_field(FieldId::Vers, 65535).data, "ff ff", "65535 (2 bytes!)");
  AW_REQUIRE_BYTES_MSG(make_integer_field(FieldId::Vers, 65536).data, "00 01 00 00", "65536");
  AW_REQUIRE_BYTES_MSG(make_integer_field(FieldId::Vers, -1).data, "ff ff ff ff", "-1");
  AW_REQUIRE_BYTES_MSG(make_integer_field(FieldId::Vers, -65536).data, "ff ff 00 00", "-65536");
}

AW_TEST_CASE("integer decode accepts 1-, 2- and 4-byte forms") {
  const auto one = decode_integer_field(Field{FieldId::Vers, bytes_from_hex("2a")});
  AW_CHECK(one.has_value() && *one == 42);

  const auto two = decode_integer_field(Field{FieldId::Vers, bytes_from_hex("00 c5")});
  AW_CHECK(two.has_value() && *two == 197);

  const auto two_unsigned = decode_integer_field(Field{FieldId::Vers, bytes_from_hex("ff ff")});
  AW_CHECK(two_unsigned.has_value() && *two_unsigned == 65535);

  const auto four = decode_integer_field(Field{FieldId::Vers, bytes_from_hex("00 01 00 00")});
  AW_CHECK(four.has_value() && *four == 65536);

  const auto four_negative =
      decode_integer_field(Field{FieldId::Vers, bytes_from_hex("ff ff ff ff")});
  AW_CHECK(four_negative.has_value() && *four_negative == -1);
}

AW_TEST_CASE("integer decode rejects unsupported sizes explicitly") {
  const auto three = decode_integer_field(Field{FieldId::Vers, bytes_from_hex("01 02 03")});
  AW_CHECK(!three.has_value());
  AW_CHECK(three.error() == DecodeError::invalid_integer_field_size);

  const auto five = decode_integer_field(Field{FieldId::Vers, bytes_from_hex("01 02 03 04 05")});
  AW_CHECK(!five.has_value());
  AW_CHECK(five.error() == DecodeError::invalid_integer_field_size);

  const auto zero = decode_integer_field(Field{FieldId::Vers, {}});
  AW_CHECK(!zero.has_value());
  AW_CHECK(zero.error() == DecodeError::invalid_integer_field_size);
}

// --- text fields (raw bytes, no terminator) ------------------------------

AW_TEST_CASE("string fields carry raw bytes without a terminator") {
  const Field field = make_string_field(FieldId::ChatSubject, "hello");
  AW_REQUIRE_BYTES_MSG(field.data, "68 65 6c 6c 6f", "cstring wire form");

  AW_CHECK(decode_string_field(field) == "hello");
  AW_CHECK(make_string_field(FieldId::ChatSubject, "").data.empty());
}

// --- encode-side limits ---------------------------------------------------

AW_TEST_CASE("encode rejects field data larger than 65535 bytes") {
  FieldList list;
  list.fields.push_back(Field{FieldId::Data, std::vector<std::byte>(65536)});
  const auto result = encode_field_list(list);
  AW_CHECK(!result.has_value());
  AW_CHECK(result.error() == EncodeError::element_too_large);
}

AW_TEST_CASE("encode rejects more than 65535 fields") {
  FieldList list;
  list.fields.reserve(65536);
  for (std::size_t i = 0; i <= kMaxFieldCount; ++i) {
    list.fields.push_back(Field{FieldId::Data, {}});
  }
  const auto result = encode_field_list(list);
  AW_CHECK(!result.has_value());
  AW_CHECK(result.error() == EncodeError::count_too_large);
}
