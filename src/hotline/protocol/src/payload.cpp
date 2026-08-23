#include "hotline/protocol/payload.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "appwarrior/core/endian.h"

namespace hotline::protocol {

using aw::endian::read_u16be;
using aw::endian::read_u32be;
using aw::endian::read_u32le;
using aw::endian::read_u64be;
using aw::endian::write_u16be;
using aw::endian::write_u32be;
using aw::endian::write_u32le;
using aw::endian::write_u64be;

namespace {

// Shared plumbing for the {header bytes + Pascal-style name} payloads.
auto append_raw(std::vector<std::byte>& out, std::string_view text) -> void {
  for (const char character : text) {
    out.push_back(static_cast<std::byte>(static_cast<unsigned char>(character)));
  }
}

auto read_raw_name(std::span<const std::byte> name_bytes) -> std::string {
  std::string name;
  name.reserve(name_bytes.size());
  for (const std::byte byte : name_bytes) {
    name.push_back(static_cast<char>(std::to_integer<unsigned char>(byte)));
  }
  return name;
}

}  // namespace

auto encode_file_info(const FileInfo& info)
    -> std::expected<std::vector<std::byte>, EncodeError> {
  if (info.name.size() > kMaxFieldDataSize) {
    return std::unexpected(EncodeError::string_too_long);
  }

  std::vector<std::byte> out;
  out.reserve(kFileInfoHeaderSize + info.name.size());

  std::array<std::byte, 4> u32{};
  std::array<std::byte, 2> u16{};
  // Verified legacy-Intel convention: type/creator little-endian (raw host
  // copy), everything else big-endian (explicit TB()).
  write_u32le(info.type, u32);
  out.insert(out.end(), u32.begin(), u32.end());
  write_u32le(info.creator, u32);
  out.insert(out.end(), u32.begin(), u32.end());
  write_u32be(info.file_size, u32);
  out.insert(out.end(), u32.begin(), u32.end());
  write_u32be(0, u32);  // rsvd
  out.insert(out.end(), u32.begin(), u32.end());
  write_u16be(info.name_script, u16);
  out.insert(out.end(), u16.begin(), u16.end());
  write_u16be(static_cast<std::uint16_t>(info.name.size()), u16);
  out.insert(out.end(), u16.begin(), u16.end());

  append_raw(out, info.name);
  return out;
}

auto try_decode_file_info(std::span<const std::byte> bytes)
    -> std::expected<FileInfo, DecodeError> {
  if (bytes.size() < kFileInfoHeaderSize) {
    return std::unexpected(DecodeError::truncated);
  }
  const std::uint16_t name_size = read_u16be(bytes.subspan(18).first<2>());
  const std::size_t total = kFileInfoHeaderSize + static_cast<std::size_t>(name_size);
  if (bytes.size() < total) {
    return std::unexpected(DecodeError::truncated);
  }
  if (bytes.size() > total) {
    return std::unexpected(DecodeError::trailing_bytes);
  }

  FileInfo info;
  info.type = read_u32le(bytes.first<4>());
  info.creator = read_u32le(bytes.subspan(4).first<4>());
  info.file_size = read_u32be(bytes.subspan(8).first<4>());
  // bytes 12..16: rsvd, ignored.
  info.name_script = read_u16be(bytes.subspan(16).first<2>());
  info.name = read_raw_name(bytes.subspan(kFileInfoHeaderSize, name_size));
  return info;
}

auto encode_user_info(const UserInfo& info)
    -> std::expected<std::vector<std::byte>, EncodeError> {
  if (info.name.size() > kMaxFieldDataSize) {
    return std::unexpected(EncodeError::string_too_long);
  }

  std::vector<std::byte> out;
  out.reserve(kUserInfoHeaderSize + info.name.size());

  std::array<std::byte, 2> u16{};
  write_u16be(info.id, u16);
  out.insert(out.end(), u16.begin(), u16.end());
  write_u16be(static_cast<std::uint16_t>(info.icon_id), u16);
  out.insert(out.end(), u16.begin(), u16.end());
  write_u16be(info.flags, u16);
  out.insert(out.end(), u16.begin(), u16.end());
  write_u16be(static_cast<std::uint16_t>(info.name.size()), u16);
  out.insert(out.end(), u16.begin(), u16.end());

  append_raw(out, info.name);
  return out;
}

auto try_decode_user_info(std::span<const std::byte> bytes)
    -> std::expected<UserInfo, DecodeError> {
  if (bytes.size() < kUserInfoHeaderSize) {
    return std::unexpected(DecodeError::truncated);
  }
  const std::uint16_t name_size = read_u16be(bytes.subspan(6).first<2>());
  const std::size_t total = kUserInfoHeaderSize + static_cast<std::size_t>(name_size);
  if (bytes.size() < total) {
    return std::unexpected(DecodeError::truncated);
  }
  if (bytes.size() > total) {
    return std::unexpected(DecodeError::trailing_bytes);
  }

  UserInfo info;
  info.id = read_u16be(bytes.first<2>());
  info.icon_id = static_cast<std::int16_t>(read_u16be(bytes.subspan(2).first<2>()));
  info.flags = read_u16be(bytes.subspan(4).first<2>());
  info.name = read_raw_name(bytes.subspan(kUserInfoHeaderSize, name_size));
  return info;
}

auto try_decode_access_mask(std::span<const std::byte> bytes)
    -> std::expected<AccessMask, DecodeError> {
  if (bytes.size() < kAccessMaskSize) {
    return std::unexpected(DecodeError::truncated);
  }
  if (bytes.size() > kAccessMaskSize) {
    return std::unexpected(DecodeError::trailing_bytes);
  }
  return decode_access_mask(bytes.first<kAccessMaskSize>());
}

auto try_decode_date_time_stamp(std::span<const std::byte> bytes)
    -> std::expected<DateTimeStamp, DecodeError> {
  if (bytes.size() < kDateTimeStampSize) {
    return std::unexpected(DecodeError::truncated);
  }
  if (bytes.size() > kDateTimeStampSize) {
    return std::unexpected(DecodeError::trailing_bytes);
  }
  return decode_date_time_stamp(bytes.first<kDateTimeStampSize>());
}

}  // namespace hotline::protocol
