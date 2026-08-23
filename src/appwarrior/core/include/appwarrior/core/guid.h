// AppWarrior Core: GUID (Microsoft UUID network form).
//
// General-purpose identifier — moved out of the Hotline protocol layer
// because it is not Hotline-specific (news categories merely happen to use
// it). Layout and codec reproduce the historical SGUID + UGUID::Flatten
// (legacy AppWarrior/Headers/UGUID.h, Source/Misc/UGUID(W).cpp:63-75):
// time_low u32 BE, time_mid u16 BE, time_hi_and_version u16 BE, then
// clock_seq_hi_and_reserved, clock_seq_low and node[6] as raw bytes —
// 16 bytes total.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

#include "appwarrior/core/decode_error.h"
#include "appwarrior/core/endian.h"

namespace aw::guid {

inline constexpr std::size_t kSize = 16;

struct Guid {
  std::uint32_t time_low = 0;
  std::uint16_t time_mid = 0;
  std::uint16_t time_hi_and_version = 0;
  std::uint8_t clock_seq_hi_and_reserved = 0;
  std::uint8_t clock_seq_low = 0;
  std::array<std::uint8_t, 6> node{};

  friend constexpr auto operator==(const Guid&, const Guid&) -> bool = default;
};

static_assert(sizeof(Guid) == kSize);

constexpr void encode(const Guid& guid, std::span<std::byte, kSize> out) noexcept {
  aw::endian::write_u32be(guid.time_low, out.first<4>());
  aw::endian::write_u16be(guid.time_mid, out.subspan<4>().first<2>());
  aw::endian::write_u16be(guid.time_hi_and_version, out.subspan<6>().first<2>());
  out[8] = static_cast<std::byte>(guid.clock_seq_hi_and_reserved);
  out[9] = static_cast<std::byte>(guid.clock_seq_low);
  for (std::size_t i = 0; i < guid.node.size(); ++i) {
    out[10 + i] = static_cast<std::byte>(guid.node[i]);
  }
}

[[nodiscard]] constexpr auto decode(std::span<const std::byte, kSize> bytes) noexcept -> Guid {
  Guid guid;
  guid.time_low = aw::endian::read_u32be(bytes.first<4>());
  guid.time_mid = aw::endian::read_u16be(bytes.subspan<4>().first<2>());
  guid.time_hi_and_version = aw::endian::read_u16be(bytes.subspan<6>().first<2>());
  guid.clock_seq_hi_and_reserved = std::to_integer<std::uint8_t>(bytes[8]);
  guid.clock_seq_low = std::to_integer<std::uint8_t>(bytes[9]);
  for (std::size_t i = 0; i < guid.node.size(); ++i) {
    guid.node[i] = std::to_integer<std::uint8_t>(bytes[10 + i]);
  }
  return guid;
}

[[nodiscard]] inline auto try_decode(std::span<const std::byte> bytes)
    -> std::expected<Guid, DecodeError> {
  if (bytes.size() < kSize) {
    return std::unexpected(DecodeError::truncated);
  }
  if (bytes.size() > kSize) {
    return std::unexpected(DecodeError::trailing_bytes);
  }
  return decode(bytes.first<kSize>());
}

}  // namespace aw::guid
