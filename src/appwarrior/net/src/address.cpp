#include "appwarrior/net/address.h"

#include <charconv>
#include <cstdint>
#include <format>

namespace aw::net {

auto IpAddress::from_text(std::string_view text) -> std::expected<IpAddress, NetError> {
  // Split off the optional ":port" suffix.
  std::string_view host = text;
  std::uint32_t port = 0;
  if (const std::size_t colon = text.rfind(':'); colon != std::string_view::npos) {
    host = text.substr(0, colon);
    const std::string_view port_text = text.substr(colon + 1);
    if (port_text.empty() || port_text.size() > 5) {
      return std::unexpected(NetError::invalid_argument);
    }
    const auto [ptr, error] = std::from_chars(port_text.data(), port_text.data() + port_text.size(), port);
    if (error != std::errc{} || ptr != port_text.data() + port_text.size() || port > 65535) {
      return std::unexpected(NetError::invalid_argument);
    }
  }

  // Four dotted decimal octets.
  std::array<std::uint8_t, 4> octets{};
  std::size_t octet = 0;
  std::size_t value = 0;
  bool in_number = false;
  for (const char character : host) {
    if (character == '.') {
      if (!in_number || octet >= 3 || value > 255) {
        return std::unexpected(NetError::invalid_argument);
      }
      octets[octet] = static_cast<std::uint8_t>(value);
      ++octet;
      value = 0;
      in_number = false;
    } else if (character >= '0' && character <= '9') {
      in_number = true;
      value = value * 10 + static_cast<std::size_t>(character - '0');
      if (value > 255) {
        return std::unexpected(NetError::invalid_argument);
      }
    } else {
      return std::unexpected(NetError::invalid_argument);
    }
  }
  if (!in_number || octet != 3) {
    return std::unexpected(NetError::invalid_argument);
  }
  octets[3] = static_cast<std::uint8_t>(value);

  return IpAddress{octets[0], octets[1], octets[2], octets[3], static_cast<std::uint16_t>(port)};
}

auto IpAddress::to_text() const -> std::string {
  const std::array<std::uint8_t, 4> bytes = octets();
  return std::format("{}.{}.{}.{}:{}", bytes[0], bytes[1], bytes[2], bytes[3], port_);
}

}  // namespace aw::net
