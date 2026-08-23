# hotline::server — the Hotline server application project.
#
# Currently carries the server-side connection (ServerConnection over the
# role-neutral hotline::net::ConnectionBase) and the login session state
# machine; the server core, handlers, and executable land here in later
# phases. Built only when BUILD_SERVER is ON.

add_library(hotline_server STATIC
  src/connection.cpp
  src/session.cpp
)
add_library(hotline::server ALIAS hotline_server)

target_include_directories(hotline_server PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_compile_features(hotline_server PUBLIC cxx_std_23)
target_link_libraries(hotline_server PUBLIC hotline::net)

hotline_enable_warnings(hotline_server)
hotline_enable_sanitizers(hotline_server)
