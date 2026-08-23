#include "hotline/protocol/payload.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "appwarrior/core/endian.h"
#include "appwarrior/testing.h"

using namespace hotline::protocol;
using namespace aw::test;

AW_TEST_CASE("FileInfo: golden encode with the legacy-Intel endianness quirk") {
  FileInfo info;
  info.type = aw::endian::four_cc('T', 'E', 'X', 'T');
  info.creator = aw::endian::four_cc('t', 't', 'x', 't');
  info.file_size = 0x12345678U;
  info.name = "a.txt";

  // type/creator little-endian (raw host copy on legacy Intel builds —
  // 'TEXT' appears byte-reversed); fileSize/nameSize big-endian.
  const std::vector<std::byte> encoded = unwrap(encode_file_info(info));
  AW_REQUIRE_BYTES_MSG(
      encoded,
      "54 58 45 54 74 78 74 74 12 34 56 78 00 00 00 00 00 00 00 05 61 2e 74 78 74",
      "file info wire form");

  const auto decoded = try_decode_file_info(encoded);
  AW_CHECK(decoded.has_value());
  AW_CHECK(decoded->type == info.type);
  AW_CHECK(decoded->creator == info.creator);
  AW_CHECK(decoded->file_size == info.file_size);
  AW_CHECK(decoded->name_script == 0);
  AW_CHECK(decoded->name == "a.txt");
}

AW_TEST_CASE("FileInfo: truncation and trailing bytes are rejected") {
  const std::vector<std::byte> full =
      bytes_from_hex("54 58 45 54 74 78 74 74 12 34 56 78 00 00 00 00 00 00 00 05"
                     " 61 2e 74 78 74");
  for (std::size_t prefix = 0; prefix < full.size(); ++prefix) {
    const auto decoded = try_decode_file_info(std::span<const std::byte>(full).first(prefix));
    AW_CHECK(!decoded.has_value());
    AW_CHECK(decoded.error() == DecodeError::truncated);
  }
  std::vector<std::byte> oversized = full;
  oversized.push_back(std::byte{0});
  const auto trailing = try_decode_file_info(oversized);
  AW_CHECK(!trailing.has_value());
  AW_CHECK(trailing.error() == DecodeError::trailing_bytes);
}

AW_TEST_CASE("UserInfo: golden encode (all multi-byte fields big-endian)") {
  UserInfo info;
  info.id = 0x1234;
  info.icon_id = -2;
  info.flags = 1;
  info.name = "ab";
  AW_REQUIRE_BYTES_MSG(unwrap(encode_user_info(info)), "12 34 ff fe 00 01 00 02 61 62",
                    "user info wire form");

  const auto decoded = try_decode_user_info(bytes_from_hex("12 34 ff fe 00 01 00 02 61 62"));
  AW_CHECK(decoded.has_value());
  AW_CHECK(decoded->id == 0x1234);
  AW_CHECK(decoded->icon_id == -2);
  AW_CHECK(decoded->flags == 1);
  AW_CHECK(decoded->name == "ab");
}

AW_TEST_CASE("UserInfo: truncation and trailing bytes are rejected") {
  const std::vector<std::byte> full = bytes_from_hex("12 34 ff fe 00 01 00 02 61 62");
  const auto short_form = try_decode_user_info(std::span<const std::byte>(full).first(7));
  AW_CHECK(!short_form.has_value());
  AW_CHECK(short_form.error() == DecodeError::truncated);

  std::vector<std::byte> oversized = full;
  oversized.push_back(std::byte{0});
  const auto trailing = try_decode_user_info(oversized);
  AW_CHECK(!trailing.has_value());
  AW_CHECK(trailing.error() == DecodeError::trailing_bytes);
}

AW_TEST_CASE("AccessMask: privileges map to the legacy byte/bit positions") {
  AccessMask mask;
  mask.set(AccessPrivilege::DeleteFile);     // 0  -> byte 0, MSB
  mask.set(AccessPrivilege::DownloadFile);   // 2  -> byte 0, bit 5
  mask.set(AccessPrivilege::AdmInSpector);   // 53 -> byte 6, bit 2

  std::array<std::byte, kAccessMaskSize> encoded{};
  encode_access_mask(mask, encoded);
  AW_REQUIRE_BYTES(encoded, "a0 00 00 00 00 00 04 00");
  AW_CHECK(mask.bits() == 0xA000000000000400ULL);

  const auto decoded = try_decode_access_mask(bytes_from_hex("a0 00 00 00 00 00 04 00"));
  AW_CHECK(decoded.has_value());
  AW_CHECK(decoded->has(AccessPrivilege::DeleteFile));
  AW_CHECK(!decoded->has(AccessPrivilege::UploadFile));
  AW_CHECK(decoded->has(AccessPrivilege::DownloadFile));
  AW_CHECK(decoded->has(AccessPrivilege::AdmInSpector));
  AW_CHECK(!decoded->has(AccessPrivilege::PostBefore));
  AW_CHECK(decoded->bits() == mask.bits());
  AW_CHECK(*decoded == mask);
}

AW_TEST_CASE("AccessMask: fill/clear and boundary privileges") {
  AccessMask mask;
  mask.fill();
  AW_CHECK(mask.has(AccessPrivilege::DeleteFile));
  AW_CHECK(mask.has(AccessPrivilege::PostBefore));
  std::array<std::byte, kAccessMaskSize> encoded{};
  encode_access_mask(mask, encoded);
  AW_REQUIRE_BYTES(encoded, "ff ff ff ff ff ff ff ff");

  mask.clear();
  AW_CHECK(!mask.has(AccessPrivilege::DeleteFile));
  AW_CHECK(mask.bits() == 0);

  mask.set(AccessPrivilege::PostBefore);  // 54 -> byte 6, bit 1
  encode_access_mask(mask, encoded);
  AW_REQUIRE_BYTES(encoded, "00 00 00 00 00 00 02 00");

  const auto truncated = try_decode_access_mask(bytes_from_hex("a0 00 00"));
  AW_CHECK(!truncated.has_value());
  AW_CHECK(truncated.error() == DecodeError::truncated);
  const auto trailing = try_decode_access_mask(bytes_from_hex("a0 00 00 00 00 00 04 00 00"));
  AW_CHECK(!trailing.has_value());
  AW_CHECK(trailing.error() == DecodeError::trailing_bytes);
}

AW_TEST_CASE("DateTimeStamp: golden 8-byte big-endian form") {
  DateTimeStamp stamp;
  stamp.year = 2003;
  stamp.msecs = 500;
  stamp.seconds = 12345;
  std::array<std::byte, kDateTimeStampSize> encoded{};
  encode_date_time_stamp(stamp, encoded);
  AW_REQUIRE_BYTES(encoded, "07 d3 01 f4 00 00 30 39");

  const auto decoded = decode_date_time_stamp(encoded);
  AW_CHECK(decoded.year == 2003);
  AW_CHECK(decoded.msecs == 500);
  AW_CHECK(decoded.seconds == 12345);

  const auto truncated = try_decode_date_time_stamp(bytes_from_hex("07 d3 01 f4 00 00 30"));
  AW_CHECK(!truncated.has_value());
  AW_CHECK(truncated.error() == DecodeError::truncated);
}

