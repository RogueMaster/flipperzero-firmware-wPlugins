# SPDX-License-Identifier: GPL-2.0-or-later

function(tc_sm_fixture_header target)
  find_package(Python3 COMPONENTS Interpreter QUIET)
  if(NOT Python3_Interpreter_FOUND)
    return()
  endif()
  set(header ${CMAKE_CURRENT_BINARY_DIR}/generated/sm_fixtures.h)
  if(NOT TARGET tiny_crypto_c-sm-fixtures)
    add_custom_command(OUTPUT ${header}
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/sm_fixtures.py
        --output ${header}
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tools/sm_fixtures.py
              ${CMAKE_CURRENT_SOURCE_DIR}/tools/pki_fixtures.py VERBATIM)
    add_custom_target(tiny_crypto_c-sm-fixtures DEPENDS ${header})
  endif()
  add_dependencies(${target} tiny_crypto_c-sm-fixtures)
  target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
  target_compile_definitions(${target} PRIVATE TC_TEST_SM_FIXTURES=1)
endfunction()
