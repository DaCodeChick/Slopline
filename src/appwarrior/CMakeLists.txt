# AppWarrior — lightweight native cross-platform C++23 application framework.
#
# ONE monolithic static library (per project decision; see AGENTS.md
# "AppWarrior Modularity"). Components are organized by directory and
# namespace (aw::endian/bits/align/ivar/guid, aw::crypto, aw::test; UI and
# platform backends join later under their own subdirectories). Because the
# library is STATIC, the linker only pulls in objects that are actually
# referenced — a headless Hotline server never links unused code, and the
# GUI/backend stacks added later cost nothing for consumers that don't use
# them. C++ namespaces use the short prefix `aw`; the CMake target uses the
# full framework name (`appwarrior::appwarrior`); test macros are
# AW_-prefixed (macros cannot be namespaced).

add_library(appwarrior STATIC
  core/src/ivar_array.cpp
  crypto/src/md5.cpp
  crypto/src/sha1.cpp
  crypto/src/blowfish.cpp
)
add_library(appwarrior::appwarrior ALIAS appwarrior)

target_include_directories(appwarrior PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core/include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/crypto/include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/testing/include>
)
target_compile_features(appwarrior PUBLIC cxx_std_23)

hotline_enable_warnings(appwarrior)
hotline_enable_sanitizers(appwarrior)
