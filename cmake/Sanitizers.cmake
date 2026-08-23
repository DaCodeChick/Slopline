# Sanitizer configuration (AGENTS.md: keep ASan/UBSan targets practical).

function(hotline_enable_sanitizers target)
  if(HOTLINE_SANITIZE)
    target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(${target} PRIVATE -fsanitize=address,undefined)
  endif()
endfunction()
