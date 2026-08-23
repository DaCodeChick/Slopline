# AppWarrior crypto — general-purpose message-digest primitives.
#
# MD5/SHA-1 (RFC implementations) and generic RFC 2104 HMAC. Framework
# facilities: not Hotline-specific. Hotline's legacy login key schedule
# builds on them and lives in the Hotline layer. Blowfish lands here too
# when the encrypted-transaction layer is modernized.

add_library(appwarrior_crypto STATIC
  src/md5.cpp
  src/sha1.cpp
)
add_library(appwarrior::crypto ALIAS appwarrior_crypto)

target_include_directories(appwarrior_crypto PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_compile_features(appwarrior_crypto PUBLIC cxx_std_23)

hotline_enable_warnings(appwarrior_crypto)
hotline_enable_sanitizers(appwarrior_crypto)
