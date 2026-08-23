# Compiler warning policy.
#
# Protocol code deals in explicit integer widths and byte order, so
# conversion warnings are enabled: they catch real width/endian bugs that
# are easy to introduce in codec work. Warnings are errors by default
# (HOTLINE_WARNINGS_AS_ERRORS); there is deliberately no per-target
# warning-suppression escape hatch — fix the code instead (AGENTS.md).

function(hotline_enable_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive- /Zc:preprocessor)
    if(HOTLINE_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  else()
    target_compile_options(${target} PRIVATE
      -Wall
      -Wextra
      -Wpedantic
      -Wconversion
      -Wsign-conversion
      -Wshadow
      -Wformat=2
      -Wundef
      -Wnull-dereference
      -Wuninitialized)
    if(HOTLINE_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  endif()
endfunction()
