# hotline::protocol — Hotline wire-protocol codec library.
#
# Pure logic: no I/O, no platform dependencies, no UI. Depends on nothing
# but the C++23 standard library. Byte-for-byte compatible with the
# historical wire format (see docs/modernization-ledger.md and
# audit/06-protocol.md).

add_library(hotline_protocol STATIC
  src/transaction.cpp
  src/field_list.cpp
  src/handshake.cpp
)
add_library(hotline::protocol ALIAS hotline_protocol)

target_include_directories(hotline_protocol PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_compile_features(hotline_protocol PUBLIC cxx_std_23)

hotline_enable_warnings(hotline_protocol)
hotline_enable_sanitizers(hotline_protocol)
