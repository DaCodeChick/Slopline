// Hotline crypto: HMAC (RFC 2104) and the legacy login key schedule.
//
// hmac<Hash>() is the generic RFC 2104 construction over a one-shot digest
// (Md5 or Sha1). derive_login_keys/permute_key reproduce the historical
// HLCrypt::Init / PermEncodeKey / PermDecodeKey semantics exactly
// (legacy AppWarrior Crypt/HLCrypt.cpp:16-74):
//
//   temp1 = HMAC(password, sessionKey)   (applied twice)
//   temp2 = HMAC(password, temp1)
//   client: encodeKey = temp2, decodeKey = temp1
//   server: encodeKey = temp1, decodeKey = temp2
//   Perm(n): key = HMAC(sessionKey, key), n times
//
// The HMAC hash choice is negotiated at login (myField_MacAlg: "HMAC-SHA1"
// or "HMAC-MD5"), which is why these functions are templates over the
// digest type rather than a runtime-polymorphic interface.

#pragma once

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>
#include <vector>

namespace hotline::crypto {

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

// The two keys of the historical HLCrypt::Init schedule. `first` is the
// legacy "temp1" and `second` is "temp2"; which one a side uses to encode
// vs decode depends on the isClient flag (see header comment).
struct LoginKeys {
  std::vector<std::byte> first;
  std::vector<std::byte> second;
};

template <message_digest H>
[[nodiscard]] auto derive_login_keys(std::span<const std::byte> password,
                                     std::span<const std::byte> session_key) -> LoginKeys {
  LoginKeys keys;
  keys.first = hmac<H>(password, session_key);
  keys.first = hmac<H>(password, keys.first);
  keys.second = hmac<H>(password, keys.first);
  return keys;
}

// HLCrypt::PermEncodeKey/PermDecodeKey: replace the key with
// HMAC(sessionKey, key), `rounds` times, in place.
template <message_digest H>
void permute_key(std::vector<std::byte>& key, std::span<const std::byte> session_key,
                 std::uint32_t rounds) {
  while (rounds-- > 0) {
    key = hmac<H>(session_key, key);
  }
}

}  // namespace hotline::crypto
