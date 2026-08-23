// AppWarrior Core: big-endian encoding helpers.
//
// General-purpose byte-order primitives shared by Hotline and every
// AppWarrior binary-format codec. Hotline's history includes big-endian
// Macintosh systems, so the wire protocol, news database, user records and
// resource formats all use big-endian integers; the historical code
// serialized them via TB()/FB() byte swaps on little-endian hosts. These
// helpers are byte-shift based and therefore independent of the host's byte
// order by construction — never replace them with memcpy-of-native-int
// tricks (AGENTS.md: explicit codecs, centralized byte-order handling).
//
// `four_cc` lives here too: it encodes a FourCC tag ('TRTP', 'HLNZ',
// 'AWRZ', ...) as the big-endian u32 it appears as on the wire/disk.

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace appwarrior::endian {

[[nodiscard]] constexpr auto read_u16be(std::span<const std::byte, 2> bytes) noexcept
    -> std::uint16_t {
  return static_cast<std::uint16_t>(
    (static_cast<std::uint16_t>(bytes[0]) << 8) | static_cast<std::uint16_t>(bytes[1]));
}

[[nodiscard]] constexpr auto read_u32be(std::span<const std::byte, 4> bytes) noexcept
    -> std::uint32_t {
  return (static_cast<std::uint32_t>(bytes[0]) << 24) |
         (static_cast<std::uint32_t>(bytes[1]) << 16) |
         (static_cast<std::uint32_t>(bytes[2]) << 8) |
         static_cast<std::uint32_t>(bytes[3]);
}

constexpr void write_u16be(std::uint16_t value, std::span<std::byte, 2> out) noexcept {
  out[0] = static_cast<std::byte>((value >> 8) & 0xFFU);
  out[1] = static_cast<std::byte>(value & 0xFFU);
}

constexpr void write_u32be(std::uint32_t value, std::span<std::byte, 4> out) noexcept {
  out[0] = static_cast<std::byte>((value >> 24) & 0xFFU);
  out[1] = static_cast<std::byte>((value >> 16) & 0xFFU);
  out[2] = static_cast<std::byte>((value >> 8) & 0xFFU);
  out[3] = static_cast<std::byte>(value & 0xFFU);
}

// FourCC tag as a big-endian 32-bit integer.
// e.g. four_cc('T','R','T','P') == 0x54525450.
[[nodiscard]] constexpr auto four_cc(char c0, char c1, char c2, char c3) noexcept -> std::uint32_t {
  return (static_cast<std::uint32_t>(static_cast<unsigned char>(c0)) << 24) |
         (static_cast<std::uint32_t>(static_cast<unsigned char>(c1)) << 16) |
         (static_cast<std::uint32_t>(static_cast<unsigned char>(c2)) << 8) |
         static_cast<std::uint32_t>(static_cast<unsigned char>(c3));
}

}  // namespace appwarrior::endian
