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
  appwarrior/test_net.cpp
  protocol/test_constants.cpp
  protocol/test_transaction.cpp
  protocol/test_field_list.cpp
  protocol/test_handshake.cpp
  protocol/test_payload.cpp
  protocol/test_auth.cpp
  protocol/test_key_schedule.cpp
  protocol/test_transaction_cipher.cpp
  protocol/test_hope.cpp
  protocol/test_transfer.cpp
  protocol/test_archive.cpp
  protocol/test_tracker.cpp
)
target_link_libraries(hotline_protocol_tests PRIVATE hotline::protocol appwarrior::framework)

hotline_enable_warnings(hotline_protocol_tests)
hotline_enable_sanitizers(hotline_protocol_tests)

add_test(NAME protocol COMMAND hotline_protocol_tests)

# hotline_net_tests covers the connection/session layer. Sources are
# gated by the application-component switches: the shared-layer tests
# (base machinery + the bilateral login session) always build, the
# client/server role tests build with their component, and the cross-role
# integration tests need both.
add_executable(hotline_net_tests
  test_main.cpp
  net/test_base.cpp
  net/test_session.cpp
)
if(BUILD_CLIENT)
  target_sources(hotline_net_tests PRIVATE net/test_client.cpp)
  target_link_libraries(hotline_net_tests PRIVATE hotline::client)
endif()
if(BUILD_SERVER)
  target_sources(hotline_net_tests PRIVATE net/test_server.cpp)
  target_link_libraries(hotline_net_tests PRIVATE hotline::server)
endif()
if(BUILD_CLIENT AND BUILD_SERVER)
  target_sources(hotline_net_tests PRIVATE net/test_both.cpp)
endif()
target_link_libraries(hotline_net_tests PRIVATE hotline::net appwarrior::framework)

hotline_enable_warnings(hotline_net_tests)
hotline_enable_sanitizers(hotline_net_tests)

add_test(NAME net COMMAND hotline_net_tests)
