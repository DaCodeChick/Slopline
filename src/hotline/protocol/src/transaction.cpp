#include "hotline/protocol/transaction.h"

#include <cstddef>

namespace hotline::protocol {

auto try_decode_header(std::span<const std::byte> bytes)
    -> std::expected<TransactionHeader, DecodeError> {
  if (bytes.size() < kTransactionHeaderSize) {
    return std::unexpected(DecodeError::truncated);
  }
  if (bytes.size() > kTransactionHeaderSize) {
    return std::unexpected(DecodeError::trailing_bytes);
  }
  return decode_header(bytes.first<kTransactionHeaderSize>());
}

auto encode_transaction(const TransactionHeader& header, std::span<const std::byte> data)
    -> std::vector<std::byte> {
  std::array<std::byte, kTransactionHeaderSize> header_bytes{};
  encode_header(header, header_bytes);

  std::vector<std::byte> out;
  out.reserve(kTransactionHeaderSize + data.size());
  out.insert(out.end(), header_bytes.begin(), header_bytes.end());
  out.insert(out.end(), data.begin(), data.end());
  return out;
}

}  // namespace hotline::protocol
