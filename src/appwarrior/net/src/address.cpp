#include "appwarrior/net/address.h"

#include <array>
#include <charconv>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>

namespace aw::net {

namespace {

// Parses a 16-bit decimal port. Returns false on any malformed input.
auto parse_port(std::string_view text, std::uint32_t& out) noexcept -> bool {
  if (text.empty() || text.size() > 5) {
    return false;
  }
  const auto [ptr, error] = std::from_chars(text.data(), text.data() + text.size(), out);
  return error == std::errc{} && ptr == text.data() + text.size() && out <= 65535;
}

// Parses a dotted-decimal IPv4 host into the four octets.
auto parse_ipv4_host(std::string_view host, std::array<std::uint8_t, 4>& octets) noexcept
    -> bool {
  std::size_t octet = 0;
  std::size_t value = 0;
  bool in_number = false;
  for (const char character : host) {
    if (character == '.') {
      if (!in_number || octet >= 3) {
        return false;
      }
      octets[octet] = static_cast<std::uint8_t>(value);
      ++octet;
      value = 0;
      in_number = false;
    } else if (character >= '0' && character <= '9') {
      in_number = true;
      value = value * 10 + static_cast<std::size_t>(character - '0');
      if (value > 255) {
        return false;
      }
    } else {
      return false;
    }
  }
  if (!in_number || octet != 3) {
    return false;
  }
  octets[3] = static_cast<std::uint8_t>(value);
  return true;
}

auto hex_value(char digit) noexcept -> int {
  if (digit >= '0' && digit <= '9') {
    return digit - '0';
  }
  if (digit >= 'a' && digit <= 'f') {
    return digit - 'a' + 10;
  }
  if (digit >= 'A' && digit <= 'F') {
    return digit - 'A' + 10;
  }
  return -1;
}

// Parses a bare IPv6 host into 16 octets, expanding "::" once.
auto parse_ipv6_host(std::string_view host, std::array<std::uint8_t, 16>& bytes) noexcept
    -> bool {
  // Split into head and tail around "::" (at most one occurrence).
  const std::size_t compression = host.find("::");
  std::string_view head = compression == std::string_view::npos ? std::string_view{}
                                                                 : host.substr(0, compression);
  std::string_view tail = compression == std::string_view::npos ? std::string_view{}
                                                                 : host.substr(compression + 2);

  const auto parse_groups = [](std::string_view part, std::array<std::uint16_t, 8>& groups,
                               std::size_t& count) -> bool {
    if (part.empty()) {
      return true;
    }
    std::size_t start = 0;
    for (;;) {
      const std::size_t colon = part.find(':', start);
      const std::string_view group = part.substr(start, colon == std::string_view::npos
                                                              ? std::string_view::npos
                                                              : colon - start);
      if (group.empty() || group.size() > 4) {
        return false;
      }
      if (count >= groups.size()) {
        return false;
      }
      std::uint16_t value = 0;
      for (const char digit : group) {
        const int nibble = hex_value(digit);
        if (nibble < 0) {
          return false;
        }
        value = static_cast<std::uint16_t>((value << 4) | static_cast<std::uint16_t>(nibble));
      }
      groups[count] = value;
      ++count;
      if (colon == std::string_view::npos) {
        return true;
      }
      start = colon + 1;
      if (start >= part.size()) {
        return false;  // trailing colon without compression
      }
    }
  };

  std::array<std::uint16_t, 8> head_groups{};
  std::array<std::uint16_t, 8> tail_groups{};
  std::size_t head_count = 0;
  std::size_t tail_count = 0;

  if (compression == std::string_view::npos) {
    // No compression: exactly 8 groups.
    if (!parse_groups(host, head_groups, head_count) || head_count != 8) {
      return false;
    }
    for (std::size_t i = 0; i < 8; ++i) {
      bytes[i * 2] = static_cast<std::uint8_t>(head_groups[i] >> 8);
      bytes[i * 2 + 1] = static_cast<std::uint8_t>(head_groups[i] & 0xFFU);
    }
    return true;
  }

  if (!parse_groups(head, head_groups, head_count) ||
      !parse_groups(tail, tail_groups, tail_count)) {
    return false;
  }
  if (head_count + tail_count > 7) {
    return false;
  }

  const std::size_t zero_groups = 8 - head_count - tail_count;
  std::size_t at = 0;
  const auto emit = [&](std::uint16_t group) {
    bytes[at * 2] = static_cast<std::uint8_t>(group >> 8);
    bytes[at * 2 + 1] = static_cast<std::uint8_t>(group & 0xFFU);
    ++at;
  };
  for (std::size_t i = 0; i < head_count; ++i) {
    emit(head_groups[i]);
  }
  for (std::size_t i = 0; i < zero_groups; ++i) {
    emit(0);
  }
  for (std::size_t i = 0; i < tail_count; ++i) {
    emit(tail_groups[i]);
  }
  return true;
}

}  // namespace

auto IpAddress::from_text(std::string_view text) -> std::expected<IpAddress, NetError> {
  // Bracketed IPv6 with optional port.
  if (text.starts_with('[')) {
    const std::size_t close = text.find(']');
    if (close == std::string_view::npos) {
      return std::unexpected(NetError::invalid_argument);
    }
    const std::string_view host = text.substr(1, close - 1);
    const std::string_view remainder = text.substr(close + 1);

    std::uint32_t port = 0;
    if (!remainder.empty()) {
      if (!remainder.starts_with(':') || !parse_port(remainder.substr(1), port)) {
        return std::unexpected(NetError::invalid_argument);
      }
    }

    std::array<std::uint8_t, 16> bytes{};
    if (!parse_ipv6_host(host, bytes)) {
      return std::unexpected(NetError::invalid_argument);
    }
    return ipv6(bytes, static_cast<std::uint16_t>(port));
  }

  // Split off an optional ":port" suffix (IPv4 and bare IPv6 without port).
  const std::size_t colon = text.rfind(':');
  std::string_view host = text;
  std::uint32_t port = 0;
  if (colon != std::string_view::npos) {
    const std::string_view suffix = text.substr(colon + 1);
    // Treat the suffix as a port only when it is all digits AND the host
    // before it contains no other ':' (IPv4, or a bracketed IPv6 handled
    // above). Bare IPv6 with a port must use brackets.
    if (!suffix.empty() && suffix.find_first_not_of("0123456789") == std::string_view::npos &&
        text.find(':') == colon) {
      if (!parse_port(suffix, port)) {
        return std::unexpected(NetError::invalid_argument);
      }
      host = text.substr(0, colon);
    }
  }

  std::array<std::uint8_t, 4> octets{};
  if (host.find('.') != std::string_view::npos) {
    if (!parse_ipv4_host(host, octets)) {
      return std::unexpected(NetError::invalid_argument);
    }
    return IpAddress{octets[0], octets[1], octets[2], octets[3],
                     static_cast<std::uint16_t>(port)};
  }

  std::array<std::uint8_t, 16> bytes{};
  if (!parse_ipv6_host(host, bytes)) {
    return std::unexpected(NetError::invalid_argument);
  }
  return ipv6(bytes, static_cast<std::uint16_t>(port));
}

auto IpAddress::to_text() const -> std::string {
  if (family_ == Family::ipv4) {
    return std::format("{}.{}.{}.{}:{}", bytes_[0], bytes_[1], bytes_[2], bytes_[3], port_);
  }

  // IPv6: groups with the longest zero run compressed to "::".
  std::array<std::uint16_t, 8> groups{};
  for (std::size_t i = 0; i < 8; ++i) {
    groups[i] = static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes_[i * 2]) << 8) |
                static_cast<std::uint16_t>(bytes_[i * 2 + 1]);
  }
  std::size_t best_start = 0;
  std::size_t best_length = 0;
  std::size_t run_start = 0;
  while (run_start < 8) {
    if (groups[run_start] != 0) {
      ++run_start;
      continue;
    }
    std::size_t run_length = 0;
    while (run_start + run_length < 8 && groups[run_start + run_length] == 0) {
      ++run_length;
    }
    if (run_length > best_length) {
      best_start = run_start;
      best_length = run_length;
    }
    run_start += run_length;
  }
  if (best_length < 2) {
    best_start = 8;
    best_length = 0;
  }

  std::string text;
  text.reserve(48);
  for (std::size_t i = 0; i < 8; ++i) {
    if (i == best_start) {
      // The run always collapses to "::" (separators are emitted BEFORE
      // each group, so nothing trails the previous group).
      text += "::";
      i += best_length - 1;
      continue;
    }
    // No separator at the very start, and none directly after the
    // compressed run (the "::" already provides it).
    if (i != 0 && i != best_start + best_length) {
      text += ':';
    }
    text += std::format("{:x}", groups[i]);
  }

  return std::format("[{}]:{}", text, port_);
}

}  // namespace aw::net
