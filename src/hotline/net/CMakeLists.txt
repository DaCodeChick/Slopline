# hotline::net — Hotline connection/session layer over aw::net.
#
# TRTP/HTXF establish, transaction framing + reassembly, the historical
# receive policy, encrypted-transaction hooks, and the login session state
# machine. Depends on hotline::protocol (codecs) and the AppWarrior
# framework (transport).

add_library(hotline_net STATIC
  src/connection.cpp
  src/session.cpp
)
add_library(hotline::net ALIAS hotline_net)

target_include_directories(hotline_net PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_compile_features(hotline_net PUBLIC cxx_std_23)
target_link_libraries(hotline_net PUBLIC hotline::protocol appwarrior::framework)

hotline_enable_warnings(hotline_net)
hotline_enable_sanitizers(hotline_net)
