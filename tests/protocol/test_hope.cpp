#include "hotline/protocol/hope.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "appwarrior/crypto/md5.h"
#include "appwarrior/crypto/sha1.h"
#include "appwarrior/testing.h"

using namespace hotline::protocol;
using namespace hotline::protocol::auth::hope;
using namespace aw::test;

AW_TEST_CASE("stage 1: golden request (login/password zero bytes, algorithm lists)") {
  const FieldList request = build_stage1_login();
  AW_REQUIRE_BYTES_MSG(
      unwrap(encode_field_list(request)),
      "00 04"
      " 00 69 00 01 00"
      " 00 6a 00 01 00"
      " 0e 04 00 0c 00 02 09 48 4d 41 43 2d 53 48 41 31"
      " 0e c2 00 0b 00 01 08 42 4c 4f 57 46 49 53 48",
      "HOPE stage-1 request");
}

AW_TEST_CASE("server algorithm parsing follows the legacy name comparison") {
  const auto sha1 = parse_server_mac_algorithm(bytes_from_hex("00 01 09 48 4d 41 43 2d 53 48 41 31"));
  AW_CHECK(sha1.has_value() && *sha1 == MacAlgorithm::hmac_sha1);

  const auto md5 = parse_server_mac_algorithm(bytes_from_hex("00 01 08 48 4d 41 43 2d 4d 44 35"));
  AW_CHECK(md5.has_value() && *md5 == MacAlgorithm::hmac_md5);

  const auto bogus = parse_server_mac_algorithm(bytes_from_hex("00 01 08 42 4c 4f 57 46 49 53 48"));
  AW_CHECK(!bogus.has_value());
  AW_CHECK(bogus.error() == HopeError::unsupported_mac_algorithm);

  const auto short_form = parse_server_mac_algorithm(bytes_from_hex("00"));
  AW_CHECK(!short_form.has_value());
  AW_CHECK(short_form.error() == HopeError::malformed);

  const auto cipher = parse_server_cipher_algorithm(bytes_from_hex("00 01 08 42 4c 4f 57 46 49 53 48"));
  AW_CHECK(cipher.has_value() && *cipher == CipherAlgorithm::blowfish);

  const auto bad_cipher = parse_server_cipher_algorithm(bytes_from_hex("00 01 03 41 45 53"));
  AW_CHECK(!bad_cipher.has_value());
  AW_CHECK(bad_cipher.error() == HopeError::unsupported_cipher_algorithm);
}

AW_TEST_CASE("login digests: golden vectors from an independent implementation") {
  const std::vector<std::byte> session_key = bytes_from_hex("01 02 03 04 05 06 07 08");
  const std::vector<std::byte> login = bytes_from_ascii("alice");
  const std::vector<std::byte> password = bytes_from_ascii("secret");

  {
    const auto [mac_login, mac_password] = login_digests<aw::crypto::Sha1>(login, password, session_key);
    AW_REQUIRE_BYTES(mac_login, "7c 30 42 37 68 e0 98 1e 71 d0 85 00 18 02 ca 52 1b b3 2d 48");
    AW_REQUIRE_BYTES(mac_password, "00 5d 56 e3 d7 ed a0 2a bf be e2 e8 8d a8 8b f4 75 30 0d 6d");
  }
  {
    const auto [mac_login, mac_password] = login_digests<aw::crypto::Md5>(login, password, session_key);
    AW_REQUIRE_BYTES(mac_login, "22 33 4c d8 09 4e a4 02 ae a6 14 0a ae 08 f0 41");
    AW_REQUIRE_BYTES(mac_password, "84 c8 d9 4e 1d 8d 79 ce 49 af a8 60 2b 5c d3 9b");
  }
}

AW_TEST_CASE("stage 2: digest login echoes the server cipher field and Vers 197") {
  const std::vector<std::byte> session_key = bytes_from_hex("01 02 03 04 05 06 07 08");
  const std::vector<std::byte> login = bytes_from_ascii("alice");
  const std::vector<std::byte> password = bytes_from_ascii("secret");
  const std::vector<std::byte> server_cipher = bytes_from_hex("00 01 08 42 4c 4f 57 46 49 53 48");

  const FieldList list =
      build_stage2_login<aw::crypto::Sha1>(login, password, session_key, server_cipher);

  AW_CHECK(list.fields.size() == 4U);
  AW_CHECK(list.fields[0].id == FieldId::UserLogin);
  AW_REQUIRE_BYTES(list.fields[0].data, "7c 30 42 37 68 e0 98 1e 71 d0 85 00 18 02 ca 52 1b b3 2d 48");
  AW_CHECK(list.fields[1].id == FieldId::UserPassword);
  AW_REQUIRE_BYTES(list.fields[1].data, "00 5d 56 e3 d7 ed a0 2a bf be e2 e8 8d a8 8b f4 75 30 0d 6d");
  AW_CHECK(list.fields[2].id == FieldId::ClientCipherAlg);
  AW_REQUIRE_BYTES(list.fields[2].data, "00 01 08 42 4c 4f 57 46 49 53 48");
  AW_CHECK(list.fields[3].id == FieldId::Vers);
  AW_REQUIRE_BYTES(list.fields[3].data, "00 c5");
}

AW_TEST_CASE("compatible server reply: session key + single-entry algorithm fields") {
  const std::vector<std::byte> session_key(32, std::byte{0x11});

  const FieldList reply = build_server_stage2_reply(session_key, MacAlgorithm::hmac_sha1);
  AW_CHECK(reply.fields.size() == 3U);
  AW_CHECK(reply.fields[0].id == FieldId::SessionKey);
  AW_CHECK(reply.fields[0].data.size() == 32U);
  AW_CHECK(reply.fields[1].id == FieldId::MacAlg);
  AW_REQUIRE_BYTES(reply.fields[1].data, "00 01 09 48 4d 41 43 2d 53 48 41 31");
  AW_CHECK(reply.fields[2].id == FieldId::ServerCipherAlg);
  AW_REQUIRE_BYTES(reply.fields[2].data, "00 01 08 42 4c 4f 57 46 49 53 48");

  const FieldList md5_reply = build_server_stage2_reply(session_key, MacAlgorithm::hmac_md5);
  AW_REQUIRE_BYTES(field_data(md5_reply, FieldId::MacAlg), "00 01 08 48 4d 41 43 2d 4d 44 35");
}
