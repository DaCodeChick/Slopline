// Hotline crypto: MD5 message digest (RFC 1321).
//
// Implemented from the RFC (not transcribed from the legacy HLMD5 code),
// verified against the RFC 1321 test vectors in tests/crypto. One-shot
// digest API — the protocol uses digests only through HMAC, and streaming
// is not needed.

#pragma once

#include <array>
#include <cstddef>
#include <span>

namespace hotline::crypto {

struct Md5 {
  static constexpr std::size_t block_size = 64;
  static constexpr std::size_t digest_size = 16;
  using Digest = std::array<std::byte, digest_size>;

  [[nodiscard]] static auto digest(std::span<const std::byte> message) -> Digest;
};

}  // namespace hotline::crypto
