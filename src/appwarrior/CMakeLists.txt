# AppWarrior — lightweight native cross-platform C++23 application framework.
#
# Modernized per AGENTS.md: meaningful framework architecture preserved,
# obsolete internals replaced. Components are independently buildable so a
# headless Hotline server never pays for the GUI stack. C++ namespaces use
# the short prefix `aw`; CMake target names use the full framework name
# (`appwarrior::*`), and the registration macros are AW_-prefixed.

add_subdirectory(core)
add_subdirectory(crypto)
add_subdirectory(testing)
