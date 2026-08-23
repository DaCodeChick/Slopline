# Hotline — modern C++23 implementation
#
# Modernization of the Hotline client/server/tracker and the AppWarrior
# framework. Historical reference (read-only): legacy/ (Schala/Gloarbline).
#
# See AGENTS.md for the modernization charter, HOTLINE_MODERNIZATION_REPORT.md
# for the archaeology, and docs/modernization-ledger.md for the phase ledger.

cmake_minimum_required(VERSION 3.28)

project(Hotline
        VERSION 0.1.0
        DESCRIPTION "Hotline client/server/tracker and AppWarrior framework, modernized in C++23"
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(HOTLINE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(HOTLINE_BUILD_TESTS "Build Hotline tests" ON)
option(HOTLINE_SANITIZE "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(BUILD_MONOLITHIC "Build AppWarrior as a single monolithic library" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(Sanitizers)

add_subdirectory(src/appwarrior)
add_subdirectory(src/hotline/protocol)

if(HOTLINE_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
