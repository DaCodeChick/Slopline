# AppWarrior — lightweight native cross-platform C++23 application framework.
#
# ONE monolithic SHARED library (project decision; see AGENTS.md
# "AppWarrior Modularity"): the framework ships as a single dynamic
# library that all Hotline applications link against. Components are
# organized by directory and namespace (aw::endian/bits/align/ivar/guid,
# aw::crypto, aw::test; UI and platform backends join later under their
# own subdirectories and become part of the same library). C++ namespaces
# use the short prefix `aw`; the CMake target uses the full framework name
# (`appwarrior::appwarrior`); test macros are AW_-prefixed.

add_library(appwarrior SHARED
  core/src/ivar_array.cpp
  crypto/src/md5.cpp
  crypto/src/sha1.cpp
  crypto/src/blowfish.cpp
)
add_library(appwarrior::appwarrior ALIAS appwarrior)

set_target_properties(appwarrior PROPERTIES
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR}
  # The framework has no dllexport annotations yet; export everything for
  # Windows builds until per-symbol visibility is introduced with the UI
  # backends.
  WINDOWS_EXPORT_ALL_SYMBOLS ON
)

target_include_directories(appwarrior PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core/include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/crypto/include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/testing/include>
)
target_compile_features(appwarrior PUBLIC cxx_std_23)

hotline_enable_warnings(appwarrior)
hotline_enable_sanitizers(appwarrior)
