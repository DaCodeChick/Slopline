#include "appwarrior/crypto/blowfish.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "appwarrior/core/endian.h"
#include "blowfish_tables.h"

namespace aw::crypto {


void Blowfish::set_encode_key(std::span<const std::byte> key) {
  expand_key(key, encode_key_);
}

void Blowfish::set_decode_key(std::span<const std::byte> key) {
  expand_key(key, decode_key_);
}

auto Blowfish::round_function(std::uint32_t x, const Schedule& key) -> std::uint32_t {
  const std::uint32_t a = (x >> 24) & 0xFFU;
  const std::uint32_t b = (x >> 16) & 0xFFU;
  const std::uint32_t c = (x >> 8) & 0xFFU;
  const std::uint32_t d = x & 0xFFU;

  std::uint32_t y = key.s[0][a] + key.s[1][b];
  y ^= key.s[2][c];
  y += key.s[3][d];
  return y;
}

auto Blowfish::encrypt_words(std::uint32_t l, std::uint32_t r, const Schedule& key)
    -> std::pair<std::uint32_t, std::uint32_t> {
  for (std::size_t i = 0; i < 16; ++i) {
    l ^= key.p[i];
    r = round_function(l, key) ^ r;
    std::swap(l, r);
  }
  std::swap(l, r);
  r ^= key.p[16];
  l ^= key.p[17];
  return {l, r};
}

auto Blowfish::decrypt_words(std::uint32_t l, std::uint32_t r, const Schedule& key)
    -> std::pair<std::uint32_t, std::uint32_t> {
  for (std::size_t i = 17; i > 1; --i) {
    l ^= key.p[i];
    r = round_function(l, key) ^ r;
    std::swap(l, r);
  }
  std::swap(l, r);
  r ^= key.p[1];
  l ^= key.p[0];
  return {l, r};
}

void Blowfish::expand_key(std::span<const std::byte> key, Schedule& out) {
  // Hex digits of pi, per the Blowfish specification.
  out.p = detail::kParrayPi;
  out.s[0] = detail::kSbox0;
  out.s[1] = detail::kSbox1;
  out.s[2] = detail::kSbox2;
  out.s[3] = detail::kSbox3;

  // XOR the P array with the key, wrapping (legacy ExpandKey does the same).
  std::size_t k = 0;
  for (std::size_t i = 0; i < out.p.size(); ++i) {
    std::uint32_t word = 0;
    for (int j = 0; j < 4; ++j) {
      word = (word << 8) | std::to_integer<std::uint32_t>(key[k]);
      k = (k + 1) % key.size();
    }
    out.p[i] ^= word;
  }

  std::uint32_t l = 0;
  std::uint32_t r = 0;
  for (std::size_t i = 0; i < out.p.size(); i += 2) {
    const auto [nl, nr] = encrypt_words(l, r, out);
    out.p[i] = nl;
    out.p[i + 1] = nr;
    l = nl;
    r = nr;
  }
  for (std::size_t box = 0; box < 4; ++box) {
    for (std::size_t j = 0; j < 256; j += 2) {
      const auto [nl, nr] = encrypt_words(l, r, out);
      out.s[box][j] = nl;
      out.s[box][j + 1] = nr;
      l = nl;
      r = nr;
    }
  }
}

auto Blowfish::encrypt_block(const Schedule& key,
                             std::span<const std::byte, kBlowfishBlockSize> block) const
    -> std::array<std::byte, kBlowfishBlockSize> {
  const std::uint32_t l = aw::endian::read_u32be(block.first<4>());
  const std::uint32_t r = aw::endian::read_u32be(block.subspan<4>().first<4>());
  const auto [nl, nr] = encrypt_words(l, r, key);

  std::array<std::byte, kBlowfishBlockSize> out{};
  aw::endian::write_u32be(nl, std::span<std::byte, kBlowfishBlockSize>(out).first<4>());
  aw::endian::write_u32be(nr, std::span<std::byte, kBlowfishBlockSize>(out).subspan<4>().first<4>());
  return out;
}

auto Blowfish::encrypt_block(std::span<const std::byte, kBlowfishBlockSize> block) const
    -> std::array<std::byte, kBlowfishBlockSize> {
  return encrypt_block(encode_key_, block);
}

auto Blowfish::decrypt_block(std::span<const std::byte, kBlowfishBlockSize> block) const
    -> std::array<std::byte, kBlowfishBlockSize> {
  const std::uint32_t l = aw::endian::read_u32be(block.first<4>());
  const std::uint32_t r = aw::endian::read_u32be(block.subspan<4>().first<4>());
  const auto [nl, nr] = decrypt_words(l, r, decode_key_);

  std::array<std::byte, kBlowfishBlockSize> out{};
  aw::endian::write_u32be(nl, std::span<std::byte, kBlowfishBlockSize>(out).first<4>());
  aw::endian::write_u32be(nr, std::span<std::byte, kBlowfishBlockSize>(out).subspan<4>().first<4>());
  return out;
}

void Blowfish::Ofb64::reset() noexcept {
  iv_.fill(std::byte{0});
  offset_ = 0;
}

void Blowfish::Ofb64::process(const Blowfish& cipher, const Schedule& schedule,
                              std::span<std::byte> buffer) noexcept {
  for (std::byte& byte : buffer) {
    if (offset_ == 0) {
      iv_ = cipher.encrypt_block(schedule, iv_);
    }
    byte ^= iv_[offset_];
    offset_ = (offset_ + 1) & 0x7U;
  }
}

void Blowfish::encode_stream(Ofb64& stream, std::span<std::byte> buffer) const noexcept {
  stream.process(*this, encode_key_, buffer);
}

void Blowfish::decode_stream(Ofb64& stream, std::span<std::byte> buffer) const noexcept {
  stream.process(*this, decode_key_, buffer);
}

}  // namespace aw::crypto
