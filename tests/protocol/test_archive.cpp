#include "hotline/protocol/archive.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "appwarrior/testing.h"

using namespace hotline::protocol;
using namespace aw::test;

namespace {

void append_u32be(std::vector<std::byte>& out, std::uint32_t value) {
  out.push_back(static_cast<std::byte>((value >> 24) & 0xFFU));
  out.push_back(static_cast<std::byte>((value >> 16) & 0xFFU));
  out.push_back(static_cast<std::byte>((value >> 8) & 0xFFU));
  out.push_back(static_cast<std::byte>(value & 0xFFU));
}

// One-entry 'harc' built from the layout (HotlineArchiveStruct.h): 98-byte
// header, then path head (10), path (10: count=1 + {script 0, len 5,
// "a.txt"}), file rsvd (2), file head (12), payload.
auto make_archive(std::string_view name, std::uint32_t compression_type,
                  std::uint32_t decompressed_size, const std::vector<std::byte>& payload)
    -> std::vector<std::byte> {
  std::vector<std::byte> out = bytes_from_hex("68 61 72 63 00 00 00 01");  // 'harc', vers 1

  const std::size_t entry_size = 10 + 10 + 2 + 12 + payload.size();
  append_u32be(out, static_cast<std::uint32_t>(86 + entry_size));  // archiveSize
  out.insert(out.end(), 16, std::byte{0});                         // rsvd[4]

  // 64-byte p-string name field.
  out.push_back(static_cast<std::byte>(std::min<std::size_t>(name.size(), 255)));
  for (const char character : name) {
    out.push_back(static_cast<std::byte>(static_cast<unsigned char>(character)));
  }
  while (out.size() < 92) {
    out.push_back(std::byte{0});
  }
  out.push_back(std::byte{0});  // fileCount hi
  out.push_back(std::byte{1});  // fileCount lo
  out.push_back(std::byte{0});  // fileAutoLaunch hi
  out.push_back(std::byte{0});  // fileAutoLaunch lo
  out.push_back(std::byte{0});  // rsvd3size hi
  out.push_back(std::byte{0});  // rsvd3size lo

  const std::vector<std::byte> entry_head = bytes_from_hex(
      "66 69 6c 65 00 00 00 00 00 0a"          // type 'file', rsvd, pathSize = 10
      " 00 01 00 00 05 61 2e 74 78 74"         // path: count 1, {script 0, "a.txt"}
      " 00 00");                               // file rsvd size 0
  out.insert(out.end(), entry_head.begin(), entry_head.end());
  append_u32be(out, compression_type);
  append_u32be(out, decompressed_size);
  append_u32be(out, static_cast<std::uint32_t>(payload.size()));
  out.insert(out.end(), payload.begin(), payload.end());
  return out;
}

}  // namespace

AW_TEST_CASE("harc: raw-compressed golden decodes entry and payload") {
  const std::vector<std::byte> bytes =
      make_archive("hi", kArchiveCompressionRaw, 2, bytes_from_hex("68 69"));
  const auto decoded = try_decode_archive(bytes);
  AW_CHECK(decoded.has_value());
  AW_CHECK(decoded->version == 1U);
  AW_CHECK(decoded->archive_name == "hi");
  AW_CHECK(decoded->file_auto_launch == 0);
  AW_CHECK(decoded->entries.size() == 1U);

  const ArchiveEntry& entry = decoded->entries[0];
  AW_CHECK(entry.type == aw::endian::four_cc('f', 'i', 'l', 'e'));
  AW_CHECK(entry.path.size() == 1U);
  AW_CHECK(entry.path[0].name == "a.txt");
  AW_CHECK(entry.compression_type == kArchiveCompressionRaw);
  AW_CHECK(entry.decompressed_size == 2U);
  AW_REQUIRE_BYTES(entry.payload, "68 69");

  const auto inflated = decompress_archive_entry(entry);
  AW_CHECK(inflated.has_value());
  AW_REQUIRE_BYTES(*inflated, "68 69");
}

