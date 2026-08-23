// Hotline crypto: SHA-1 message digest (FIPS 180-1).
//
// Implemented from the specification, verified against the FIPS 180-1 test
// vectors in tests/crypto. One-shot digest API.

#pragma once

#include <array>
#include <cstddef>
#include <span>

namespace hotline::crypto {

struct Sha1 {
  static constexpr std::size_t block_size = 64;
  static constexpr std::size_t digest_size = 20;
  using Digest = std::array<std::byte, digest_size>;

  [[nodiscard]] static auto digest(std::span<const std::byte> message) -> Digest;
};

}  // namespace hotline::crypto
