#include "hotline/protocol/transfer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "appwarrior/testing.h"

using namespace hotline::protocol;
using namespace aw::test;

AW_TEST_CASE("FILP: minimal golden header + two empty forks") {
  FlatFile file;
  file.version = 1;
  file.forks.push_back(FlatFileFork{});
  file.forks.push_back(FlatFileFork{});

  const std::vector<std::byte> encoded = unwrap(encode_flat_file(file));
  // 'FILP', version 1, 16 reserved zeros, forkCount 2, two empty fork headers.
  AW_REQUIRE_BYTES_MSG(std::span<const std::byte>(encoded).first<24>(),
                       "46 49 4c 50 00 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 02",
                       "FILP header");
  AW_CHECK(encoded.size() == kFlatFileHeaderSize + 2 * kFlatFileForkHeaderSize);

  const auto decoded = try_decode_flat_file(encoded);
  AW_CHECK(decoded.has_value());
  AW_CHECK(decoded->version == 1);
  AW_CHECK(decoded->forks.size() == 2U);
}

AW_TEST_CASE("FILP: info + data fork round-trip") {
  FlatFileInfo info;
  info.platform = aw::endian::four_cc('M', 'W', 'I', 'N');
  info.type_sig = aw::endian::four_cc('T', 'E', 'X', 'T');
  info.creator_sig = aw::endian::four_cc('t', 't', 'x', 't');
  info.name = "a.txt";

  FlatFile file;
  file.version = 1;
  file.forks.push_back(FlatFileFork{kForkTypeInfo, 0, unwrap(encode_info_fork(info))});
  file.forks.push_back(FlatFileFork{kForkTypeData, 0, bytes_from_hex("68 69")});

  const auto decoded = try_decode_flat_file(unwrap(encode_flat_file(file)));
  AW_CHECK(decoded.has_value());
  AW_CHECK(decoded->forks.size() == 2U);
  AW_CHECK(decoded->forks[0].type == kForkTypeInfo);
  AW_CHECK(decoded->forks[1].type == kForkTypeData);

  const auto decoded_info = try_decode_info_fork(decoded->forks[0].data);
  AW_CHECK(decoded_info.has_value());
  AW_CHECK(decoded_info->platform == aw::endian::four_cc('M', 'W', 'I', 'N'));
  AW_CHECK(decoded_info->type_sig == aw::endian::four_cc('T', 'E', 'X', 'T'));
  AW_CHECK(decoded_info->name == "a.txt");
  AW_REQUIRE_BYTES(decoded->forks[1].data, "68 69");
}

AW_TEST_CASE("FILP info fork: golden bytes (Windows writer shape)") {
  FlatFileInfo info;
  info.platform = aw::endian::four_cc('M', 'W', 'I', 'N');
  info.type_sig = aw::endian::four_cc('T', 'E', 'X', 'T');
  info.creator_sig = aw::endian::four_cc('t', 't', 'x', 't');
  info.name = "a.txt";

  const std::vector<std::byte> encoded = unwrap(encode_info_fork(info));
  // 72 fixed bytes: platform/type/creator/flags/platFlags BE, 32 reserved,
  // two zero date stamps, script 0, nameSize 5, "a.txt", zero-length comment.
  AW_REQUIRE_BYTES_MSG(
      encoded,
      "4d 57 49 4e 54 45 58 54 74 74 78 74 00 00 00 00 00 00 00 00"
      " 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"
      " 00 00 00 00 00 00 00 00 00 00 00 00"
      " 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"
      " 00 00 00 05"
      " 61 2e 74 78 74 00 00",
      "info fork");

  const auto decoded = try_decode_info_fork(encoded);
  AW_CHECK(decoded.has_value());
  AW_CHECK(decoded->name == "a.txt");
  AW_CHECK(decoded->comment.empty());
}

AW_TEST_CASE("FILP: malformed inputs are rejected") {
  const auto wrong_tag = try_decode_flat_file(bytes_from_hex(
      "00 00 00 00 00 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"));
  AW_CHECK(!wrong_tag.has_value());
  AW_CHECK(wrong_tag.error() == DecodeError::wrong_format_tag);

  const std::vector<std::byte> full = bytes_from_hex(
      "46 49 4c 50 00 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 01"
      " 49 4e 46 4f 00 00 00 00 00 00 00 00 00 00 00 01 78");
  // fork header declares 1 data byte but none present -> truncated.
  const auto truncated = try_decode_flat_file(std::span<const std::byte>(full).first(40));
  AW_CHECK(!truncated.has_value());
  AW_CHECK(truncated.error() == DecodeError::truncated);

  std::vector<std::byte> oversized = full;
  oversized.push_back(std::byte{0});
  const auto trailing = try_decode_flat_file(oversized);
  AW_CHECK(!trailing.has_value());
  AW_CHECK(trailing.error() == DecodeError::trailing_bytes);
}

AW_TEST_CASE("RFLT: golden resume record for a 100-byte data fork") {
  ResumeData resume;
  resume.version = 1;
  resume.entries.push_back(ResumeEntry{kForkTypeData, 100});

  const std::vector<std::byte> encoded = unwrap(encode_resume_data(resume));
  AW_REQUIRE_BYTES_MSG(
      encoded,
      "52 46 4c 54 00 01"
      " 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"
      " 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"
      " 00 01"
      " 44 41 54 41 00 00 00 64 00 00 00 00 00 00 00 00",
      "RFLT golden");

  const auto decoded = try_decode_resume_data(encoded);
  AW_CHECK(decoded.has_value());
  AW_CHECK(decoded->version == 1);
  AW_CHECK(decoded->entries.size() == 1U);
  AW_CHECK(decoded->entries[0].fork == kForkTypeData);
  AW_CHECK(decoded->entries[0].data_size == 100U);
  AW_CHECK(data_resume_size(*decoded) == 100U);
}