AW_TEST_CASE("harc: zlib payload inflates (independent python oracle)") {
  const std::string text = "The quick brown fox jumps over the lazy dog";
  const std::vector<std::byte> zlib_payload = bytes_from_hex(
      "78 9c 0b c9 48 55 28 2c cd 4c ce 56 48 2a ca 2f cf 53 48 cb af 50 c8 2a cd 2d 28 56"
      " c8 2f 4b 2d 52 28 01 4a e7 24 56 55 2a a4 e4 a7 03 00 5b dc 0f da");

  const std::vector<std::byte> bytes =
      make_archive("z", kArchiveCompressionZlib, static_cast<std::uint32_t>(text.size()),
                   zlib_payload);
  const auto decoded = try_decode_archive(bytes);
  AW_CHECK(decoded.has_value());
  AW_CHECK(decoded->entries.size() == 1U);
  AW_CHECK(decoded->entries[0].compression_type == kArchiveCompressionZlib);
  AW_CHECK(decoded->entries[0].decompressed_size == text.size());

  const auto inflated = decompress_archive_entry(decoded->entries[0]);
  AW_CHECK(inflated.has_value());
  AW_CHECK(std::ranges::equal(*inflated, bytes_from_ascii(text)));
}

AW_TEST_CASE("harc: header validation errors") {
  const std::vector<std::byte> golden =
      make_archive("hi", kArchiveCompressionRaw, 2, bytes_from_hex("68 69"));

  std::vector<std::byte> wrong_tag = golden;
  wrong_tag[0] = std::byte{0};
  const auto wrong_tag_result = try_decode_archive(wrong_tag);
  AW_CHECK(!wrong_tag_result.has_value());
  AW_CHECK(wrong_tag_result.error() == DecodeError::wrong_format_tag);

  std::vector<std::byte> bad_version = golden;
  bad_version[7] = std::byte{2};
  const auto bad_version_result = try_decode_archive(bad_version);
  AW_CHECK(!bad_version_result.has_value());
  AW_CHECK(bad_version_result.error() == DecodeError::unsupported_version);

  for (std::size_t prefix = 0; prefix < golden.size(); ++prefix) {
    const auto decoded = try_decode_archive(std::span<const std::byte>(golden).first(prefix));
    AW_CHECK(!decoded.has_value());
  }
}

AW_TEST_CASE("harc: entry-level shape errors") {
  // pathSize overruns the buffer.
  const std::vector<std::byte> path_overrun_input = [] {
    std::vector<std::byte> out =
        make_archive("hi", kArchiveCompressionRaw, 2, bytes_from_hex("68 69"));
    out[98 + 8] = std::byte{0xFF};  // pathSize hi byte
    out[98 + 9] = std::byte{0xFF};  // pathSize lo byte
    return out;
  }();
  const auto path_overrun = try_decode_archive(path_overrun_input);
  AW_CHECK(!path_overrun.has_value());
  AW_CHECK(path_overrun.error() == DecodeError::truncated);

  // Trailing byte after the archive.
  std::vector<std::byte> oversized =
      make_archive("hi", kArchiveCompressionRaw, 2, bytes_from_hex("68 69"));
  oversized.push_back(std::byte{0});
  const auto trailing = try_decode_archive(oversized);
  AW_CHECK(!trailing.has_value());
  AW_CHECK(trailing.error() == DecodeError::trailing_bytes);
}

AW_TEST_CASE("harc: decompress error paths") {
  ArchiveEntry entry;

  entry.compression_type = aw::endian::four_cc('x', 'x', 'x', 'x');
  entry.decompressed_size = 1;
  entry.payload = bytes_from_hex("00");
  const auto unsupported = decompress_archive_entry(entry);
  AW_CHECK(!unsupported.has_value());
  AW_CHECK(unsupported.error() == ArchiveError::unsupported_compression);

  entry.compression_type = kArchiveCompressionRaw;
  entry.decompressed_size = 5;
  entry.payload = bytes_from_hex("00 01");
  const auto mismatch = decompress_archive_entry(entry);
  AW_CHECK(!mismatch.has_value());
  AW_CHECK(mismatch.error() == ArchiveError::decompressed_size_mismatch);

  entry.compression_type = kArchiveCompressionZlib;
  entry.decompressed_size = 10;
  entry.payload = bytes_from_hex("de ad be ef");
  const auto zlib_failure = decompress_archive_entry(entry);
  AW_CHECK(!zlib_failure.has_value());
  AW_CHECK(zlib_failure.error() == ArchiveError::zlib_failure);

  entry.compression_type = kArchiveCompressionRaw;
  entry.decompressed_size = kMaxArchiveEntryDecompressedSize + 1;
  entry.payload = bytes_from_hex("00");
  const auto too_big = decompress_archive_entry(entry);
  AW_CHECK(!too_big.has_value());
  AW_CHECK(too_big.error() == ArchiveError::exceeds_size_cap);
}
