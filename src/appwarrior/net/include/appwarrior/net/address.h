// AppWarrior networking: IP address (IPv4 and IPv6).
//
// Hotline's WIRE formats are IPv4-only (tracker entries carry 4 raw
// octets), so the protocol codecs keep their own octet handling — this
// transport-level address type supports both families: IPv6 hosts may
// serve IPv4-only Hotline traffic via dual-stack sockets and v4-mapped
// addresses. Internally addresses are kept in network byte order.

#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

#include "appwarrior/export.h"
#include "appwarrior/net/net_error.h"

namespace aw::net {

class AW_API IpAddress {
 public:
  enum class Family : std::uint8_t { ipv4, ipv6 };

  constexpr IpAddress() = default;
  constexpr IpAddress(std::uint8_t a, std::uint8_t b, std::uint8_t c, std::uint8_t d,
                      std::uint16_t port) noexcept
      : family_(Family::ipv4),
        bytes_{a, b, c, d},
        port_(port),
        scope_id_(0) {}

  static constexpr auto ipv6(std::array<std::uint8_t, 16> bytes, std::uint16_t port,
                             std::uint32_t scope_id = 0) noexcept -> IpAddress {
    IpAddress address;
    address.family_ = Family::ipv6;
    address.bytes_ = bytes;
    address.port_ = port;
    address.scope_id_ = scope_id;
    return address;
  }

  // Parses "a.b.c.d", "a.b.c.d:port", "::1", "[::1]:port", or a bare
  // IPv6 address without port. Strict: octets 0..255, hex groups 1..4
  // digits, at most one "::" compression.
  [[nodiscard]] AW_API static auto from_text(std::string_view text)
      -> std::expected<IpAddress, NetError>;

  // "a.b.c.d:port" or "[groups]:port" (IPv6 always bracketed when a port
  // is present).
  [[nodiscard]] AW_API auto to_text() const -> std::string;

  [[nodiscard]] constexpr auto family() const noexcept -> Family { return family_; }

  // IPv4 only (precondition): the four octets, first octet first.
  [[nodiscard]] constexpr auto octets() const noexcept -> std::array<std::uint8_t, 4> {
    return {bytes_[0], bytes_[1], bytes_[2], bytes_[3]};
  }

  // IPv6 only (precondition): the 16 octets, network order.
  [[nodiscard]] constexpr auto ipv6_bytes() const noexcept -> std::array<std::uint8_t, 16> {
    return bytes_;
  }

  [[nodiscard]] constexpr auto port() const noexcept -> std::uint16_t { return port_; }

  // IPv6 link-local scope id (0 otherwise).
  [[nodiscard]] constexpr auto scope_id() const noexcept -> std::uint32_t { return scope_id_; }

  // Network-byte-order u32 of the IPv4 address (the Hotline wire form).
  [[nodiscard]] constexpr auto network_address() const noexcept -> std::uint32_t {
    return static_cast<std::uint32_t>(bytes_[0]) << 24 |
           static_cast<std::uint32_t>(bytes_[1]) << 16 |
           static_cast<std::uint32_t>(bytes_[2]) << 8 |
           static_cast<std::uint32_t>(bytes_[3]);
  }

  friend constexpr auto operator==(const IpAddress&, const IpAddress&) -> bool = default;

 private:
  Family family_ = Family::ipv4;
  std::array<std::uint8_t, 16> bytes_{};
  std::uint16_t port_ = 0;
  std::uint32_t scope_id_ = 0;
};

}  // namespace aw::net
