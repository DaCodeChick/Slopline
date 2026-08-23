// Hotline wire protocol: tracker messages.
//
// Verified against the legacy tracker and client (TrackerServ.cpp:585-643
// registration, :1055-1170 server list + lookup; HotlineTasks.cpp:5939-5959
// and :6057-6065 handshake):
//
//  * Registration (server -> tracker, UDP): u16 type (1 = add, 2 = remove
//    — the tracker ignores removes), u16 port, u16 userCount, u16 flags
//    (1 = don't show in list), u32 passID, p-string name, p-string desc,
//    p-string password. The server address is the UDP source address.
//  * Handshake (client -> tracker, TCP): 'HTRK' + u16 version; version 2
//    appends two 32-byte fields (login and password p-strings padded with
//    zeros, clamped to 31 chars). The tracker replies with the same 6
//    bytes ('HTRK' + version).
//  * Server list (tracker -> client, TCP): u16 type=1, u16 size (bytes
//    after this field), u16 totalCount, u16 count, then count × {4 raw
//    IP octets, u16 port, u16 userCount, u16 flags, p-string name,
//    p-string desc}. Large lists are split into multiple messages at the
//    legacy 8192-byte buffer boundary — a transport/session concern, not
//    part of the codec (each message is self-contained).
//  * Lookup: u16 type=4 + u16 size + one entry (found), or u16 type=5 +
//    u16 size=0 (not found).

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <vector>

#include "appwarrior/core/endian.h"
#include "hotline/protocol/decode_error.h"

namespace hotline::protocol {

inline constexpr std::uint32_t kTrackerProtocolTag = aw::endian::four_cc('H', 'T', 'R', 'K');
inline constexpr std::uint16_t kTrackerHandshakeVersion1 = 1;
inline constexpr std::uint16_t kTrackerHandshakeVersion2 = 2;
inline constexpr std::size_t kTrackerLoginFieldSize = 32;
inline constexpr std::size_t kTrackerHandshakeSizeV1 = 6;
inline constexpr std::size_t kTrackerHandshakeSizeV2 = 70;

inline constexpr std::uint16_t kRegistrationTypeAdd = 1;
inline constexpr std::uint16_t kRegistrationTypeRemove = 2;
inline constexpr std::uint16_t kServerListType = 1;
inline constexpr std::uint16_t kLookupTypeFound = 4;
inline constexpr std::uint16_t kLookupTypeNotFound = 5;

// ---------------------------------------------------------------------------
// Registration (UDP)
// ---------------------------------------------------------------------------

struct TrackerRegistration {
  std::uint16_t port = 0;
  std::uint16_t user_count = 0;
  std::uint16_t flags = 0;  // 1 = don't show in list
  std::uint32_t pass_id = 0;
  std::string name;
  std::string description;
  std::string password;
};

struct TrackerRegistrationMessage {
  std::uint16_t type = kRegistrationTypeAdd;
  TrackerRegistration registration;
};

[[nodiscard]] auto encode_tracker_registration(std::uint16_t type,
                                               const TrackerRegistration& registration)
    -> std::expected<std::vector<std::byte>, EncodeError>;
[[nodiscard]] auto try_decode_tracker_registration(std::span<const std::byte> bytes)
    -> std::expected<TrackerRegistrationMessage, DecodeError>;

// ---------------------------------------------------------------------------
// Handshake (TCP)
// ---------------------------------------------------------------------------

struct TrackerHandshake {
  std::uint16_t version = 0;
  std::string login;
  std::string password;
};

[[nodiscard]] auto encode_tracker_handshake(std::uint16_t version, std::string_view login,
                                            std::string_view password) -> std::vector<std::byte>;
[[nodiscard]] auto try_decode_tracker_handshake(std::span<const std::byte> bytes)
    -> std::expected<TrackerHandshake, DecodeError>;

[[nodiscard]] auto encode_tracker_handshake_reply(std::uint16_t version)
    -> std::array<std::byte, kTrackerHandshakeSizeV1>;
[[nodiscard]] auto try_decode_tracker_handshake_reply(std::span<const std::byte> bytes)
    -> std::expected<std::uint16_t, DecodeError>;

// ---------------------------------------------------------------------------
// Server list and lookup (TCP)
// ---------------------------------------------------------------------------

struct TrackerServerEntry {
  std::array<std::uint8_t, 4> address{};  // raw IP octets, network order
  std::uint16_t port = 0;
  std::uint16_t user_count = 0;
  std::uint16_t flags = 0;
  std::string name;
  std::string description;
};

struct TrackerServerListMessage {
  std::uint16_t total_count = 0;  // same value in every chunk of a split list
  std::vector<TrackerServerEntry> servers;
};

[[nodiscard]] auto encode_tracker_server_list(const TrackerServerListMessage& message)
    -> std::expected<std::vector<std::byte>, EncodeError>;
[[nodiscard]] auto try_decode_tracker_server_list(std::span<const std::byte> bytes)
    -> std::expected<TrackerServerListMessage, DecodeError>;

struct TrackerLookupReply {
  bool found = false;
  TrackerServerEntry entry;
};

[[nodiscard]] auto encode_tracker_lookup_reply(const TrackerLookupReply& reply)
    -> std::expected<std::vector<std::byte>, EncodeError>;
[[nodiscard]] auto try_decode_tracker_lookup_reply(std::span<const std::byte> bytes)
    -> std::expected<TrackerLookupReply, DecodeError>;

}  // namespace hotline::protocol
