# SPDX-License-Identifier: GPL-2.0-or-later

set(tc_sanitize_flags)
if(NOT TINY_CRYPTO_SANITIZE STREQUAL "")
  if(MSVC)
    message(FATAL_ERROR "TINY_CRYPTO_SANITIZE requires GCC or Clang")
  endif()
  set(tc_sanitize_flags -O1 -fsanitize=${TINY_CRYPTO_SANITIZE}
      -fno-omit-frame-pointer)
endif()

function(tc_use_test_sanitizers target)
  if(tc_sanitize_flags)
    target_compile_options(${target} PRIVATE ${tc_sanitize_flags})
    get_target_property(kind ${target} TYPE)
    if(kind STREQUAL "STATIC_LIBRARY")
      # Consumers must link the runtime required by instrumented objects.
      target_link_options(${target} INTERFACE ${tc_sanitize_flags})
    else()
      target_link_options(${target} PRIVATE ${tc_sanitize_flags})
    endif()
  endif()
endfunction()
