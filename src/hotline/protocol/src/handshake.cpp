#include "hotline/protocol/handshake.h"

namespace hotline::protocol {

auto try_decode_client_handshake(std::span<const std::byte> bytes)
    -> std::expected<ClientHandshake, DecodeError> {
  if (bytes.size() < kClientHandshakeSize) {
    return std::unexpected(DecodeError::truncated);
  }
  if (bytes.size() > kClientHandshakeSize) {
    return std::unexpected(DecodeError::trailing_bytes);
  }
  return decode_client_handshake(bytes.first<kClientHandshakeSize>());
}

auto try_decode_server_handshake_reply(std::span<const std::byte> bytes)
    -> std::expected<ServerHandshakeReply, DecodeError> {
  if (bytes.size() < kServerHandshakeReplySize) {
    return std::unexpected(DecodeError::truncated);
  }
  if (bytes.size() > kServerHandshakeReplySize) {
    return std::unexpected(DecodeError::trailing_bytes);
  }
  return decode_server_handshake_reply(bytes.first<kServerHandshakeReplySize>());
}

}  // namespace hotline::protocol
