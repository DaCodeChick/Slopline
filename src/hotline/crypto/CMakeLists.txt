# Hotline crypto — legacy protocol cryptography, modern implementations.
#
# MD5/SHA-1/HMAC primitives (RFC implementations, RFC-vector tested) plus
# the legacy login key schedule (HLCrypt::Init / Perm*Key semantics).
# Deliberately dependency-free; Blowfish and the encrypted-transaction
# stream layer land in the next increment.

add_library(hotline_crypto STATIC
  src/md5.cpp
  src/sha1.cpp
)
add_library(hotline::crypto ALIAS hotline_crypto)

target_include_directories(hotline_crypto PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_compile_features(hotline_crypto PUBLIC cxx_std_23)

hotline_enable_warnings(hotline_crypto)
hotline_enable_sanitizers(hotline_crypto)
