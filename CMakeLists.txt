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

# Hotline application components. Each gates the code of the respective
# component (connection role cores, session, and the future application
# libraries) behind compile-time switches — a server-only build carries no
# client code and vice versa.
option(BUILD_CLIENT "Build the Hotline client component" ON)
option(BUILD_SERVER "Build the Hotline server component" ON)
option(BUILD_TRACKER "Build the Hotline tracker component (future application; the tracker wire codecs are always available)" ON)

# AppWarrior library shape — two independent axes:
#   BUILD_SHARED_LIBS   shared (ON) vs static (OFF) — governs the AppWarrior
#                       libraries (they are declared without a fixed type).
#   BUILD_MONOLITHIC    one 'appwarrior' library (ON) vs per-component
#                       targets appwarrior::core/crypto/testing (OFF).
# All four combinations are supported; consumers always link the aggregate
# target appwarrior::framework.
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(BUILD_MONOLITHIC "Build AppWarrior as a single monolithic library" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(Sanitizers)

add_subdirectory(src/appwarrior)
add_subdirectory(src/hotline/protocol)
add_subdirectory(src/hotline/net)

if(HOTLINE_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
