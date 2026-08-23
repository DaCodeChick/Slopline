// Hotline wire protocol: field payload codecs.
//
// Explicit codecs for the structured data carried inside fields. Every
// layout below was verified against the legacy writer/reader code
// (HOTLINE_MODERNIZATION_REPORT.md §16; the exact legacy sites are cited
// per codec). Two of these carry historical endianness quirks that are
// preserved deliberately — see FileInfo and AccessMask.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "appwarrior/core/endian.h"
#include "hotline/protocol/constants.h"
#include "hotline/protocol/decode_error.h"

namespace hotline::protocol {

// ---------------------------------------------------------------------------
// FileInfo — myField_FileNameWithInfo (200)
//
// Legacy: SMyFileInfo in HotlineClientServerCommon.h:257-264, written by
// CMyApplication::BuildFileList (HotlineServ.cpp:2183-2220):
//   u32 type; u32 creator; u32 fileSize; u32 rsvd; u16 nameScript;
//   u16 nameSize; u8 nameData[nameSize];
//
// Endianness quirk (preserved for legacy-Intel peer interop): the writer
// TB()'d fileSize and nameSize (big-endian) but copied type/creator as raw
// host u32s, so Intel servers emit them little-endian — a FourCC like
// 'TEXT' appears byte-reversed on the wire from Windows peers. Mac peers
// emitted them big-endian. The modern codec follows the verified
// legacy-Intel convention (LE type/creator, BE everything else) and this
// asymmetry is part of the wire contract.
// ---------------------------------------------------------------------------

inline constexpr std::size_t kFileInfoHeaderSize = 20;  // 4 x u32 + 2 x u16

struct FileInfo {
  std::uint32_t type = 0;     // FourCC value (e.g. four_cc('T','E','X','T'))
  std::uint32_t creator = 0;  // FourCC value
  std::uint32_t file_size = 0;
  std::uint16_t name_script = 0;  // legacy writers always send 0
  std::string name;               // raw bytes (MacRoman historically)
};

[[nodiscard]] auto encode_file_info(const FileInfo& info) -> std::vector<std::byte>;
[[nodiscard]] auto try_decode_file_info(std::span<const std::byte> bytes)
    -> std::expected<FileInfo, DecodeError>;

// ---------------------------------------------------------------------------
// UserInfo — myField_UserNameWithInfo (300)
//
// Legacy: SMyUserInfo in Apps/Client/Source/Hotline.h:465-472, written by
// ProcessTran_GetUserNameList (HotlineServTrans.cpp:1475-1515):
//   u16 id; i16 iconID; u16 flags; u16 nameSize; u8 nameData[nameSize];
// all multi-byte fields TB()'d (big-endian).
// ---------------------------------------------------------------------------

inline constexpr std::size_t kUserInfoHeaderSize = 8;

struct UserInfo {
  std::uint16_t id = 0;
  std::int16_t icon_id = 0;
  std::uint16_t flags = 0;
  std::string name;  // raw bytes
};

[[nodiscard]] auto encode_user_info(const UserInfo& info) -> std::vector<std::byte>;
[[nodiscard]] auto try_decode_user_info(std::span<const std::byte> bytes)
    -> std::expected<UserInfo, DecodeError>;

// ---------------------------------------------------------------------------
// AccessMask — myField_UserAccess (110)
//
// Legacy: SMyUserAccess {Uint32 data[2]} (HotlineClientServerCommon.h),
// sent as a raw 8-byte struct copy (HotlineServTrans.cpp:3262; the client
// likewise — HotlineTasks.cpp:4988/5119). Privilege p is bit 7-(p%8) of
// wire byte p/8 (UMemory::SetBit order == aw::bits), which makes
// the WIRE BYTES host-independent: both the legacy Mac and Intel builds
// emitted identical bytes for the same privilege set. The historical
// "endianness hazard" only affects interpreting the two u32 VALUES across
// hosts — the modern mask avoids it entirely by modeling the mask as a
// 64-bit value read big-endian from the 8 wire bytes, where privilege p is
// bit (63-p). That reproduces the byte-level semantics exactly.
// ---------------------------------------------------------------------------

inline constexpr std::size_t kAccessMaskSize = 8;

class AccessMask {
 public:
  constexpr AccessMask() = default;

