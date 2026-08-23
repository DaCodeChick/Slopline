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

# Publish the component switches so headers and consumers can gate
# role-specific code at compile time.
if(BUILD_CLIENT)
  target_compile_definitions(hotline_net PUBLIC HOTLINE_BUILD_CLIENT=1)
else()
  target_compile_definitions(hotline_net PUBLIC HOTLINE_BUILD_CLIENT=0)
endif()
if(BUILD_SERVER)
  target_compile_definitions(hotline_net PUBLIC HOTLINE_BUILD_SERVER=1)
else()
  target_compile_definitions(hotline_net PUBLIC HOTLINE_BUILD_SERVER=0)
endif()

hotline_enable_warnings(hotline_net)
hotline_enable_sanitizers(hotline_net)
