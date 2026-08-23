add_executable(hotline_protocol_tests
  test_main.cpp
  appwarrior/test_endian.cpp
  appwarrior/test_bits.cpp
  appwarrior/test_align.cpp
  appwarrior/test_ivar.cpp
  appwarrior/test_md5.cpp
  appwarrior/test_sha1.cpp
  appwarrior/test_hmac.cpp
  appwarrior/test_guid.cpp
  appwarrior/test_blowfish.cpp
  protocol/test_constants.cpp
  protocol/test_transaction.cpp
  protocol/test_field_list.cpp
  protocol/test_handshake.cpp
  protocol/test_payload.cpp
  protocol/test_auth.cpp
  protocol/test_key_schedule.cpp
  protocol/test_transaction_cipher.cpp
  protocol/test_hope.cpp
)
target_link_libraries(hotline_protocol_tests PRIVATE hotline::protocol appwarrior::crypto
                                                      appwarrior::testing appwarrior::core)

hotline_enable_warnings(hotline_protocol_tests)
hotline_enable_sanitizers(hotline_protocol_tests)

add_test(NAME protocol COMMAND hotline_protocol_tests)
