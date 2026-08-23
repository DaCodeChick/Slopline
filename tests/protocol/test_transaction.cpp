#include "hotline/protocol/transaction.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "support/test_support.h"

using namespace hotline::protocol;
using namespace hotline_test;

// Golden vector: an empty KeepConnectionAlive request as sent by the
// historical client (audit/06 §11.1) — flag=0, isReply=0, type=500, id=1,
// error/totalSize/dataSize all zero.
TEST_CASE("golden: empty KeepConnectionAlive header") {
  const std::vector<std::byte> bytes =
      bytes_from_hex("00 00 01 f4 00 00 00 01 00 00 00 00 00 00 00 00 00 00 00 00");

  const auto decoded = try_decode_header(std::span<const std::byte>(bytes));
  CHECK(decoded.has_value());
  const TransactionHeader header = decoded.value();
  CHECK(header.flag == 0);
  CHECK(header.is_reply == 0);
  CHECK(header.type == TransactionType::KeepConnectionAlive);
  CHECK(header.id == 1U);
  CHECK(header.error == 0U);
  CHECK(header.total_size == 0U);
  CHECK(header.data_size == 0U);

  TransactionHeader reencoded;
  reencoded.type = TransactionType::KeepConnectionAlive;
  reencoded.id = 1U;
  std::array<std::byte, kTransactionHeaderSize> out{};
  encode_header(reencoded, out);
  REQUIRE_BYTES(out, "00 00 01 f4 00 00 00 01 00 00 00 00 00 00 00 00 00 00 00 00");
}

TEST_CASE("reply header round-trip with all fields populated") {
  TransactionHeader header;
  header.flag = 0;
  header.is_reply = 1;
  header.type = TransactionType::ChatMessage;
  header.id = 42U;
  header.error = 7U;
  header.total_size = 5U;
  header.data_size = 5U;

  std::array<std::byte, kTransactionHeaderSize> encoded{};
  encode_header(header, encoded);

  const auto decoded = decode_header(encoded);
  CHECK(decoded.flag == header.flag);
  CHECK(decoded.is_reply == header.is_reply);
  CHECK(decoded.type == header.type);
  CHECK(decoded.id == header.id);
  CHECK(decoded.error == header.error);
  CHECK(decoded.total_size == header.total_size);
  CHECK(decoded.data_size == header.data_size);
}

TEST_CASE("header round-trip at the numeric boundaries") {
  TransactionHeader header;
  header.flag = 0xFF;
  header.is_reply = 1;
  header.type = TransactionType::KeepConnectionAlive;  // 500 = 0x01F4
  header.id = 0xFFFFFFFFU;
  header.error = 0xFFFFFFFFU;
  header.total_size = 0xFFFFFFFFU;
  header.data_size = 0xFFFFFFFFU;

  std::array<std::byte, kTransactionHeaderSize> encoded{};
  encode_header(header, encoded);

  const auto decoded = decode_header(encoded);
  CHECK(decoded.flag == 0xFF);
  CHECK(decoded.is_reply == 1);
  CHECK(decoded.type == TransactionType::KeepConnectionAlive);
  CHECK(decoded.id == 0xFFFFFFFFU);
  CHECK(decoded.error == 0xFFFFFFFFU);
  CHECK(decoded.total_size == 0xFFFFFFFFU);
  CHECK(decoded.data_size == 0xFFFFFFFFU);
}

TEST_CASE("encode_transaction places data after the header") {
  TransactionHeader header;
  header.type = TransactionType::ServerMessage;
  header.id = 9U;
  header.total_size = 4U;
  header.data_size = 4U;
  const std::vector<std::byte> data = bytes_from_hex("68 69 21 21");

  const std::vector<std::byte> encoded = encode_transaction(header, data);
  CHECK(encoded.size() == kTransactionHeaderSize + data.size());
  REQUIRE_BYTES_MSG(std::span<const std::byte>(encoded).first<kTransactionHeaderSize>(),
                    "00 00 00 68 00 00 00 09 00 00 00 00 00 00 00 04 00 00 00 04",
                    "transaction header prefix");
  REQUIRE_BYTES_MSG(std::span<const std::byte>(encoded).subspan(kTransactionHeaderSize),
                    "68 69 21 21", "transaction data suffix");
}

TEST_CASE("dynamic decode rejects truncation and trailing bytes") {
  const std::vector<std::byte> full =
      bytes_from_hex("00 00 01 f4 00 00 00 01 00 00 00 00 00 00 00 00 00 00 00 00");

  for (std::size_t prefix = 0; prefix < kTransactionHeaderSize; ++prefix) {
    const auto decoded =
        try_decode_header(std::span<const std::byte>(full).first(prefix));
    CHECK(!decoded.has_value());
    CHECK(decoded.error() == DecodeError::truncated);
  }

  std::vector<std::byte> oversized = full;
  oversized.push_back(std::byte{0});
  const auto decoded = try_decode_header(std::span<const std::byte>(oversized));
  CHECK(!decoded.has_value());
  CHECK(decoded.error() == DecodeError::trailing_bytes);
}

TEST_CASE("fixed-size decode accepts a full dynamic span") {
  const std::vector<std::byte> bytes =
      bytes_from_hex("00 01 00 6c 00 00 00 02 00 00 00 00 00 00 00 03 00 00 00 03");
  const auto decoded = decode_header(std::span<const std::byte>(bytes).first<kTransactionHeaderSize>());
  CHECK(decoded.is_reply == 1);
  CHECK(decoded.type == TransactionType::SendInstantMessage);  // 108 = 0x006C
  CHECK(decoded.id == 2U);
  CHECK(decoded.total_size == 3U);
  CHECK(decoded.data_size == 3U);
}
