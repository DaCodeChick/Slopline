#include "hotline/protocol/transfer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "appwarrior/core/endian.h"

namespace hotline::protocol {

using aw::endian::read_u16be;
using aw::endian::read_u32be;
using aw::endian::write_u16be;
using aw::endian::write_u32be;

namespace {

auto append_raw(std::vector<std::byte>& out, std::string_view text) -> void {
  for (const char character : text) {
    out.push_back(static_cast<std::byte>(static_cast<unsigned char>(character)));
  }
}

auto read_raw_name(std::span<const std::byte> bytes) -> std::string {
  std::string text;
  text.reserve(bytes.size());
  for (const std::byte byte : bytes) {
    text.push_back(static_cast<char>(std::to_integer<unsigned char>(byte)));
  }
  return text;
}

void append_u16(std::vector<std::byte>& out, std::uint16_t value) {
  std::array<std::byte, 2> bytes{};
  write_u16be(value, bytes);
  out.insert(out.end(), bytes.begin(), bytes.end());
}

void append_u32(std::vector<std::byte>& out, std::uint32_t value) {
  std::array<std::byte, 4> bytes{};
  write_u32be(value, bytes);
  out.insert(out.end(), bytes.begin(), bytes.end());
}

}  // namespace

auto encode_flat_file(const FlatFile& file)
    -> std::expected<std::vector<std::byte>, EncodeError> {
  if (file.forks.size() > kMaxFieldCount) {
    return std::unexpected(EncodeError::count_too_large);
  }

  std::vector<std::byte> out;
  out.reserve(kFlatFileHeaderSize + file.forks.size() * kFlatFileForkHeaderSize);
  append_u32(out, kFlatFileFormat);
  append_u16(out, file.version);
  out.insert(out.end(), 16, std::byte{0});
  append_u16(out, static_cast<std::uint16_t>(file.forks.size()));

  for (const FlatFileFork& fork : file.forks) {
    append_u32(out, fork.type);
    append_u32(out, fork.compression_type);
    append_u32(out, 0);  // rsvd
    append_u32(out, static_cast<std::uint32_t>(fork.data.size()));
    out.insert(out.end(), fork.data.begin(), fork.data.end());
  }
  return out;
}

auto try_decode_flat_file(std::span<const std::byte> bytes)
    -> std::expected<FlatFile, DecodeError> {
  if (bytes.size() < kFlatFileHeaderSize) {
    return std::unexpected(DecodeError::truncated);
  }
  if (read_u32be(bytes.first<4>()) != kFlatFileFormat) {
    return std::unexpected(DecodeError::wrong_format_tag);
  }

  FlatFile file;
  file.version = read_u16be(bytes.subspan(4).first<2>());
  const std::uint16_t fork_count = read_u16be(bytes.subspan(22).first<2>());
  bytes = bytes.subspan(kFlatFileHeaderSize);
  file.forks.reserve(fork_count);

  for (std::uint16_t i = 0; i < fork_count; ++i) {
    if (bytes.size() < kFlatFileForkHeaderSize) {
      return std::unexpected(DecodeError::truncated);
    }
    FlatFileFork fork;
    fork.type = read_u32be(bytes.first<4>());
    fork.compression_type = read_u32be(bytes.subspan(4).first<4>());
    const std::size_t data_size = read_u32be(bytes.subspan(12).first<4>());
    bytes = bytes.subspan(kFlatFileForkHeaderSize);
    if (bytes.size() < data_size) {
      return std::unexpected(DecodeError::truncated);
    }
    fork.data.assign(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(data_size));
    bytes = bytes.subspan(data_size);
    file.forks.push_back(std::move(fork));
  }

  if (!bytes.empty()) {
    return std::unexpected(DecodeError::trailing_bytes);
  }
  return file;
}

auto encode_info_fork(const FlatFileInfo& info)
    -> std::expected<std::vector<std::byte>, EncodeError> {
  if (info.name.size() > kMaxFieldDataSize || info.comment.size() > kMaxFieldDataSize) {
    return std::unexpected(EncodeError::string_too_long);
  }

  std::vector<std::byte> out;
  out.reserve(kFlatFileInfoFixedSize + info.name.size() + info.comment.size() + 2);
  append_u32(out, info.platform);
  append_u32(out, info.type_sig);
  append_u32(out, info.creator_sig);
  append_u32(out, info.flags);
  append_u32(out, info.platform_flags);
  out.insert(out.end(), info.reserved.begin(), info.reserved.end());
  append_u16(out, info.create_date.year);
  append_u16(out, info.create_date.msecs);
  append_u32(out, info.create_date.seconds);
  append_u16(out, info.modify_date.year);
  append_u16(out, info.modify_date.msecs);
  append_u32(out, info.modify_date.seconds);
  append_u16(out, info.name_script);
  append_u16(out, static_cast<std::uint16_t>(info.name.size()));
  append_raw(out, info.name);
  append_u16(out, static_cast<std::uint16_t>(info.comment.size()));
  out.insert(out.end(), info.comment.begin(), info.comment.end());
  return out;
}

auto try_decode_info_fork(std::span<const std::byte> bytes)
    -> std::expected<FlatFileInfo, DecodeError> {
  if (bytes.size() < kFlatFileInfoFixedSize) {
    return std::unexpected(DecodeError::truncated);
  }

  FlatFileInfo info;
  info.platform = read_u32be(bytes.first<4>());
  info.type_sig = read_u32be(bytes.subspan(4).first<4>());
  info.creator_sig = read_u32be(bytes.subspan(8).first<4>());
  info.flags = read_u32be(bytes.subspan(12).first<4>());
  info.platform_flags = read_u32be(bytes.subspan(16).first<4>());
  std::ranges::copy(bytes.subspan(20, 32), info.reserved.begin());
  const auto create = try_decode_date_time_stamp(bytes.subspan(52).first<8>());
  const auto modify = try_decode_date_time_stamp(bytes.subspan(60).first<8>());
  info.create_date = create.value();
  info.modify_date = modify.value();
  info.name_script = read_u16be(bytes.subspan(68).first<2>());
  const std::size_t name_size = read_u16be(bytes.subspan(70).first<2>());
  if (bytes.size() < kFlatFileInfoFixedSize + name_size + 2) {
    return std::unexpected(DecodeError::truncated);
  }
  info.name = read_raw_name(bytes.subspan(kFlatFileInfoFixedSize, name_size));
  const std::size_t comment_size =
      read_u16be(bytes.subspan(kFlatFileInfoFixedSize + name_size).first<2>());
  const std::size_t total = kFlatFileInfoFixedSize + name_size + 2 + comment_size;
  if (bytes.size() < total) {
    return std::unexpected(DecodeError::truncated);
  }
  if (bytes.size() > total) {
    return std::unexpected(DecodeError::trailing_bytes);
  }
  info.comment.assign(bytes.begin() + static_cast<std::ptrdiff_t>(kFlatFileInfoFixedSize + name_size + 2),
                      bytes.begin() + static_cast<std::ptrdiff_t>(total));
  return info;
}

auto encode_resume_data(const ResumeData& resume)
    -> std::expected<std::vector<std::byte>, EncodeError> {
  if (resume.entries.size() > kMaxFieldCount) {
    return std::unexpected(EncodeError::count_too_large);
  }

  std::vector<std::byte> out;
  out.reserve(kResumeDataHeaderSize + resume.entries.size() * kResumeEntrySize);
  append_u32(out, kResumeDataFormat);
  append_u16(out, resume.version);
  out.insert(out.end(), 34, std::byte{0});
  append_u16(out, static_cast<std::uint16_t>(resume.entries.size()));
  for (const ResumeEntry& entry : resume.entries) {
    append_u32(out, entry.fork);
    append_u32(out, entry.data_size);
    append_u32(out, 0);  // rsvdA
    append_u32(out, 0);  // rsvdB
  }
  return out;
}

auto try_decode_resume_data(std::span<const std::byte> bytes)
    -> std::expected<ResumeData, DecodeError> {
  if (bytes.size() < kResumeDataHeaderSize) {
    return std::unexpected(DecodeError::truncated);
  }
  if (read_u32be(bytes.first<4>()) != kResumeDataFormat) {
    return std::unexpected(DecodeError::wrong_format_tag);
  }
  const std::uint16_t version = read_u16be(bytes.subspan(4).first<2>());
  if (version != 1) {
    return std::unexpected(DecodeError::unsupported_version);
  }
  const std::size_t count = read_u16be(bytes.subspan(40).first<2>());
  const std::size_t total = kResumeDataHeaderSize + count * kResumeEntrySize;
  if (bytes.size() < total) {
    return std::unexpected(DecodeError::truncated);
  }
  if (bytes.size() > total) {
    return std::unexpected(DecodeError::trailing_bytes);
  }

  ResumeData resume;
  resume.version = version;
  resume.entries.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    const std::span<const std::byte> entry =
        bytes.subspan(kResumeDataHeaderSize + i * kResumeEntrySize, kResumeEntrySize);
    ResumeEntry item;
    item.fork = read_u32be(entry.first<4>());
    item.data_size = read_u32be(entry.subspan(4).first<4>());
    resume.entries.push_back(item);
  }
  return resume;
}

