#include "hotline/crypto/sha1.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace hotline::crypto {

namespace {

[[nodiscard]] constexpr auto rotate_left(std::uint32_t value, std::uint32_t amount) noexcept
    -> std::uint32_t {
  return (value << amount) | (value >> (32U - amount));
}

struct State {
  std::uint32_t h0 = 0x67452301U;
  std::uint32_t h1 = 0xEFCDAB89U;
  std::uint32_t h2 = 0x98BADCFEU;
  std::uint32_t h3 = 0x10325476U;
  std::uint32_t h4 = 0xC3D2E1F0U;
  std::uint64_t bytes = 0;
};

void process_block(State& state, const std::byte* block) {
  std::array<std::uint32_t, 80> w{};
  for (std::size_t i = 0; i < 16; ++i) {
    const std::size_t at = i * 4U;
    w[i] = (std::to_integer<std::uint32_t>(block[at]) << 24) |
           (std::to_integer<std::uint32_t>(block[at + 1]) << 16) |
           (std::to_integer<std::uint32_t>(block[at + 2]) << 8) |
           std::to_integer<std::uint32_t>(block[at + 3]);
  }
  for (std::size_t i = 16; i < w.size(); ++i) {
    w[i] = rotate_left(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
  }

  std::uint32_t a = state.h0;
  std::uint32_t b = state.h1;
  std::uint32_t c = state.h2;
  std::uint32_t d = state.h3;
  std::uint32_t e = state.h4;

  for (std::size_t i = 0; i < 80; ++i) {
    std::uint32_t f;
    std::uint32_t k;
    if (i < 20) {
      f = (b & c) | (~b & d);
      k = 0x5A827999U;
    } else if (i < 40) {
      f = b ^ c ^ d;
      k = 0x6ED9EBA1U;
    } else if (i < 60) {
      f = (b & c) | (b & d) | (c & d);
      k = 0x8F1BBCDCU;
    } else {
      f = b ^ c ^ d;
      k = 0xCA62C1D6U;
    }

    const std::uint32_t temp = rotate_left(a, 5) + f + e + k + w[i];
    e = d;
    d = c;
    c = rotate_left(b, 30);
    b = a;
    a = temp;
  }

  state.h0 += a;
  state.h1 += b;
  state.h2 += c;
  state.h3 += d;
  state.h4 += e;
}

}  // namespace

auto Sha1::digest(std::span<const std::byte> message) -> Digest {
  State state;

  const std::byte* at = message.data();
  std::size_t remaining = message.size();
  while (remaining >= block_size) {
    process_block(state, at);
    state.bytes += block_size;
    at += block_size;
    remaining -= block_size;
  }

  // Final block: remaining bytes, 0x80, zero padding, 64-bit BE bit length.
  const std::uint64_t bit_length = (state.bytes + remaining) * 8U;
  std::array<std::byte, 128> tail{};
  std::size_t n = 0;
  for (std::size_t i = 0; i < remaining; ++i) {
    tail[n] = at[i];
    ++n;
  }
  tail[n] = std::byte{0x80};
  ++n;

  if (n > 56) {
    while (n < block_size) {
      tail[n] = std::byte{0};
      ++n;
    }
    process_block(state, tail.data());
    n = 0;
  }
  while (n < 56) {
    tail[n] = std::byte{0};
    ++n;
  }
  for (std::size_t i = 0; i < 8; ++i) {
    tail[56 + i] = static_cast<std::byte>((bit_length >> (8U * (7U - i))) & 0xFFU);
  }
  process_block(state, tail.data());

  Digest out{};
  const auto emit = [&out](std::size_t offset, std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i) {
      out[offset + i] = static_cast<std::byte>((value >> (8U * (3U - i))) & 0xFFU);
    }
  };
  emit(0, state.h0);
  emit(4, state.h1);
  emit(8, state.h2);
  emit(12, state.h3);
  emit(16, state.h4);
  return out;
}

}  // namespace hotline::crypto
