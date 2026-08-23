#include "hotline/protocol/tracker.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "appwarrior/core/endian.h"
#include "hotline/protocol/constants.h"

namespace hotline::protocol {

using aw::endian::read_u16be;
using aw::endian::read_u32be;
using aw::endian::write_u16be;
using aw::endian::write_u32be;

namespace {

auto append_raw(std::vector<std::byte>& out, std::string_view text) -> void {
  for (const char character : text) {
    out.push_back(static_cast<std::byte>(static_cast<unsigned char>(character)));
  }
}

auto read_raw_name(std::span<const std::byte> bytes) -> std::string {
  std::string text;
  text.reserve(bytes.size());
  for (const std::byte byte : bytes) {
    text.push_back(static_cast<char>(std::to_integer<unsigned char>(byte)));
  }
  return text;
}

void append_u16(std::vector<std::byte>& out, std::uint16_t value) {
  std::array<std::byte, 2> bytes{};
  write_u16be(value, bytes);
  out.insert(out.end(), bytes.begin(), bytes.end());
}

void append_u32(std::vector<std::byte>& out, std::uint32_t value) {
  std::array<std::byte, 4> bytes{};
  write_u32be(value, bytes);
  out.insert(out.end(), bytes.begin(), bytes.end());
}

// Precondition (checked by the encode functions): text.size() <= 255.
void append_pstring(std::vector<std::byte>& out, std::string_view text) {
  out.push_back(static_cast<std::byte>(text.size()));
  append_raw(out, text);
}

[[nodiscard]] auto read_pstring(std::span<const std::byte> bytes, std::size_t& consumed)
    -> std::expected<std::string, DecodeError> {
  if (bytes.empty()) {
    return std::unexpected(DecodeError::truncated);
  }
  const std::size_t length = std::to_integer<std::uint8_t>(bytes[0]);
  if (bytes.size() < length + 1) {
    return std::unexpected(DecodeError::truncated);
  }
  consumed = length + 1;
  return read_raw_name(bytes.subspan(1, length));
}

// Precondition (checked by the encode functions): name/description <= 255 bytes.
void append_server_entry(std::vector<std::byte>& out, const TrackerServerEntry& entry) {
  for (const std::uint8_t octet : entry.address) {
    out.push_back(static_cast<std::byte>(octet));
  }
  append_u16(out, entry.port);
  append_u16(out, entry.user_count);
  append_u16(out, entry.flags);
  append_pstring(out, entry.name);
  append_pstring(out, entry.description);
}

[[nodiscard]] auto read_server_entry(std::span<const std::byte> bytes, std::size_t& consumed)
    -> std::expected<TrackerServerEntry, DecodeError> {
  if (bytes.size() < 12) {  // 4 + 2 + 2 + 2 + 1 + 1
    return std::unexpected(DecodeError::truncated);
  }
  TrackerServerEntry entry;
  for (std::size_t i = 0; i < 4; ++i) {
    entry.address[i] = std::to_integer<std::uint8_t>(bytes[i]);
  }
  entry.port = read_u16be(bytes.subspan(4).first<2>());
  entry.user_count = read_u16be(bytes.subspan(6).first<2>());
  entry.flags = read_u16be(bytes.subspan(8).first<2>());
  std::size_t name_consumed = 0;
  std::size_t desc_consumed = 0;
  auto name = read_pstring(bytes.subspan(10), name_consumed);
  if (!name.has_value()) {
    return std::unexpected(name.error());
  }
  auto description = read_pstring(bytes.subspan(10 + name_consumed), desc_consumed);
  if (!description.has_value()) {
    return std::unexpected(description.error());
  }
  entry.name = std::move(*name);
  entry.description = std::move(*description);
  consumed = 10 + name_consumed + desc_consumed;
  return entry;
}

}  // namespace

auto encode_tracker_registration(std::uint16_t type, const TrackerRegistration& registration)
    -> std::expected<std::vector<std::byte>, EncodeError> {
  if (registration.name.size() > 255 || registration.description.size() > 255 ||
      registration.password.size() > 255) {
    return std::unexpected(EncodeError::string_too_long);
  }

  std::vector<std::byte> out;
  append_u16(out, type);
  append_u16(out, registration.port);
  append_u16(out, registration.user_count);
  append_u16(out, registration.flags);
  append_u32(out, registration.pass_id);
  append_pstring(out, registration.name);
  append_pstring(out, registration.description);
  append_pstring(out, registration.password);
  return out;
}

auto try_decode_tracker_registration(std::span<const std::byte> bytes)
    -> std::expected<TrackerRegistrationMessage, DecodeError> {
  // Legacy guard: dataSize > 14.
  if (bytes.size() <= 14) {
    return std::unexpected(DecodeError::truncated);
  }

  TrackerRegistrationMessage message;
  message.type = read_u16be(bytes.first<2>());
  if (message.type != kRegistrationTypeAdd && message.type != kRegistrationTypeRemove) {
    return std::unexpected(DecodeError::wrong_format_tag);
  }
  message.registration.port = read_u16be(bytes.subspan(2).first<2>());
  message.registration.user_count = read_u16be(bytes.subspan(4).first<2>());
  message.registration.flags = read_u16be(bytes.subspan(6).first<2>());
  message.registration.pass_id = read_u32be(bytes.subspan(8).first<4>());

  std::size_t consumed = 0;
  std::size_t total = 12;
  auto name = read_pstring(bytes.subspan(total), consumed);
  if (!name.has_value()) {
    return std::unexpected(name.error());
  }
  message.registration.name = std::move(*name);
  total += consumed;
  auto description = read_pstring(bytes.subspan(total), consumed);
  if (!description.has_value()) {
    return std::unexpected(description.error());
  }
  message.registration.description = std::move(*description);
  total += consumed;
  auto password = read_pstring(bytes.subspan(total), consumed);
  if (!password.has_value()) {
    return std::unexpected(password.error());
  }
  message.registration.password = std::move(*password);
  total += consumed;

  if (total != bytes.size()) {
    return std::unexpected(DecodeError::trailing_bytes);
  }
  return message;
}

auto encode_tracker_handshake(std::uint16_t version, std::string_view login,
                              std::string_view password) -> std::vector<std::byte> {
  std::vector<std::byte> out;
  out.reserve(kTrackerHandshakeSizeV2);
  append_u32(out, kTrackerProtocolTag);
  append_u16(out, version);

  if (version == kTrackerHandshakeVersion2) {
    // 32-byte zero-padded p-strings, clamped to 31 characters.
    const std::size_t login_size = std::min<std::size_t>(login.size(), 31);
    const std::size_t password_size = std::min<std::size_t>(password.size(), 31);
    std::array<std::byte, kTrackerLoginFieldSize> field{};
    field[0] = static_cast<std::byte>(login_size);
    for (std::size_t i = 0; i < login_size; ++i) {
      field[1 + i] = static_cast<std::byte>(static_cast<unsigned char>(login[i]));
    }
    out.insert(out.end(), field.begin(), field.end());
    field.fill(std::byte{0});
    field[0] = static_cast<std::byte>(password_size);
    for (std::size_t i = 0; i < password_size; ++i) {
      field[1 + i] = static_cast<std::byte>(static_cast<unsigned char>(password[i]));
    }
    out.insert(out.end(), field.begin(), field.end());
  }
  return out;
}

auto try_decode_tracker_handshake(std::span<const std::byte> bytes)
    -> std::expected<TrackerHandshake, DecodeError> {
  if (bytes.size() < kTrackerHandshakeSizeV1) {
    return std::unexpected(DecodeError::truncated);
  }
  if (read_u32be(bytes.first<4>()) != kTrackerProtocolTag) {
    return std::unexpected(DecodeError::wrong_format_tag);
  }

  TrackerHandshake handshake;
  handshake.version = read_u16be(bytes.subspan(4).first<2>());
  if (handshake.version == kTrackerHandshakeVersion1) {
    if (bytes.size() != kTrackerHandshakeSizeV1) {
      return std::unexpected(DecodeError::trailing_bytes);
    }
    return handshake;
  }
  if (handshake.version != kTrackerHandshakeVersion2) {
    return std::unexpected(DecodeError::unsupported_version);
  }
  if (bytes.size() < kTrackerHandshakeSizeV2) {
    return std::unexpected(DecodeError::truncated);
  }
  if (bytes.size() > kTrackerHandshakeSizeV2) {
    return std::unexpected(DecodeError::trailing_bytes);
  }

  const auto parse_field = [](std::span<const std::byte> field)
      -> std::expected<std::string, DecodeError> {
    const std::size_t length = std::to_integer<std::uint8_t>(field[0]);
    if (length > 31) {
      return std::unexpected(DecodeError::truncated);
    }
    return read_raw_name(field.subspan(1, length));
  };

  auto login = parse_field(bytes.subspan(6, kTrackerLoginFieldSize));
  if (!login.has_value()) {
    return std::unexpected(login.error());
  }
  auto password = parse_field(bytes.subspan(6 + kTrackerLoginFieldSize, kTrackerLoginFieldSize));
  if (!password.has_value()) {
    return std::unexpected(password.error());
  }
  handshake.login = std::move(*login);
  handshake.password = std::move(*password);
  return handshake;
}

auto encode_tracker_handshake_reply(std::uint16_t version)
    -> std::array<std::byte, kTrackerHandshakeSizeV1> {
  std::array<std::byte, kTrackerHandshakeSizeV1> out{};
  write_u32be(kTrackerProtocolTag, std::span<std::byte, kTrackerHandshakeSizeV1>(out).first<4>());
  write_u16be(version, std::span<std::byte, kTrackerHandshakeSizeV1>(out).subspan<4>().first<2>());
  return out;
}

auto try_decode_tracker_handshake_reply(std::span<const std::byte> bytes)
    -> std::expected<std::uint16_t, DecodeError> {
  if (bytes.size() < kTrackerHandshakeSizeV1) {
    return std::unexpected(DecodeError::truncated);
  }
  if (bytes.size() > kTrackerHandshakeSizeV1) {
    return std::unexpected(DecodeError::trailing_bytes);
  }
  if (read_u32be(bytes.first<4>()) != kTrackerProtocolTag) {
    return std::unexpected(DecodeError::wrong_format_tag);
  }
  return read_u16be(bytes.subspan(4).first<2>());
}

auto encode_tracker_server_list(const TrackerServerListMessage& message)
    -> std::expected<std::vector<std::byte>, EncodeError> {
  if (message.servers.size() > kMaxFieldCount) {
    return std::unexpected(EncodeError::count_too_large);
  }

  std::vector<std::byte> entries;
  for (const TrackerServerEntry& entry : message.servers) {
    if (entry.name.size() > 255 || entry.description.size() > 255) {
      return std::unexpected(EncodeError::string_too_long);
    }
    append_server_entry(entries, entry);
  }
  const std::size_t size = 4 + entries.size();  // totalCount + count + entries
  if (size > kMaxFieldDataSize) {
    return std::unexpected(EncodeError::element_too_large);
  }

  std::vector<std::byte> out;
  out.reserve(2 + 2 + size);
  append_u16(out, kServerListType);
  append_u16(out, static_cast<std::uint16_t>(size));
  append_u16(out, message.total_count);
  append_u16(out, static_cast<std::uint16_t>(message.servers.size()));
  out.insert(out.end(), entries.begin(), entries.end());
  return out;
}

auto try_decode_tracker_server_list(std::span<const std::byte> bytes)
    -> std::expected<TrackerServerListMessage, DecodeError> {
  if (bytes.size() < 4) {
    return std::unexpected(DecodeError::truncated);
  }
  const std::uint16_t type = read_u16be(bytes.first<2>());
  if (type != kServerListType) {
    return std::unexpected(DecodeError::wrong_format_tag);
  }
  const std::size_t size = read_u16be(bytes.subspan(2).first<2>());
  if (size < 4) {
    return std::unexpected(DecodeError::truncated);
  }
  if (bytes.size() < 4 + size) {
    return std::unexpected(DecodeError::truncated);
  }
  if (bytes.size() > 4 + size) {
    return std::unexpected(DecodeError::trailing_bytes);
  }

  TrackerServerListMessage message;
  message.total_count = read_u16be(bytes.subspan(4).first<2>());
  const std::size_t count = read_u16be(bytes.subspan(6).first<2>());
  std::span<const std::byte> entries = bytes.subspan(8, size - 4);
  message.servers.reserve(count);

  for (std::size_t i = 0; i < count; ++i) {
    std::size_t consumed = 0;
    auto entry = read_server_entry(entries, consumed);
    if (!entry.has_value()) {
      return std::unexpected(entry.error());
    }
    entries = entries.subspan(consumed);
    message.servers.push_back(std::move(*entry));
  }
  if (!entries.empty()) {
    return std::unexpected(DecodeError::trailing_bytes);
  }
  return message;
}

auto encode_tracker_lookup_reply(const TrackerLookupReply& reply)
    -> std::expected<std::vector<std::byte>, EncodeError> {
  std::vector<std::byte> out;
  if (reply.found) {
    if (reply.entry.name.size() > 255 || reply.entry.description.size() > 255) {
      return std::unexpected(EncodeError::string_too_long);
    }
    std::vector<std::byte> entry_bytes;
    append_server_entry(entry_bytes, reply.entry);
    if (entry_bytes.size() > kMaxFieldDataSize) {
      return std::unexpected(EncodeError::element_too_large);
    }
    append_u16(out, kLookupTypeFound);
    append_u16(out, static_cast<std::uint16_t>(entry_bytes.size()));
    out.insert(out.end(), entry_bytes.begin(), entry_bytes.end());
  } else {
    append_u16(out, kLookupTypeNotFound);
    append_u16(out, 0);
  }
  return out;
}

auto try_decode_tracker_lookup_reply(std::span<const std::byte> bytes)
    -> std::expected<TrackerLookupReply, DecodeError> {
  if (bytes.size() < 4) {
    return std::unexpected(DecodeError::truncated);
  }
  const std::uint16_t type = read_u16be(bytes.first<2>());
  const std::size_t size = read_u16be(bytes.subspan(2).first<2>());

  TrackerLookupReply reply;
  if (type == kLookupTypeNotFound) {
    if (size != 0) {
      return std::unexpected(DecodeError::trailing_bytes);
    }
    if (bytes.size() != 4) {
      return std::unexpected(DecodeError::trailing_bytes);
    }
    reply.found = false;
    return reply;
  }
  if (type != kLookupTypeFound) {
    return std::unexpected(DecodeError::wrong_format_tag);
  }
  if (bytes.size() != 4 + size) {
    return std::unexpected(bytes.size() < 4 + size ? DecodeError::truncated
                                                   : DecodeError::trailing_bytes);
  }
  std::size_t consumed = 0;
  auto entry = read_server_entry(bytes.subspan(4, size), consumed);
  if (!entry.has_value()) {
    return std::unexpected(entry.error());
  }
  if (consumed != size) {
    return std::unexpected(DecodeError::trailing_bytes);
  }
  reply.found = true;
  reply.entry = std::move(*entry);
  return reply;
}

}  // namespace hotline::protocol