auto data_resume_size(const ResumeData& resume) noexcept -> std::uint64_t {
  std::uint64_t size = 0;
  for (const ResumeEntry& entry : resume.entries) {
    if (entry.fork == kForkTypeData) {
      size = entry.data_size;  // last DATA entry wins (legacy semantics)
    }
  }
  return size;
}

auto encode_folder_download_item(const FolderDownloadItem& item)
    -> std::expected<std::vector<std::byte>, EncodeError> {
  std::vector<std::byte> path;
  for (const FolderPathComponent& component : item.path) {
    if (component.name.size() > 255) {
      return std::unexpected(EncodeError::string_too_long);
    }
    append_u16(path, component.script);
    path.push_back(static_cast<std::byte>(component.name.size()));
    append_raw(path, component.name);
  }

  const std::size_t size = 4 + path.size();  // type + pathCount + path bytes
  if (size > kMaxFieldDataSize) {
    return std::unexpected(EncodeError::element_too_large);
  }

  std::vector<std::byte> out;
  out.reserve(2 + size);
  append_u16(out, static_cast<std::uint16_t>(size));
  append_u16(out, item.folder ? 1 : 0);
  append_u16(out, static_cast<std::uint16_t>(item.path.size()));
  out.insert(out.end(), path.begin(), path.end());
  return out;
}