  [[nodiscard]] constexpr auto has(AccessPrivilege privilege) const noexcept -> bool {
    const unsigned index = static_cast<unsigned>(privilege);
    return ((bits_ >> (63U - index)) & 1U) != 0;
  }

  constexpr void set(AccessPrivilege privilege, bool value = true) noexcept {
    const unsigned index = static_cast<unsigned>(privilege);
    if (value) {
      bits_ |= (std::uint64_t{1} << (63U - index));
    } else {
      bits_ &= ~(std::uint64_t{1} << (63U - index));
    }
  }

  constexpr void clear() noexcept { bits_ = 0; }
  constexpr void fill() noexcept { bits_ = ~std::uint64_t{0}; }
  [[nodiscard]] constexpr auto bits() const noexcept -> std::uint64_t { return bits_; }

  friend constexpr auto operator==(const AccessMask&, const AccessMask&) -> bool = default;

  // Decoder friend: constructs a mask from its big-endian wire value.
  friend constexpr auto decode_access_mask(std::span<const std::byte, kAccessMaskSize> bytes) noexcept
      -> AccessMask;

 private:
  explicit constexpr AccessMask(std::uint64_t bits) : bits_(bits) {}

  std::uint64_t bits_ = 0;
};

static_assert(sizeof(AccessMask) == kAccessMaskSize);

constexpr void encode_access_mask(const AccessMask& mask,
                                  std::span<std::byte, kAccessMaskSize> out) noexcept {
  aw::endian::write_u64be(mask.bits(), out);
}

[[nodiscard]] constexpr auto decode_access_mask(
    std::span<const std::byte, kAccessMaskSize> bytes) noexcept -> AccessMask {
  // Privilege p is bit (63-p) of the big-endian 64-bit reading of the wire
  // bytes (see the AccessMask docs above).
  return AccessMask{aw::endian::read_u64be(bytes)};
}

[[nodiscard]] auto try_decode_access_mask(std::span<const std::byte> bytes)
    -> std::expected<AccessMask, DecodeError>;

// ---------------------------------------------------------------------------
// DateTimeStamp — myField_FileCreateDate (208) / myField_FileModifyDate (209)
//
// Legacy: SDateTimeStamp::Flatten (UDateTime(W).cpp:136-142):
//   u16 year; u16 msecs; u32 seconds — all big-endian, 8 bytes total.
// Semantics (UDateTime(W).cpp _DTSSysTimeToDTS): LOCAL time; `year` is the
// real calendar year, `seconds` counts from Jan 1 00:00:00 of that year,
// `msecs` is the millisecond fraction of the current second. This is the
// within-year convention the protocol code uses (report §16.5/§24.2).
// ---------------------------------------------------------------------------

inline constexpr std::size_t kDateTimeStampSize = 8;

struct DateTimeStamp {
  std::uint16_t year = 0;
  std::uint16_t msecs = 0;
  std::uint32_t seconds = 0;
};

static_assert(sizeof(DateTimeStamp) == kDateTimeStampSize);

constexpr void encode_date_time_stamp(const DateTimeStamp& stamp,
                                      std::span<std::byte, kDateTimeStampSize> out) noexcept {
  aw::endian::write_u16be(stamp.year, out.first<2>());
  aw::endian::write_u16be(stamp.msecs, out.subspan<2>().first<2>());
  aw::endian::write_u32be(stamp.seconds, out.subspan<4>().first<4>());
}

[[nodiscard]] constexpr auto decode_date_time_stamp(
    std::span<const std::byte, kDateTimeStampSize> bytes) noexcept -> DateTimeStamp {
  DateTimeStamp stamp;
  stamp.year = aw::endian::read_u16be(bytes.first<2>());
  stamp.msecs = aw::endian::read_u16be(bytes.subspan<2>().first<2>());
  stamp.seconds = aw::endian::read_u32be(bytes.subspan<4>().first<4>());
  return stamp;
}

[[nodiscard]] auto try_decode_date_time_stamp(std::span<const std::byte> bytes)
    -> std::expected<DateTimeStamp, DecodeError>;

// Note: news category GUIDs (myField_NewsCatGUID, 319) are framework-level —
// the type and its codec live in appwarrior/core/guid.h (aw::guid::Guid).

}  // namespace hotline::protocol
