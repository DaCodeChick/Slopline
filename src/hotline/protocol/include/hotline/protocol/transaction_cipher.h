// Hotline wire protocol: encrypted-transaction stream cipher.
//
// Reproduces the historical encrypted-transaction mechanics exactly
// (legacy UTransact.cpp _TNSendTran :907-975 and the receive path
// :1003-1047 / :1069-1084, over HLCrypt/HLSha1/HLMD5/HLBlowfish):
//
//  * the login key schedule (HLCrypt::Init) derives two keys: the client
//    encodes with temp2 and decodes with temp1, the server the reverse
//    (key_schedule.h);
//  * the 20-byte transaction header is always stream-encoded under the
//    current key;
//  * header flag == 0: the whole data buffer is stream-encoded under the
//    current key;
//  * header flag != 0 (1..32): the FIRST TWO data bytes are encoded under
//    the current key, then the key is permuted `flag` times
//    (PermEncodeKey/PermDecodeKey) and the remainder encoded under the
//    new key — the historical receiver explicitly notes hxd treats those
//    2 bytes as part of the header;
//  * OFB-64 stream state persists across transactions in both directions.
//
// The sender's flag choice is free; legacy_flag_quirk() reproduces the
// historical sender's random distribution (HLRand bytes re-rolled through
// the 2/7/13 branch) as a documented, deterministic helper.

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "appwarrior/crypto/blowfish.h"
#include "hotline/protocol/key_schedule.h"
#include "hotline/protocol/transaction.h"

namespace hotline::protocol::auth {

template <aw::crypto::message_digest H>
class TransactionCipher {
 public:
  TransactionCipher(std::span<const std::byte> password,
                    std::span<const std::byte> session_key, bool is_client)
      : session_key_(session_key.begin(), session_key.end()) {
    const LoginKeys keys = derive_login_keys<H>(password, session_key);
    encode_key_ = is_client ? keys.second : keys.first;
    decode_key_ = is_client ? keys.first : keys.second;
    cipher_.set_encode_key(encode_key_);
    cipher_.set_decode_key(decode_key_);
  }

  // Encodes header + data in place; `flag` must be 0..32 (0 = no key
  // permutation). Mirrors _TNSendTran including the empty-data case
  // (header only).
  void encode(std::span<std::byte, kTransactionHeaderSize> header,
              std::span<std::byte> data, std::uint8_t flag) {
    cipher_.encode_stream(encode_stream_, header);
    if (data.empty()) {
      return;
    }
    if (flag != 0) {
      const std::size_t prefix = std::min<std::size_t>(2, data.size());
      cipher_.encode_stream(encode_stream_, data.first(prefix));
      permute_key<H>(encode_key_, session_key_, flag);
      cipher_.set_encode_key(encode_key_);
      cipher_.encode_stream(encode_stream_, data.subspan(prefix));
    } else {
      cipher_.encode_stream(encode_stream_, data);
    }
  }

  // Decodes the header in place; returns the flag read after the decode.
  // Split from decode_data so callers can enforce the receive policy on
  // the DECODED sizes (the legacy order: header decode -> policy check ->
  // data decode).
  [[nodiscard]] auto decode_header(std::span<std::byte, kTransactionHeaderSize> header)
      -> std::uint8_t {
    cipher_.decode_stream(decode_stream_, header);
    return std::to_integer<std::uint8_t>(header[0]);
  }

  // Decodes the data in place for the given header flag (2-byte old-key
  // prefix + permute for flag != 0, whole buffer for flag == 0).
  void decode_data(std::span<std::byte> data, std::uint8_t flag) {
    if (data.empty()) {
      return;
    }
    if (flag != 0) {
      const std::size_t prefix = std::min<std::size_t>(2, data.size());
      cipher_.decode_stream(decode_stream_, data.first(prefix));
      permute_key<H>(decode_key_, session_key_, flag);
      cipher_.set_decode_key(decode_key_);
      cipher_.decode_stream(decode_stream_, data.subspan(prefix));
    } else {
      cipher_.decode_stream(decode_stream_, data);
    }
  }

  // Decodes header + data in place; returns the header flag (the legacy
  // receive path).
  [[nodiscard]] auto decode(std::span<std::byte, kTransactionHeaderSize> header,
                            std::span<std::byte> data) -> std::uint8_t {
    const std::uint8_t flag = decode_header(header);
    decode_data(data, flag);
    return flag;
  }

  // The historical sender's flag distribution (UTransact.cpp:922-938):
  // r0 >> 4 is used directly unless it is 2, 7 or 13, in which case r1 >> 2
  // replaces it; if that is 0, (r2 >> 3) + 1 replaces it; anything else
  // becomes 0. Reproduced as a pure function for tests and faithful
  // senders.
  [[nodiscard]] static constexpr auto legacy_flag_quirk(std::uint32_t r0, std::uint32_t r1,
                                                        std::uint32_t r2) noexcept -> std::uint8_t {
    std::uint8_t flag = static_cast<std::uint8_t>(r0 >> 4);
    if (flag == 2 || flag == 7 || flag == 13) {
      flag = static_cast<std::uint8_t>(r1 >> 2);
      if (flag == 0) {
        flag = static_cast<std::uint8_t>((r2 >> 3) + 1);
      }
    } else {
      flag = 0;
    }
    return flag;
  }

 private:
  std::vector<std::byte> session_key_;
  std::vector<std::byte> encode_key_;
  std::vector<std::byte> decode_key_;
  aw::crypto::Blowfish cipher_;
  aw::crypto::Blowfish::Ofb64 encode_stream_;
  aw::crypto::Blowfish::Ofb64 decode_stream_;
};

}  // namespace hotline::protocol::auth
