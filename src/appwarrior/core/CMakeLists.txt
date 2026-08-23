# AppWarrior Core — platform-independent framework foundations.
#
# Pure C++23 utilities shared by every AppWarrior module and by the Hotline
# applications. No platform, no I/O, no UI. First content: the big-endian
# encoding helpers (appwarrior/core/endian.h); types and the container layer
# land in the next increments (docs/modernization-ledger.md).

add_library(appwarrior_core INTERFACE)
add_library(appwarrior::core ALIAS appwarrior_core)

target_include_directories(appwarrior_core INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_compile_features(appwarrior_core INTERFACE cxx_std_23)
