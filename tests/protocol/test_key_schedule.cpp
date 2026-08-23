#include "hotline/protocol/key_schedule.h"

#include <cstddef>
#include <span>
#include <vector>

#include "appwarrior/crypto/md5.h"
#include "appwarrior/crypto/sha1.h"
#include "appwarrior/testing.h"

using namespace hotline::protocol::auth;
using namespace aw::crypto;
using namespace aw::test;

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
