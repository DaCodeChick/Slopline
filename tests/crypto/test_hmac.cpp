#include "hotline/crypto/hmac.h"
#include "hotline/crypto/md5.h"
#include "hotline/crypto/sha1.h"

#include <cstddef>
#include <span>
#include <vector>

#include "appwarrior/testing.h"

using namespace hotline::crypto;
using namespace appwarrior::test;

AW_TEST_CASE("HMAC-MD5: RFC 2202 test vectors") {
  const std::vector<std::byte> key_0b(16, std::byte{0x0B});
  const std::vector<std::byte> hi_there = bytes_from_ascii("Hi There");
  AW_REQUIRE_BYTES(hmac<Md5>(key_0b, hi_there), "92 94 72 7a 36 38 bb 1c 13 f4 8e f8 15 8b fc 9d");

  const std::vector<std::byte> jefe = bytes_from_ascii("Jefe");
  const std::vector<std::byte> what = bytes_from_ascii("what do ya want for nothing?");
  AW_REQUIRE_BYTES(hmac<Md5>(jefe, what), "75 0c 78 3e 6a b0 b5 03 ea a8 6e 31 0a 5d b7 38");

  // audit/06 §11.1: HMAC-MD5 with empty key and text.
  AW_REQUIRE_BYTES(hmac<Md5>({}, {}), "74 e6 f7 29 8a 9c 2d 16 89 35 f5 8c 00 1b ad 88");
}

AW_TEST_CASE("HMAC-SHA1: RFC 2202 test vectors") {
  const std::vector<std::byte> key_0b(20, std::byte{0x0B});
  const std::vector<std::byte> hi_there = bytes_from_ascii("Hi There");
  AW_REQUIRE_BYTES(hmac<Sha1>(key_0b, hi_there),
                "b6 17 31 86 55 05 72 64 e2 8b c0 b6 fb 37 8c 8e f1 46 be 00");

  const std::vector<std::byte> jefe = bytes_from_ascii("Jefe");
  const std::vector<std::byte> what = bytes_from_ascii("what do ya want for nothing?");
  AW_REQUIRE_BYTES(hmac<Sha1>(jefe, what),
                "ef fc df 6a e5 eb 2f a2 d2 74 16 d5 f1 84 df 9c 25 9a 7c 79");

  const std::vector<std::byte> key_aa(20, std::byte{0xAA});
  const std::vector<std::byte> dd(50, std::byte{0xDD});
  AW_REQUIRE_BYTES(hmac<Sha1>(key_aa, dd),
                "12 5d 73 42 b9 ac 11 cd 91 a3 9a f4 8a a1 7b 4f 63 f1 75 d3");
}

AW_TEST_CASE("HMAC: keys longer than the block size are hashed first") {
  // Key longer than 64 bytes forces the RFC 2104 pre-hash path
  // (cross-checked against python hmac).
  const std::vector<std::byte> long_key = bytes_from_ascii(
      "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdef");
  const std::vector<std::byte> data = bytes_from_ascii("payload");
  AW_REQUIRE_BYTES(hmac<Md5>(long_key, data), "95 7d 28 c2 22 b3 c2 56 9c 70 87 46 70 b6 55 63");
  AW_REQUIRE_BYTES(hmac<Sha1>(long_key, data),
                "4d b0 2c 24 51 94 90 94 3e 74 f0 0b 99 25 a6 ff 92 2f bd 78");
}

AW_TEST_CASE("login key schedule: golden vectors from the legacy HLCrypt flow") {
  // sessionKey = 01 02 03 04 05 06 07 08, password = "secret" — expected
  // values computed with an independent implementation (python hashlib/hmac).
  const std::vector<std::byte> session_key = bytes_from_hex("01 02 03 04 05 06 07 08");
  const std::vector<std::byte> password = bytes_from_ascii("secret");

  {
    const LoginKeys keys = derive_login_keys<Sha1>(password, session_key);
    AW_REQUIRE_BYTES(keys.first,
                  "37 ad 66 72 71 92 70 96 06 4f d7 68 1b 71 1f 80 2a 50 68 ec");
    AW_REQUIRE_BYTES(keys.second,
                  "06 aa 54 d4 82 75 5b c9 fd e3 2a 67 d1 0c ed 99 e4 a4 34 c6");

    // client: encodeKey = second, decodeKey = first
    std::vector<std::byte> encode_key = keys.second;
    permute_key<Sha1>(encode_key, session_key, 3);
    AW_REQUIRE_BYTES_MSG(encode_key,
                      "85 b8 3d 95 63 ec 7e cf 48 26 69 dd 1f c1 c0 f2 34 5c b4 67",
                      "perm3(t2) — independently verified vs python");
  }

  {
    const LoginKeys keys = derive_login_keys<Md5>(password, session_key);
    AW_REQUIRE_BYTES(keys.first, "c8 42 a9 87 17 4e 31 f0 d4 fc 3c be f3 70 6f 54");
    AW_REQUIRE_BYTES(keys.second, "0e 6a 60 2e 77 ff 6f 47 fe 8c fb 4e 10 e1 7a 32");

    std::vector<std::byte> first_key = keys.first;
    permute_key<Md5>(first_key, session_key, 3);
    AW_REQUIRE_BYTES(first_key, "f3 00 86 2e 1d 04 6d 58 fb 2d ff 83 79 ce fe 22");

    permute_key<Md5>(first_key, session_key, 0);  // zero rounds = unchanged
    AW_REQUIRE_BYTES(first_key, "f3 00 86 2e 1d 04 6d 58 fb 2d ff 83 79 ce fe 22");
  }
}
