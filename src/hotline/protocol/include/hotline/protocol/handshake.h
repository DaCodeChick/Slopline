// Hotline wire protocol: connection-establishment handshake codec.
//
// Replaces the historical establish path (AppWarrior
// Source/Hardware/UTransact.cpp:212-285 client side, :350-426 server side):
//
//   client -> server:  'TRTP'  'HOTL'  u16 version  u16 sub_version
//                       (12 bytes, all big-endian)
//   server -> client:  'TRTP'  u32 error
//                       (8 bytes; error == 0 means accepted)
//
// Historical quirks preserved verbatim:
//  * the protocol tag 'NICK' is accepted as an alias for 'TRTP' on both
//    the receive (server) and reply (client) side;
//  * a server rejection with reason 0 is normalized to 1
//    (UTransact::RejectEstablish);
//  * the server validates version == 1 and replies error 1 otherwise;
//  * the client treats a reply with a non-zero error as "version unknown".
//
// The version/sub_version values carried by the historical client are
// 1 / 2 (main connection) and 1 / 3 (HTXF transfer connection) — see
// constants.h.

#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

#include "hotline/protocol/constants.h"
#include "hotline/protocol/decode_error.h"
#include "hotline/protocol/endian.h"

namespace hotline::protocol {

struct ClientHandshake {
  std::uint32_t protocol = kProtocolTrTp;      // 'TRTP' (legacy alias: 'NICK')
  std::uint32_t sub_protocol = kSubProtocolHotl;  // 'HOTL' (or 'HTXF' for transfers)
  std::uint16_t version = kProtocolVersion;
  std::uint16_t sub_version = kClientSubVersion;
};

static_assert(sizeof(ClientHandshake) == kClientHandshakeSize);

struct ServerHandshakeReply {
  std::uint32_t protocol = kProtocolTrTp;  // legacy alias: 'NICK'
  std::uint32_t error = 0;
};

static_assert(sizeof(ServerHandshakeReply) == kServerHandshakeReplySize);

// Fixed-size overloads.
constexpr void encode_client_handshake(const ClientHandshake& handshake,
                                       std::span<std::byte, kClientHandshakeSize> out) noexcept {
  write_u32be(handshake.protocol, out.subspan<0, 4>());
  write_u32be(handshake.sub_protocol, out.subspan<4, 4>());
  write_u16be(handshake.version, out.subspan<8, 2>());
  write_u16be(handshake.sub_version, out.subspan<10, 2>());
}

[[nodiscard]] constexpr auto decode_client_handshake(
    std::span<const std::byte, kClientHandshakeSize> bytes) noexcept -> ClientHandshake {
  ClientHandshake handshake;
  handshake.protocol = read_u32be(bytes.subspan<0, 4>());
  handshake.sub_protocol = read_u32be(bytes.subspan<4, 4>());
  handshake.version = read_u16be(bytes.subspan<8, 2>());
  handshake.sub_version = read_u16be(bytes.subspan<10, 2>());
  return handshake;
}

constexpr void encode_server_handshake_reply(
    const ServerHandshakeReply& reply,
    std::span<std::byte, kServerHandshakeReplySize> out) noexcept {
  write_u32be(reply.protocol, out.subspan<0, 4>());
  write_u32be(reply.error, out.subspan<4, 4>());
}

[[nodiscard]] constexpr auto decode_server_handshake_reply(
    std::span<const std::byte, kServerHandshakeReplySize> bytes) noexcept -> ServerHandshakeReply {
  ServerHandshakeReply reply;
  reply.protocol = read_u32be(bytes.subspan<0, 4>());
  reply.error = read_u32be(bytes.subspan<4, 4>());
  return reply;
}

// Dynamic-size decode: rejects truncation / trailing bytes with
// DecodeError (distinct names so callers never land in an overload trap
// between the two span extents).
[[nodiscard]] auto try_decode_client_handshake(std::span<const std::byte> bytes)
    -> std::expected<ClientHandshake, DecodeError>;

[[nodiscard]] auto try_decode_server_handshake_reply(std::span<const std::byte> bytes)
    -> std::expected<ServerHandshakeReply, DecodeError>;

// --- historical validation policy (UTransact::ReceiveEstablish) ----------

enum class ClientHandshakeValidation {
  accepted,
  not_transaction_client,  // protocol is neither 'TRTP' nor 'NICK'
  incompatible_version,    // version != 1 (server replies error 1)
};

[[nodiscard]] constexpr auto validate_client_handshake(const ClientHandshake& handshake) noexcept
    -> ClientHandshakeValidation {
  if (handshake.protocol != kProtocolTrTp && handshake.protocol != kProtocolNick) {
    return ClientHandshakeValidation::not_transaction_client;
  }
  if (handshake.version != kProtocolVersion) {
    return ClientHandshakeValidation::incompatible_version;
  }
  return ClientHandshakeValidation::accepted;
}

enum class ServerHandshakeReplyValidation {
  accepted,
  format_unknown,   // reply protocol is neither 'TRTP' nor 'NICK'
  version_unknown,  // reply error is non-zero
};

[[nodiscard]] constexpr auto validate_server_handshake_reply(const ServerHandshakeReply& reply) noexcept
    -> ServerHandshakeReplyValidation {
  if (reply.protocol != kProtocolTrTp && reply.protocol != kProtocolNick) {
    return ServerHandshakeReplyValidation::format_unknown;
  }
  if (reply.error != 0) {
    return ServerHandshakeReplyValidation::version_unknown;
  }
  return ServerHandshakeReplyValidation::accepted;
}

// UTransact::RejectEstablish normalizes a zero reason to 1.
[[nodiscard]] constexpr auto normalize_reject_reason(std::uint32_t reason) noexcept -> std::uint32_t {
  return reason == 0 ? 1U : reason;
}

}  // namespace hotline::protocol