AW_TEST_CASE("RFLT: the last DATA entry wins (legacy ResumeFlatten semantics)") {
  ResumeData resume;
  resume.version = 1;
  resume.entries.push_back(ResumeEntry{kForkTypeData, 50});
  resume.entries.push_back(ResumeEntry{aw::endian::four_cc('M', 'A', 'C', 'R'), 99});
  resume.entries.push_back(ResumeEntry{kForkTypeData, 100});
  AW_CHECK(data_resume_size(resume) == 100U);

  ResumeData no_data;
  no_data.version = 1;
  no_data.entries.push_back(ResumeEntry{aw::endian::four_cc('M', 'A', 'C', 'R'), 99});
  AW_CHECK(data_resume_size(no_data) == 0U);
}

AW_TEST_CASE("RFLT: tag, version and shape errors") {
  const auto wrong_tag = try_decode_resume_data(bytes_from_hex(
      "00 00 00 00 00 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"
      " 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"));
  AW_CHECK(!wrong_tag.has_value());
  AW_CHECK(wrong_tag.error() == DecodeError::wrong_format_tag);

  const auto bad_version = try_decode_resume_data(bytes_from_hex(
      "52 46 4c 54 00 02 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"
      " 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"));
  AW_CHECK(!bad_version.has_value());
  AW_CHECK(bad_version.error() == DecodeError::unsupported_version);

  const std::vector<std::byte> full = bytes_from_hex(
      "52 46 4c 54 00 01"
      " 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"
      " 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"
      " 00 01"
      " 44 41 54 41 00 00 00 64 00 00 00 00 00 00 00 00");
  for (std::size_t prefix = 0; prefix < full.size(); ++prefix) {
    const auto decoded = try_decode_resume_data(std::span<const std::byte>(full).first(prefix));
    AW_CHECK(!decoded.has_value());
    AW_CHECK(decoded.error() == DecodeError::truncated);
  }
}

AW_TEST_CASE("folder download item: golden file and folder headers") {
  FolderDownloadItem file;
  file.folder = false;
  file.path.push_back(FolderPathComponent{0, "a.txt"});
  AW_REQUIRE_BYTES_MSG(unwrap(encode_folder_download_item(file)),
                       "00 0c 00 00 00 01 00 00 05 61 2e 74 78 74", "file item");

  FolderDownloadItem folder;
  folder.folder = true;
  folder.path.push_back(FolderPathComponent{0, "a.txt"});
  AW_REQUIRE_BYTES_MSG(unwrap(encode_folder_download_item(folder)),
                       "00 0c 00 01 00 01 00 00 05 61 2e 74 78 74", "folder item");

  const auto decoded = try_decode_folder_download_item(
      bytes_from_hex("00 0c 00 01 00 01 00 00 05 61 2e 74 78 74"));
  AW_CHECK(decoded.has_value());
  AW_CHECK(decoded->folder);
  AW_CHECK(decoded->path.size() == 1U);
  AW_CHECK(decoded->path[0].name == "a.txt");
}

AW_TEST_CASE("folder download item: multi-component path") {
  FolderDownloadItem item;
  item.path.push_back(FolderPathComponent{0, "sub"});
  item.path.push_back(FolderPathComponent{0, "a.txt"});
  const std::vector<std::byte> encoded = unwrap(encode_folder_download_item(item));
  AW_REQUIRE_BYTES_MSG(encoded,
                       "00 12 00 00 00 02 00 00 03 73 75 62 00 00 05 61 2e 74 78 74",
                       "two-component item");

  const auto decoded = try_decode_folder_download_item(encoded);
  AW_CHECK(decoded.has_value());
  AW_CHECK(decoded->path.size() == 2U);
  AW_CHECK(decoded->path[0].name == "sub");
  AW_CHECK(decoded->path[1].name == "a.txt");
}

AW_TEST_CASE("folder download commands: verified 1/2/3 mapping") {
  AW_REQUIRE_BYTES(encode_folder_download_command(FolderDownloadAction::SendFile), "00 01");
  AW_REQUIRE_BYTES(encode_folder_download_command(FolderDownloadAction::ResumeFile), "00 02");
  AW_REQUIRE_BYTES(encode_folder_download_command(FolderDownloadAction::NextFile), "00 03");
}

AW_TEST_CASE("folder download resume command round-trips") {
  ResumeData resume;
  resume.version = 1;
  resume.entries.push_back(ResumeEntry{kForkTypeData, 100});

  const std::vector<std::byte> encoded = unwrap(encode_folder_download_resume(resume));
  AW_CHECK(encoded.size() == 4U + kResumeDataHeaderSize + kResumeEntrySize);
  AW_REQUIRE_BYTES_MSG(std::span<const std::byte>(encoded).first<4>(), "00 02 00 3a",
                       "action + size prefix");

  const auto decoded = try_decode_folder_download_resume(encoded);
  AW_CHECK(decoded.has_value());
  AW_CHECK(data_resume_size(*decoded) == 100U);

  const auto wrong_action = try_decode_folder_download_resume(bytes_from_hex("00 01 00 00"));
  AW_CHECK(!wrong_action.has_value());
  AW_CHECK(wrong_action.error() == DecodeError::wrong_format_tag);
}
