// AppWarrior crypto: Blowfish block cipher + OFB-64 stream.
//
// Reproduces the historical HLBlowfish exactly (legacy
// AppWarrior/Source/Crypt/HLBlowfish.cpp + HLBlowfishData.h): standard
// Schneier Blowfish (hex-digits-of-pi tables, 16-round Feistel), key
// expansion wrapping the key bytes, and the OFB-64 stream mode Hotline
// uses for encrypted transactions — 64-bit output feedback with a ZERO
// initial vector, keystream bytes taken big-endian from the encrypted
// feedback block, byte-continuous across process() calls.
//
// Like the legacy class, one instance holds two independently expanded
// schedules (encode and decode) because the Hotline login key schedule
// derives different keys for each direction (hotline/protocol/
// transaction_cipher.h). OFB never uses the decrypt direction: both
// streams generate keystream via encrypt_block.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace aw::crypto {

inline constexpr std::size_t kBlowfishBlockSize = 8;

class Blowfish {
 private:
  struct Schedule {
    std::array<std::uint32_t, 18> p{};
    std::array<std::array<std::uint32_t, 256>, 4> s{};
  };

  static void expand_key(std::span<const std::byte> key, Schedule& out);
  [[nodiscard]] static auto round_function(std::uint32_t x, const Schedule&) -> std::uint32_t;
  [[nodiscard]] static auto encrypt_words(std::uint32_t l, std::uint32_t r, const Schedule&)
      -> std::pair<std::uint32_t, std::uint32_t>;
  [[nodiscard]] static auto decrypt_words(std::uint32_t l, std::uint32_t r, const Schedule&)
      -> std::pair<std::uint32_t, std::uint32_t>;
  [[nodiscard]] auto encrypt_block(const Schedule&,
                                   std::span<const std::byte, kBlowfishBlockSize>) const
      -> std::array<std::byte, kBlowfishBlockSize>;

  Schedule encode_key_{};
  Schedule decode_key_{};

 public:
  Blowfish() = default;

  // Key must be non-empty (legacy ExpandKey wraps with no bounds check;
  // the same precondition applies here).
  void set_encode_key(std::span<const std::byte> key);
  void set_decode_key(std::span<const std::byte> key);

  [[nodiscard]] auto encrypt_block(std::span<const std::byte, kBlowfishBlockSize> block) const
      -> std::array<std::byte, kBlowfishBlockSize>;
  [[nodiscard]] auto decrypt_block(std::span<const std::byte, kBlowfishBlockSize> block) const
      -> std::array<std::byte, kBlowfishBlockSize>;

  // OFB-64 stream state (zero IV; byte offset persists across calls).
  class Ofb64 {
   public:
    void reset() noexcept;

   private:
    friend class Blowfish;
    void process(const Blowfish& cipher, const Schedule& schedule,
                 std::span<std::byte> buffer) noexcept;
    std::array<std::byte, kBlowfishBlockSize> iv_{};
    std::size_t offset_ = 0;
  };

  // Stream helpers mirroring HLBlowfish::Encode/Decode: the encode stream
  // runs on the encode schedule, the decode stream on the decode schedule
  // (both generate keystream by ENCRYPTING the feedback block).
  void encode_stream(Ofb64& stream, std::span<std::byte> buffer) const noexcept;
  void decode_stream(Ofb64& stream, std::span<std::byte> buffer) const noexcept;
};

}  // namespace aw::crypto
