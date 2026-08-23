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
// wire byte p/8 (UMemory::SetBit order == appwarrior::bits), which makes
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
  appwarrior::endian::write_u64be(mask.bits(), out);
}

[[nodiscard]] constexpr auto decode_access_mask(
    std::span<const std::byte, kAccessMaskSize> bytes) noexcept -> AccessMask {
  // Privilege p is bit (63-p) of the big-endian 64-bit reading of the wire
  // bytes (see the AccessMask docs above).
  return AccessMask{appwarrior::endian::read_u64be(bytes)};
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
  appwarrior::endian::write_u16be(stamp.year, out.first<2>());
  appwarrior::endian::write_u16be(stamp.msecs, out.subspan<2>().first<2>());
  appwarrior::endian::write_u32be(stamp.seconds, out.subspan<4>().first<4>());
}

[[nodiscard]] constexpr auto decode_date_time_stamp(
    std::span<const std::byte, kDateTimeStampSize> bytes) noexcept -> DateTimeStamp {
  DateTimeStamp stamp;
  stamp.year = appwarrior::endian::read_u16be(bytes.first<2>());
  stamp.msecs = appwarrior::endian::read_u16be(bytes.subspan<2>().first<2>());
  stamp.seconds = appwarrior::endian::read_u32be(bytes.subspan<4>().first<4>());
  return stamp;
}

[[nodiscard]] auto try_decode_date_time_stamp(std::span<const std::byte> bytes)
    -> std::expected<DateTimeStamp, DecodeError>;

// ---------------------------------------------------------------------------
// Guid — news category identifiers (myField_NewsCatGUID, 319)
//
// Legacy: SGUID (AppWarrior/Headers/UGUID.h) flattened by UGUID::Flatten
// (UGUID(W).cpp:63-75): time_low u32 BE, time_mid u16 BE,
// time_hi_and_version u16 BE, then clock_seq_hi_and_reserved,
// clock_seq_low and node[6] as raw bytes — 16 bytes total (the Microsoft
// UUID network form).
// ---------------------------------------------------------------------------

inline constexpr std::size_t kGuidSize = 16;

struct Guid {
  std::uint32_t time_low = 0;
  std::uint16_t time_mid = 0;
  std::uint16_t time_hi_and_version = 0;
  std::uint8_t clock_seq_hi_and_reserved = 0;
  std::uint8_t clock_seq_low = 0;
  std::array<std::uint8_t, 6> node{};

  friend constexpr auto operator==(const Guid&, const Guid&) -> bool = default;
};

static_assert(sizeof(Guid) == kGuidSize);

constexpr void encode_guid(const Guid& guid, std::span<std::byte, kGuidSize> out) noexcept {
  appwarrior::endian::write_u32be(guid.time_low, out.first<4>());
  appwarrior::endian::write_u16be(guid.time_mid, out.subspan<4>().first<2>());
  appwarrior::endian::write_u16be(guid.time_hi_and_version, out.subspan<6>().first<2>());
  out[8] = static_cast<std::byte>(guid.clock_seq_hi_and_reserved);
  out[9] = static_cast<std::byte>(guid.clock_seq_low);
  for (std::size_t i = 0; i < guid.node.size(); ++i) {
    out[10 + i] = static_cast<std::byte>(guid.node[i]);
  }
}

[[nodiscard]] constexpr auto decode_guid(std::span<const std::byte, kGuidSize> bytes) noexcept
    -> Guid {
  Guid guid;
  guid.time_low = appwarrior::endian::read_u32be(bytes.first<4>());
  guid.time_mid = appwarrior::endian::read_u16be(bytes.subspan<4>().first<2>());
  guid.time_hi_and_version = appwarrior::endian::read_u16be(bytes.subspan<6>().first<2>());
  guid.clock_seq_hi_and_reserved = std::to_integer<std::uint8_t>(bytes[8]);
  guid.clock_seq_low = std::to_integer<std::uint8_t>(bytes[9]);
  for (std::size_t i = 0; i < guid.node.size(); ++i) {
    guid.node[i] = std::to_integer<std::uint8_t>(bytes[10 + i]);
  }
  return guid;
}

[[nodiscard]] auto try_decode_guid(std::span<const std::byte> bytes)
    -> std::expected<Guid, DecodeError>;

}  // namespace hotline::protocol
