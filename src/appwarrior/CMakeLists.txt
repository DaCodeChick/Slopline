# Library shape — two independent axes, declared at the top level:
#   BUILD_SHARED_LIBS   shared (ON) vs static (OFF).
#   BUILD_MONOLITHIC    one 'appwarrior' library (ON) vs per-component
#                       targets appwarrior::core/crypto/testing (OFF).
#
# Consumers link the aggregate target appwarrior::framework, which resolves
# to the correct set of targets in either mode. Windows DLL exports use the
# AW_API macro (appwarrior/export.h) — no blanket WINDOWS_EXPORT_ALL_SYMBOLS.
# Components are organized by directory and namespace (aw::endian/bits/
# align/ivar/guid, aw::crypto, aw::test); UI and platform backends join
# later under their own subdirectories. C++ namespaces use the short prefix
# `aw`; test macros are AW_-prefixed.

if(BUILD_MONOLITHIC)
  add_library(appwarrior
    core/src/ivar_array.cpp
    crypto/src/md5.cpp
    crypto/src/sha1.cpp
    crypto/src/blowfish.cpp
    net/src/address.cpp
    net/src/socket.cpp
    net/src/poller.cpp
  )
  add_library(appwarrior::appwarrior ALIAS appwarrior)

  target_include_directories(appwarrior PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/crypto/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/testing/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/net/include>
  )
  target_compile_features(appwarrior PUBLIC cxx_std_23)
  hotline_enable_warnings(appwarrior)
  hotline_enable_sanitizers(appwarrior)

  if(WIN32)
    target_link_libraries(appwarrior PUBLIC ws2_32)
  endif()
  if(BUILD_SHARED_LIBS)
    set_target_properties(appwarrior PROPERTIES
      VERSION ${PROJECT_VERSION}
      SOVERSION ${PROJECT_VERSION_MAJOR}
    )
    target_compile_definitions(appwarrior PRIVATE AW_BUILDING_LIBRARY)
  endif()

  add_library(appwarrior_framework INTERFACE)
  target_link_libraries(appwarrior_framework INTERFACE appwarrior)
else()
  add_library(appwarrior_core
    core/src/ivar_array.cpp
  )
  add_library(appwarrior::core ALIAS appwarrior_core)

  add_library(appwarrior_crypto
    crypto/src/md5.cpp
    crypto/src/sha1.cpp
    crypto/src/blowfish.cpp
  )
  add_library(appwarrior::crypto ALIAS appwarrior_crypto)
  target_link_libraries(appwarrior_crypto PUBLIC appwarrior_core)

  add_library(appwarrior_net
    net/src/address.cpp
    net/src/socket.cpp
    net/src/poller.cpp
  )
  add_library(appwarrior::net ALIAS appwarrior_net)
  target_link_libraries(appwarrior_net PUBLIC appwarrior_core)

  add_library(appwarrior_testing INTERFACE)
  add_library(appwarrior::testing ALIAS appwarrior_testing)

  target_include_directories(appwarrior_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core/include>
  )
  target_include_directories(appwarrior_crypto PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/crypto/include>
  )
  target_include_directories(appwarrior_testing INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/testing/include>
  )
  target_include_directories(appwarrior_net PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/net/include>
  )

  target_compile_features(appwarrior_core PUBLIC cxx_std_23)
  target_compile_features(appwarrior_crypto PUBLIC cxx_std_23)
  target_compile_features(appwarrior_net PUBLIC cxx_std_23)
  if(WIN32)
    target_link_libraries(appwarrior_net PUBLIC ws2_32)
  endif()
  target_compile_features(appwarrior_testing INTERFACE cxx_std_23)
  hotline_enable_warnings(appwarrior_core)
  hotline_enable_warnings(appwarrior_crypto)
  hotline_enable_warnings(appwarrior_net)
  hotline_enable_sanitizers(appwarrior_core)
  hotline_enable_sanitizers(appwarrior_crypto)
  hotline_enable_sanitizers(appwarrior_net)

  if(BUILD_SHARED_LIBS)
    set_target_properties(appwarrior_core appwarrior_crypto appwarrior_net PROPERTIES
      VERSION ${PROJECT_VERSION}
      SOVERSION ${PROJECT_VERSION_MAJOR}
    )
    target_compile_definitions(appwarrior_core PRIVATE AW_BUILDING_LIBRARY)
    target_compile_definitions(appwarrior_crypto PRIVATE AW_BUILDING_LIBRARY)
    target_compile_definitions(appwarrior_net PRIVATE AW_BUILDING_LIBRARY)
  endif()

  add_library(appwarrior_framework INTERFACE)
  target_link_libraries(appwarrior_framework INTERFACE
    appwarrior_core
    appwarrior_crypto
    appwarrior_net
    appwarrior_testing
  )
endif()

add_library(appwarrior::framework ALIAS appwarrior_framework)