auto try_decode_folder_download_item(std::span<const std::byte> bytes)
    -> std::expected<FolderDownloadItem, DecodeError> {
  if (bytes.size() < 2) {
    return std::unexpected(DecodeError::truncated);
  }
  const std::size_t size = read_u16be(bytes.first<2>());
  if (size < 4) {
    return std::unexpected(DecodeError::truncated);
  }
  if (bytes.size() < 2 + size) {
    return std::unexpected(DecodeError::truncated);
  }
  if (bytes.size() > 2 + size) {
    return std::unexpected(DecodeError::trailing_bytes);
  }

  FolderDownloadItem item;
  item.folder = (read_u16be(bytes.subspan(2).first<2>()) & 1U) != 0;
  const std::size_t path_count = read_u16be(bytes.subspan(4).first<2>());
  std::span<const std::byte> path = bytes.subspan(6, size - 4);
  item.path.reserve(path_count);

  for (std::size_t i = 0; i < path_count; ++i) {
    if (path.size() < 3) {
      return std::unexpected(DecodeError::truncated);
    }
    FolderPathComponent component;
    component.script = read_u16be(path.first<2>());
    const std::size_t name_size = std::to_integer<std::uint8_t>(path[2]);
    path = path.subspan(3);
    if (path.size() < name_size) {
      return std::unexpected(DecodeError::truncated);
    }
    component.name = read_raw_name(path.first(name_size));
    path = path.subspan(name_size);
    item.path.push_back(std::move(component));
  }
  if (!path.empty()) {
    return std::unexpected(DecodeError::trailing_bytes);
  }
  return item;
}

auto encode_folder_download_command(FolderDownloadAction action) -> std::array<std::byte, 2> {
  std::array<std::byte, 2> out{};
  write_u16be(static_cast<std::uint16_t>(action), out);
  return out;
}

auto encode_folder_download_resume(const ResumeData& resume)
    -> std::expected<std::vector<std::byte>, EncodeError> {
  auto rflt_result = encode_resume_data(resume);
  if (!rflt_result.has_value()) {
    return std::unexpected(rflt_result.error());
  }
  std::vector<std::byte> rflt = std::move(*rflt_result);
  if (rflt.size() > kMaxFieldDataSize) {
    return std::unexpected(EncodeError::element_too_large);
  }
  std::vector<std::byte> out;
  out.reserve(4 + rflt.size());
  append_u16(out, static_cast<std::uint16_t>(FolderDownloadAction::ResumeFile));
  append_u16(out, static_cast<std::uint16_t>(rflt.size()));
  out.insert(out.end(), rflt.begin(), rflt.end());
  return out;
}

auto try_decode_folder_download_resume(std::span<const std::byte> bytes)
    -> std::expected<ResumeData, DecodeError> {
  if (bytes.size() < 4) {
    return std::unexpected(DecodeError::truncated);
  }
  const std::uint16_t action = read_u16be(bytes.first<2>());
  if (action != static_cast<std::uint16_t>(FolderDownloadAction::ResumeFile)) {
    return std::unexpected(DecodeError::wrong_format_tag);
  }
  const std::size_t size = read_u16be(bytes.subspan(2).first<2>());
  if (bytes.size() < 4 + size) {
    return std::unexpected(DecodeError::truncated);
  }
  if (bytes.size() > 4 + size) {
    return std::unexpected(DecodeError::trailing_bytes);
  }
  return try_decode_resume_data(bytes.subspan(4, size));
}

}  // namespace hotline::protocol
