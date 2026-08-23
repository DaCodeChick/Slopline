// Hotline wire protocol: transaction header codec.
//
// Replaces the historical STranHdr handling (AppWarrior
// Source/Hardware/UTransact.cpp:13-24, :907-975, :983-1047) with an
// explicit, bounds-checked codec. The 20-byte layout is fixed and every
// multi-byte member is big-endian:
//
//   offset  size  member
//   0       1     flag        (reserved; 0, or — under encryption — the
//                              key-permutation index 1..32, see audit/06 §4)
//   1       1     is_reply    (0 = request, 1 = reply)
//   2       2     type        (TransactionType, big-endian)
//   4       4     id          (arbitrary value echoed by the reply)
//   8       4     error       (error code used for replies)
//   12      4     total_size  (full size when split across parts)
//   16      4     data_size   (bytes of data carried by this part)
//   20      —     data        (not part of the header)
//
// Historical receive policy (NOT reproduced in the codec — it belongs to
// the connection layer): the legacy reader kills the connection if
// total_size or data_size is 0 or exceeds the configured cap (2 MB
// framework default, 512 KB server override). The codec faithfully
// encodes/decodes any values, including zero.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

#include "hotline/protocol/constants.h"
#include "hotline/protocol/decode_error.h"
#include "hotline/protocol/endian.h"

namespace hotline::protocol {

struct TransactionHeader {
  std::uint8_t flag = 0;    // see above; do not assume it is always 0
  std::uint8_t is_reply = 0;
  TransactionType type = TransactionType::Error;
  std::uint32_t id = 0;
  std::uint32_t error = 0;
  std::uint32_t total_size = 0;
  std::uint32_t data_size = 0;
};

static_assert(sizeof(TransactionHeader) == kTransactionHeaderSize);

// Fixed-size overloads: exactly 20 bytes in / out.
constexpr void encode_header(const TransactionHeader& header,
                             std::span<std::byte, kTransactionHeaderSize> out) noexcept {
  out[0] = static_cast<std::byte>(header.flag);
  out[1] = static_cast<std::byte>(header.is_reply);
  write_u16be(static_cast<std::uint16_t>(header.type), out.subspan<2, 2>());
  write_u32be(header.id, out.subspan<4, 4>());
  write_u32be(header.error, out.subspan<8, 4>());
  write_u32be(header.total_size, out.subspan<12, 4>());
  write_u32be(header.data_size, out.subspan<16, 4>());
}

[[nodiscard]] constexpr auto decode_header(
    std::span<const std::byte, kTransactionHeaderSize> bytes) noexcept -> TransactionHeader {
  TransactionHeader header;
  header.flag = std::to_integer<std::uint8_t>(bytes[0]);
  header.is_reply = std::to_integer<std::uint8_t>(bytes[1]);
  header.type = static_cast<TransactionType>(read_u16be(bytes.subspan<2, 2>()));
  header.id = read_u32be(bytes.subspan<4, 4>());
  header.error = read_u32be(bytes.subspan<8, 4>());
  header.total_size = read_u32be(bytes.subspan<12, 4>());
  header.data_size = read_u32be(bytes.subspan<16, 4>());
  return header;
}

// Dynamic-size decode: rejects truncation and trailing bytes with
// DecodeError (distinct name so callers never land in an overload trap
// between the two span extents).
[[nodiscard]] auto try_decode_header(std::span<const std::byte> bytes)
    -> std::expected<TransactionHeader, DecodeError>;

// Encodes a full (single-part) transaction: header followed verbatim by
// `data`. The caller is responsible for header.data_size / total_size
// consistency exactly as the historical _TNSendTran was: it received
// dataSize and totalSize as separate parameters and did not derive or
// validate them (multi-part sends have data_size != total_size).
[[nodiscard]] auto encode_transaction(const TransactionHeader& header,
                                      std::span<const std::byte> data)
    -> std::vector<std::byte>;

}  // namespace hotline::protocol
