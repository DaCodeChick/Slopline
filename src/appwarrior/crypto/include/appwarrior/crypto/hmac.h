// AppWarrior crypto: HMAC (RFC 2104).
//
// Generic RFC 2104 construction over a one-shot digest (Md5 or Sha1).
// Hotline's legacy login key schedule (HLCrypt::Init / Perm*Key) builds on
// this and lives in the Hotline layer (hotline/protocol/key_schedule.h).

#pragma once

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>
#include <vector>

namespace aw::crypto {

// One-shot digest with a fixed-size Digest and a power-of-two block size.
template <typename H>
concept message_digest = requires(std::span<const std::byte> bytes) {
  typename H::Digest;
  requires H::block_size > 0;
  requires H::digest_size == std::tuple_size_v<typename H::Digest>;
  { H::digest(bytes) } -> std::same_as<typename H::Digest>;
};

// RFC 2104: HMAC(key, message). The key is hashed first if longer than the
// block size, then padded with zeros to the block size.
template <message_digest H>
[[nodiscard]] auto hmac(std::span<const std::byte> key, std::span<const std::byte> message)
    -> std::vector<std::byte> {
  std::array<std::byte, H::block_size> k{};
  if (key.size() > H::block_size) {
    const typename H::Digest hashed = H::digest(key);
    std::ranges::copy(hashed, k.begin());
  } else {
    std::ranges::copy(key, k.begin());
  }

  std::array<std::byte, H::block_size> inner{};
  std::array<std::byte, H::block_size> outer{};
  for (std::size_t i = 0; i < H::block_size; ++i) {
    const unsigned byte = std::to_integer<unsigned>(k[i]);
    inner[i] = static_cast<std::byte>(byte ^ 0x36U);
    outer[i] = static_cast<std::byte>(byte ^ 0x5CU);
  }

  std::vector<std::byte> buffer;
  buffer.reserve(H::block_size + message.size());
  buffer.insert(buffer.end(), inner.begin(), inner.end());
  buffer.insert(buffer.end(), message.begin(), message.end());
  const typename H::Digest first = H::digest(buffer);

  buffer.clear();
  buffer.reserve(H::block_size + H::digest_size);
  buffer.insert(buffer.end(), outer.begin(), outer.end());
  buffer.insert(buffer.end(), first.begin(), first.end());
  const typename H::Digest second = H::digest(buffer);

  return std::vector<std::byte>(second.begin(), second.end());
}

}  // namespace aw::crypto
