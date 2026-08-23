#include "hotline/crypto/md5.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace hotline::crypto {

namespace {

constexpr std::array<std::uint32_t, 64> kK = {
    0xd76aa478U, 0xe8c7b756U, 0x242070dbU, 0xc1bdceeeU, 0xf57c0fafU, 0x4787c62aU,
    0xa8304613U, 0xfd469501U, 0x698098d8U, 0x8b44f7afU, 0xffff5bb1U, 0x895cd7beU,
    0x6b901122U, 0xfd987193U, 0xa679438eU, 0x49b40821U, 0xf61e2562U, 0xc040b340U,
    0x265e5a51U, 0xe9b6c7aaU, 0xd62f105dU, 0x02441453U, 0xd8a1e681U, 0xe7d3fbc8U,
    0x21e1cde6U, 0xc33707d6U, 0xf4d50d87U, 0x455a14edU, 0xa9e3e905U, 0xfcefa3f8U,
    0x676f02d9U, 0x8d2a4c8aU, 0xfffa3942U, 0x8771f681U, 0x6d9d6122U, 0xfde5380cU,
    0xa4beea44U, 0x4bdecfa9U, 0xf6bb4b60U, 0xbebfbc70U, 0x289b7ec6U, 0xeaa127faU,
    0xd4ef3085U, 0x04881d05U, 0xd9d4d039U, 0xe6db99e5U, 0x1fa27cf8U, 0xc4ac5665U,
    0xf4292244U, 0x432aff97U, 0xab9423a7U, 0xfc93a039U, 0x655b59c3U, 0x8f0ccc92U,
    0xffeff47dU, 0x85845dd1U, 0x6fa87e4fU, 0xfe2ce6e0U, 0xa3014314U, 0x4e0811a1U,
    0xf7537e82U, 0xbd3af235U, 0x2ad7d2bbU, 0xeb86d391U};

constexpr std::array<std::uint32_t, 64> kShift = {
    7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 5, 9, 14, 20, 5, 9,
    14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
    4, 11, 16, 23, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};

[[nodiscard]] constexpr auto rotate_left(std::uint32_t value, std::uint32_t amount) noexcept
    -> std::uint32_t {
  return (value << amount) | (value >> (32U - amount));
}

struct State {
  std::uint32_t a = 0x67452301U;
  std::uint32_t b = 0xefcdab89U;
  std::uint32_t c = 0x98badcfeU;
  std::uint32_t d = 0x10325476U;
  std::uint64_t bytes = 0;
};

void process_block(State& state, const std::byte* block) {
  std::array<std::uint32_t, 16> m{};
  for (std::size_t i = 0; i < m.size(); ++i) {
    const std::size_t at = i * 4U;
    m[i] = std::to_integer<std::uint32_t>(block[at]) |
           (std::to_integer<std::uint32_t>(block[at + 1]) << 8) |
           (std::to_integer<std::uint32_t>(block[at + 2]) << 16) |
           (std::to_integer<std::uint32_t>(block[at + 3]) << 24);
  }

  std::uint32_t a = state.a;
  std::uint32_t b = state.b;
  std::uint32_t c = state.c;
  std::uint32_t d = state.d;

  for (std::size_t i = 0; i < 64; ++i) {
    std::uint32_t f;
    std::uint32_t g;
    if (i < 16) {
      f = (b & c) | (~b & d);
      g = static_cast<std::uint32_t>(i);
    } else if (i < 32) {
      f = (d & b) | (~d & c);
      g = static_cast<std::uint32_t>((5U * i + 1U) % 16U);
    } else if (i < 48) {
      f = b ^ c ^ d;
      g = static_cast<std::uint32_t>((3U * i + 5U) % 16U);
    } else {
      f = c ^ (b | ~d);
      g = static_cast<std::uint32_t>((7U * i) % 16U);
    }

    const std::uint32_t temp = d;
    d = c;
    c = b;
    b = b + rotate_left(a + f + kK[i] + m[g], kShift[i]);
    a = temp;
  }

  state.a += a;
  state.b += b;
  state.c += c;
  state.d += d;
}

}  // namespace

auto Md5::digest(std::span<const std::byte> message) -> Digest {
  State state;

  const std::byte* at = message.data();
  std::size_t remaining = message.size();
  while (remaining >= block_size) {
    process_block(state, at);
    state.bytes += block_size;
    at += block_size;
    remaining -= block_size;
  }

  // Final block: remaining bytes, 0x80, zero padding, 64-bit LE bit length.
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
    tail[56 + i] = static_cast<std::byte>((bit_length >> (8U * i)) & 0xFFU);
  }
  process_block(state, tail.data());

  Digest out{};
  const auto emit = [&out](std::size_t offset, std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i) {
      out[offset + i] = static_cast<std::byte>((value >> (8U * i)) & 0xFFU);
    }
  };
  emit(0, state.a);
  emit(4, state.b);
  emit(8, state.c);
  emit(12, state.d);
  return out;
}

}  // namespace hotline::crypto
