// Hotline wire protocol: file-transfer payload codecs.
//
// FILP flat-file package, RFLT resume records, and the folder-download
// item/command exchange — all verified against the legacy writers/readers
// (UFileSys(W).cpp:2180-2440, HotlineServ.cpp:5500-5570,
// HotlineTasks.cpp:3480-3610, HotlineClientServerCommon.h:243-248):
//
//  * FILP: 24-byte header ('FILP', version, 16 reserved bytes, forkCount)
//    then forkCount × { 16-byte fork header (forkType, compType, rsvd,
//    dataSize BE) + data }. Stream order: INFO fork then DATA fork
//    (Windows build; the Mac build used 3 forks with platform 'AMAC').
//  * INFO fork data: 72 fixed bytes (platform/typeSig/creatorSig/flags/
//    platFlags BE, 32 reserved, two SDateTimeStamps, nameScript/nameSize
//    BE) + name + u16 commentSize + comment.
//  * RFLT: 'RFLT', version 1, 34 reserved bytes, u16 count, then count ×
//    16-byte entries {fork u32, dataSize u32, rsvdA u32, rsvdB u32}.
//  * Folder item: u16 size (bytes after this field), u16 type (bit0: 1 =
//    folder), u16 pathCount, then pathCount × {u16 script, u8 namelen,
//    name} — root component excluded on the wire.
//  * Commands: u16 dlFldrAction — SendFile = 1, ResumeFile = 2,
//    NextFile = 3. (audit/06 §6.4 printed the SendFile/NextFile mapping
//    swapped; the verbatim enum and the server's dispatch — it waits for
//    dlFldrAction_NextFile — confirm 1=SendFile, 2=ResumeFile, 3=NextFile.)
//  * ResumeFile command: u16 action=2, u16 size, RFLT data.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <vector>

#include "appwarrior/core/endian.h"
#include "hotline/protocol/constants.h"
#include "hotline/protocol/decode_error.h"
#include "hotline/protocol/payload.h"

namespace hotline::protocol {

// ---------------------------------------------------------------------------
// FILP — flat-file package
// ---------------------------------------------------------------------------

inline constexpr std::uint32_t kFlatFileFormat = aw::endian::four_cc('F', 'I', 'L', 'P');
inline constexpr std::uint32_t kForkTypeInfo = aw::endian::four_cc('I', 'N', 'F', 'O');
inline constexpr std::uint32_t kForkTypeData = aw::endian::four_cc('D', 'A', 'T', 'A');
inline constexpr std::size_t kFlatFileHeaderSize = 24;
inline constexpr std::size_t kFlatFileForkHeaderSize = 16;

struct FlatFileFork {
  std::uint32_t type = 0;
  std::uint32_t compression_type = 0;
  std::vector<std::byte> data;
};

struct FlatFile {
  std::uint16_t version = 0;
  std::vector<FlatFileFork> forks;
};

[[nodiscard]] auto encode_flat_file(const FlatFile& file)
    -> std::expected<std::vector<std::byte>, EncodeError>;
[[nodiscard]] auto try_decode_flat_file(std::span<const std::byte> bytes)
    -> std::expected<FlatFile, DecodeError>;

inline constexpr std::size_t kFlatFileInfoFixedSize = 72;

struct FlatFileInfo {
  std::uint32_t platform = 0;  // 'MWIN' / 'AMAC'
  std::uint32_t type_sig = 0;
  std::uint32_t creator_sig = 0;
  std::uint32_t flags = 0;
  std::uint32_t platform_flags = 0;
  std::array<std::byte, 32> reserved{};
  DateTimeStamp create_date;
  DateTimeStamp modify_date;
  std::uint16_t name_script = 0;
  std::string name;
  std::vector<std::byte> comment;
};

[[nodiscard]] auto encode_info_fork(const FlatFileInfo& info)
    -> std::expected<std::vector<std::byte>, EncodeError>;
[[nodiscard]] auto try_decode_info_fork(std::span<const std::byte> bytes)
    -> std::expected<FlatFileInfo, DecodeError>;

// ---------------------------------------------------------------------------
// RFLT — resume record
// ---------------------------------------------------------------------------

inline constexpr std::uint32_t kResumeDataFormat = aw::endian::four_cc('R', 'F', 'L', 'T');
inline constexpr std::size_t kResumeDataHeaderSize = 42;
inline constexpr std::size_t kResumeEntrySize = 16;

struct ResumeEntry {
  std::uint32_t fork = 0;
  std::uint32_t data_size = 0;
};

struct ResumeData {
  std::uint16_t version = 0;
  std::vector<ResumeEntry> entries;
};

[[nodiscard]] auto encode_resume_data(const ResumeData& resume)
    -> std::expected<std::vector<std::byte>, EncodeError>;
[[nodiscard]] auto try_decode_resume_data(std::span<const std::byte> bytes)
    -> std::expected<ResumeData, DecodeError>;

// The resume offset for the DATA fork, mirroring UFileSys::ResumeFlatten:
// the LAST 'DATA' entry's size (0 if none).
[[nodiscard]] auto data_resume_size(const ResumeData& resume) noexcept -> std::uint64_t;

// ---------------------------------------------------------------------------
// Folder download items and commands
// ---------------------------------------------------------------------------

struct FolderPathComponent {
  std::uint16_t script = 0;
  std::string name;  // <= 255 bytes on the wire
};

struct FolderDownloadItem {
  bool folder = false;
  std::vector<FolderPathComponent> path;  // root component excluded
};

[[nodiscard]] auto encode_folder_download_item(const FolderDownloadItem& item)
    -> std::expected<std::vector<std::byte>, EncodeError>;
[[nodiscard]] auto try_decode_folder_download_item(std::span<const std::byte> bytes)
    -> std::expected<FolderDownloadItem, DecodeError>;

[[nodiscard]] auto encode_folder_download_command(FolderDownloadAction action)
    -> std::array<std::byte, 2>;

// ResumeFile command: u16 action=2, u16 size, RFLT data.
[[nodiscard]] auto encode_folder_download_resume(const ResumeData& resume)
    -> std::expected<std::vector<std::byte>, EncodeError>;
[[nodiscard]] auto try_decode_folder_download_resume(std::span<const std::byte> bytes)
    -> std::expected<ResumeData, DecodeError>;

}  // namespace hotline::protocol
