add_executable(hotline_protocol_tests
  test_main.cpp
  appwarrior/test_endian.cpp
  appwarrior/test_bits.cpp
  appwarrior/test_align.cpp
  appwarrior/test_ivar.cpp
  crypto/test_md5.cpp
  crypto/test_sha1.cpp
  crypto/test_hmac.cpp
  protocol/test_constants.cpp
  protocol/test_transaction.cpp
  protocol/test_field_list.cpp
  protocol/test_handshake.cpp
  protocol/test_payload.cpp
  protocol/test_auth.cpp
)
target_link_libraries(hotline_protocol_tests PRIVATE hotline::protocol hotline::crypto
                                                      appwarrior::testing appwarrior::core)

hotline_enable_warnings(hotline_protocol_tests)
hotline_enable_sanitizers(hotline_protocol_tests)

add_test(NAME protocol COMMAND hotline_protocol_tests)
