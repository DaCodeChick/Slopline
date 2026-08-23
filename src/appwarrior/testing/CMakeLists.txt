# AppWarrior testing component.
#
# The framework's built-in, dependency-free unit-test facility (registry,
# assertion macros, byte helpers, runner). Header-only; no build products
# beyond the interface target. First modern AppWarrior component to land —
# see docs/modernization-ledger.md.

add_library(appwarrior_testing INTERFACE)
add_library(appwarrior::testing ALIAS appwarrior_testing)

target_include_directories(appwarrior_testing INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_compile_features(appwarrior_testing INTERFACE cxx_std_23)
