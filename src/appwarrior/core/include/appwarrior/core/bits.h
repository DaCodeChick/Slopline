// AppWarrior Core: bit-array access helpers.
//
// MSB-first within each byte, bytes in increasing address order: bit
// <index> lives in byte index/8 at bit position 7 - (index % 8). Bit 0 is
// the most significant bit of the first byte. This reproduces the
// historical UMemory::GetBit/SetBit/ClearBit/InvertBit semantics
// (legacy AppWarrior/Headers/UMemory.h:129-158) — preserved because the
// 64-bit SMyUserAccess privilege mask and other legacy bitfields use this
// order, and future codecs (Phase 3) must match it byte-for-byte.
//
// Trap (documented because it cost real archaeology): the historical
// UBitString helper used the OPPOSITE order — bit <index> = bit index%8 of
// byte index/8, i.e. LSB-first (legacy UBitString.cpp:22). Any bit-level
// wire/disk format must be checked against the specific helper its writer
// used (audit/01 §2.6; HOTLINE_MODERNIZATION_REPORT.md §7).
//
// Precondition: the span must hold at least (index/8)+1 bytes — the caller
// guarantees it, exactly as the legacy callers did.

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace aw::bits {

[[nodiscard]] constexpr auto get_bit(std::span<const std::byte> data,
                                     std::uint32_t index) noexcept -> bool {
  const std::uint32_t byte = std::to_integer<std::uint8_t>(data[index >> 3]);
  return ((byte >> (7U - (index & 7U))) & 1U) != 0;
}

constexpr void set_bit(std::span<std::byte> data, std::uint32_t index) noexcept {
  data[index >> 3] = static_cast<std::byte>(
    std::to_integer<std::uint8_t>(data[index >> 3]) |
    static_cast<std::uint8_t>(1U << (7U - (index & 7U))));
}

constexpr void clear_bit(std::span<std::byte> data, std::uint32_t index) noexcept {
  data[index >> 3] = static_cast<std::byte>(
    std::to_integer<std::uint8_t>(data[index >> 3]) &
    static_cast<std::uint8_t>(~(1U << (7U - (index & 7U)))));
}

constexpr void invert_bit(std::span<std::byte> data, std::uint32_t index) noexcept {
  data[index >> 3] = static_cast<std::byte>(
    std::to_integer<std::uint8_t>(data[index >> 3]) ^
    static_cast<std::uint8_t>(1U << (7U - (index & 7U))));
}

constexpr void set_bit(std::span<std::byte> data, std::uint32_t index, bool value) noexcept {
  if (value) {
    set_bit(data, index);
  } else {
    clear_bit(data, index);
  }
}

}  // namespace aw::bits
