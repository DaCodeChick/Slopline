// Hotline wire protocol: legacy login key schedule.
//
// Reproduces the historical HLCrypt::Init / PermEncodeKey / PermDecodeKey
// semantics exactly (legacy AppWarrior Crypt/HLCrypt.cpp:16-74):
//
//   temp1 = HMAC(password, sessionKey)   (applied twice)
//   temp2 = HMAC(password, temp1)
//   client: encodeKey = temp2, decodeKey = temp1
//   server: encodeKey = temp1, decodeKey = temp2
//   Perm(n): key = HMAC(sessionKey, key), n times
//
// The HMAC hash choice is negotiated at login (myField_MacAlg: "HMAC-SHA1"
// or "HMAC-MD5"), which is why these functions are templates over the
// digest type rather than a runtime-polymorphic interface. The generic
// HMAC primitive itself is framework-level (aw::crypto::hmac).

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "appwarrior/crypto/hmac.h"

namespace hotline::protocol::auth {

// The two keys of the historical HLCrypt::Init schedule. `first` is the
// legacy "temp1" and `second` is "temp2"; which one a side uses to encode
// vs decode depends on whether it is the client (see header comment).
struct LoginKeys {
  std::vector<std::byte> first;
  std::vector<std::byte> second;
};

template <aw::crypto::message_digest H>
[[nodiscard]] auto derive_login_keys(std::span<const std::byte> password,
                                     std::span<const std::byte> session_key) -> LoginKeys {
  LoginKeys keys;
  keys.first = aw::crypto::hmac<H>(password, session_key);
  keys.first = aw::crypto::hmac<H>(password, keys.first);
  keys.second = aw::crypto::hmac<H>(password, keys.first);
  return keys;
}

// HLCrypt::PermEncodeKey/PermDecodeKey: replace the key with
// HMAC(sessionKey, key), `rounds` times, in place.
template <aw::crypto::message_digest H>
void permute_key(std::vector<std::byte>& key, std::span<const std::byte> session_key,
                 std::uint32_t rounds) {
  while (rounds-- > 0) {
    key = aw::crypto::hmac<H>(session_key, key);
  }
}

}  // namespace hotline::protocol::auth
