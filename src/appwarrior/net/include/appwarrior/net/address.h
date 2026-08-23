// AppWarrior networking: IPv4 address.
//
// Hotline is an IPv4 protocol (its wire formats carry 4 raw address
// octets), so the framework address type is IPv4 + port. Internally the
// address is kept in network byte order — the form the wire uses.

#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

#include "appwarrior/net/net_error.h"

namespace aw::net {

class IpAddress {
 public:
  constexpr IpAddress() = default;
  constexpr IpAddress(std::uint8_t a, std::uint8_t b, std::uint8_t c, std::uint8_t d,
                      std::uint16_t port) noexcept
      : address_(static_cast<std::uint32_t>(a) << 24 | static_cast<std::uint32_t>(b) << 16 |
                 static_cast<std::uint32_t>(c) << 8 | static_cast<std::uint32_t>(d)),
        port_(port) {}

  // Parses "a.b.c.d" or "a.b.c.d:port" (no port defaults to 0). Strict:
  // each octet must be a decimal number 0..255.
  [[nodiscard]] static auto from_text(std::string_view text)
      -> std::expected<IpAddress, NetError>;

  [[nodiscard]] auto to_text() const -> std::string;  // "a.b.c.d:port"

  [[nodiscard]] constexpr auto octets() const noexcept -> std::array<std::uint8_t, 4> {
    return {static_cast<std::uint8_t>(address_ >> 24), static_cast<std::uint8_t>(address_ >> 16),
            static_cast<std::uint8_t>(address_ >> 8), static_cast<std::uint8_t>(address_)};
  }

  [[nodiscard]] constexpr auto port() const noexcept -> std::uint16_t { return port_; }

  // Network-byte-order u32 (the Hotline wire form).
  [[nodiscard]] constexpr auto network_address() const noexcept -> std::uint32_t {
    return address_;
  }

  friend constexpr auto operator==(const IpAddress&, const IpAddress&) -> bool = default;

 private:
  std::uint32_t address_ = 0;
  std::uint16_t port_ = 0;
};

}  // namespace aw::net
