#include "hotline/protocol/archive.h"

#include <cstddef>
#include <cstdint>
#include <utility>

#include <zlib.h>

#include "appwarrior/core/endian.h"

namespace hotline::protocol {

using aw::endian::read_u16be;
using aw::endian::read_u32be;

namespace {

auto read_raw_name(std::span<const std::byte> bytes) -> std::string {
  std::string text;
  text.reserve(bytes.size());
  for (const std::byte byte : bytes) {
    text.push_back(static_cast<char>(std::to_integer<unsigned char>(byte)));
  }
  return text;
}

}  // namespace

auto try_decode_archive(std::span<const std::byte> bytes)
    -> std::expected<Archive, DecodeError> {
  if (bytes.size() < kArchiveHeaderSize) {
    return std::unexpected(DecodeError::truncated);
  }
  if (read_u32be(bytes.first<4>()) != kArchiveFormat) {
    return std::unexpected(DecodeError::wrong_format_tag);
  }
  const std::uint32_t version = read_u32be(bytes.subspan(4).first<4>());
  if (version != kArchiveVersion) {
    return std::unexpected(DecodeError::unsupported_version);
  }

  Archive archive;
  archive.version = version;
  archive.archive_size = read_u32be(bytes.subspan(8).first<4>());
  // 64-byte p-string field; the legacy decoder clamped to 63 chars.
  const std::size_t name_length =
      std::min<std::size_t>(std::to_integer<std::uint8_t>(bytes[28]), 63);
  archive.archive_name = read_raw_name(bytes.subspan(29, name_length));
  const std::size_t file_count = read_u16be(bytes.subspan(92).first<2>());
  archive.file_auto_launch = read_u16be(bytes.subspan(94).first<2>());
  const std::size_t rsvd3_size = read_u16be(bytes.subspan(96).first<2>());

  bytes = bytes.subspan(kArchiveHeaderSize);
  if (bytes.size() < rsvd3_size) {
    return std::unexpected(DecodeError::truncated);
  }
  archive.reserved3.assign(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(rsvd3_size));
  bytes = bytes.subspan(rsvd3_size);

  archive.entries.reserve(file_count);
  for (std::size_t i = 0; i < file_count; ++i) {
    ArchiveEntry entry;

    // Path head: type u32, rsvd u32, pathSize u16.
    if (bytes.size() < 10) {
      return std::unexpected(DecodeError::truncated);
    }
    entry.type = read_u32be(bytes.first<4>());
    const std::size_t path_size = read_u16be(bytes.subspan(8).first<2>());
    bytes = bytes.subspan(10);
    if (bytes.size() < path_size) {
      return std::unexpected(DecodeError::truncated);
    }

    // Path: u16 pathCount + components {u16 script, u8 namelen, name}.
    const std::span<const std::byte> path_bytes = bytes.first(path_size);
    if (path_bytes.size() < 2) {
      return std::unexpected(DecodeError::truncated);
    }
    const std::size_t path_count = read_u16be(path_bytes.first<2>());
    std::span<const std::byte> components = path_bytes.subspan(2);
    entry.path.reserve(path_count);
    for (std::size_t c = 0; c < path_count; ++c) {
      if (components.size() < 3) {
        return std::unexpected(DecodeError::truncated);
      }
      FolderPathComponent component;
      component.script = read_u16be(components.first<2>());
      const std::size_t name_size = std::to_integer<std::uint8_t>(components[2]);
      components = components.subspan(3);
      if (components.size() < name_size) {
        return std::unexpected(DecodeError::truncated);
      }
      component.name = read_raw_name(components.first(name_size));
      components = components.subspan(name_size);
      entry.path.push_back(std::move(component));
    }
    if (!components.empty()) {
      return std::unexpected(DecodeError::trailing_bytes);
    }
    bytes = bytes.subspan(path_size);

    // File rsvd: u16 size + bytes.
    if (bytes.size() < 2) {
      return std::unexpected(DecodeError::truncated);
    }
    const std::size_t rsvd_size = read_u16be(bytes.first<2>());
    bytes = bytes.subspan(2);
    if (bytes.size() < rsvd_size) {
      return std::unexpected(DecodeError::truncated);
    }
    entry.reserved.assign(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(rsvd_size));
    bytes = bytes.subspan(rsvd_size);

    // File head: compressionType, decompressedSize, compressedSize.
    if (bytes.size() < 12) {
      return std::unexpected(DecodeError::truncated);
    }
    entry.compression_type = read_u32be(bytes.first<4>());
    entry.decompressed_size = read_u32be(bytes.subspan(4).first<4>());
    const std::size_t compressed_size = read_u32be(bytes.subspan(8).first<4>());
    bytes = bytes.subspan(12);
    if (bytes.size() < compressed_size) {
      return std::unexpected(DecodeError::truncated);
    }
    entry.payload.assign(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(compressed_size));
    bytes = bytes.subspan(compressed_size);

    archive.entries.push_back(std::move(entry));
  }

  if (!bytes.empty()) {
    return std::unexpected(DecodeError::trailing_bytes);
  }
  return archive;
}

auto decompress_archive_entry(const ArchiveEntry& entry)
    -> std::expected<std::vector<std::byte>, ArchiveError> {
  if (entry.decompressed_size > kMaxArchiveEntryDecompressedSize) {
    return std::unexpected(ArchiveError::exceeds_size_cap);
  }

  if (entry.compression_type == kArchiveCompressionRaw) {
    if (entry.payload.size() != entry.decompressed_size) {
      return std::unexpected(ArchiveError::decompressed_size_mismatch);
    }
    return entry.payload;
  }
  if (entry.compression_type != kArchiveCompressionZlib) {
    return std::unexpected(ArchiveError::unsupported_compression);
  }

  std::vector<std::byte> out(entry.decompressed_size);
  uLongf destination_length = static_cast<uLongf>(entry.decompressed_size);
  const int result = ::uncompress(
      reinterpret_cast<Bytef*>(out.data()), &destination_length,
      reinterpret_cast<const Bytef*>(entry.payload.data()),
      static_cast<uLong>(entry.payload.size()));
  if (result != Z_OK || destination_length != entry.decompressed_size) {
    return std::unexpected(ArchiveError::zlib_failure);
  }
  return out;
}

}  // namespace hotline::protocol
