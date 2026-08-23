# hotline::client — the Hotline client application project.
#
# Currently carries the client-side connection (ClientConnection over the
# role-neutral hotline::net::ConnectionBase); the client application
# targets (core, UI-facing services, the executable) land here in later
# phases. Built only when BUILD_CLIENT is ON.

add_library(hotline_client STATIC
  src/connection.cpp
)
add_library(hotline::client ALIAS hotline_client)

target_include_directories(hotline_client PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_compile_features(hotline_client PUBLIC cxx_std_23)
target_link_libraries(hotline_client PUBLIC hotline::net)

hotline_enable_warnings(hotline_client)
hotline_enable_sanitizers(hotline_client)
