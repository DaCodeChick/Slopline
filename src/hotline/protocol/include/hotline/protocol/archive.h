// Hotline wire protocol: 'harc' archive container.
//
// The update-package container (legacy HotlineArchiveStruct.h +
// HotlineArchiveDecoder.cpp): header 'harc' + version 1 + archiveSize +
// 16 reserved + 64-byte p-string name + fileCount + fileAutoLaunch +
// rsvd3, then per file: {type u32 ('file'/'fldr'/'link'/'text'), rsvd u32,
// pathSize u16, path bytes {u16 pathCount + components {u16 script,
// u8 namelen, name}}, rsvdSize u16 + rsvd, compressionType u32 ('zlib' /
// 'raw '), decompressedSize u32, compressedSize u32, payload}. The payload
// is a zlib-compressed (or raw) FILP flat file.
//
// The decoder is bounded and validated; the legacy decoder allocated
// straight from the attacker-visible decompressedSize, so the modern one
// caps it (kMaxArchiveEntryDecompressedSize, 64 MiB) — documented
// hardening.

#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <vector>

#include "appwarrior/core/endian.h"
#include "hotline/protocol/decode_error.h"
#include "hotline/protocol/transfer.h"

namespace hotline::protocol {

inline constexpr std::uint32_t kArchiveFormat = aw::endian::four_cc('h', 'a', 'r', 'c');
inline constexpr std::uint32_t kArchiveVersion = 1;
inline constexpr std::uint32_t kArchiveCompressionZlib = aw::endian::four_cc('z', 'l', 'i', 'b');
inline constexpr std::uint32_t kArchiveCompressionRaw = aw::endian::four_cc('r', 'a', 'w', ' ');
inline constexpr std::size_t kArchiveHeaderSize = 98;  // 4+4+4+16+64+2+2+2
inline constexpr std::size_t kMaxArchiveEntryDecompressedSize = 64U * 1024U * 1024U;

struct ArchiveEntry {
  std::uint32_t type = 0;  // 'file' / 'fldr' / 'link' / 'text'
  std::vector<FolderPathComponent> path;
  std::vector<std::byte> reserved;
  std::uint32_t compression_type = 0;
  std::uint32_t decompressed_size = 0;
  std::vector<std::byte> payload;  // compressedSize bytes on the wire
};

struct Archive {
  std::uint32_t version = 0;
  std::uint32_t archive_size = 0;  // legacy "size of everything below this u32"
  std::string archive_name;
  std::uint16_t file_auto_launch = 0;
  std::vector<std::byte> reserved3;
  std::vector<ArchiveEntry> entries;
};

enum class ArchiveError {
  unsupported_compression,
  decompressed_size_mismatch,
  zlib_failure,
  exceeds_size_cap,
};

[[nodiscard]] auto try_decode_archive(std::span<const std::byte> bytes)
    -> std::expected<Archive, DecodeError>;

// Inflates a 'zlib' entry or passes a 'raw ' entry through, producing
// exactly decompressed_size bytes. Bounded by
// kMaxArchiveEntryDecompressedSize.
[[nodiscard]] auto decompress_archive_entry(const ArchiveEntry& entry)
    -> std::expected<std::vector<std::byte>, ArchiveError>;

}  // namespace hotline::protocol
