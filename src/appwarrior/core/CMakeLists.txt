# AppWarrior Core — platform-independent framework foundations.
#
# Pure C++23 utilities shared by every AppWarrior module and by the Hotline
# applications. No platform, no I/O, no UI. Contents so far:
#   appwarrior/core/endian.h   — big-endian encoding helpers (Phase 2a)
#   appwarrior/core/bits.h     — MSB-first bit access (Phase 2)
#   appwarrior/core/align.h    — power-of-two alignment rounding (Phase 2)
#   appwarrior/core/ivar_array.h — bounded 'IVA1' catalog decoder (Phase 2)

add_library(appwarrior_core STATIC
  src/ivar_array.cpp
)
add_library(appwarrior::core ALIAS appwarrior_core)

target_include_directories(appwarrior_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_compile_features(appwarrior_core PUBLIC cxx_std_23)

hotline_enable_warnings(appwarrior_core)
hotline_enable_sanitizers(appwarrior_core)
