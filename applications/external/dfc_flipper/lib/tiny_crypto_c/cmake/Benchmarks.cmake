# SPDX-License-Identifier: GPL-2.0-or-later

option(TINY_CRYPTO_BUILD_BENCHMARKS "Build benchmarks for the selected profile"
       ${PROJECT_IS_TOP_LEVEL})
if(TINY_CRYPTO_BUILD_BENCHMARKS)
  add_library(tiny_crypto_c-benchmark-support STATIC EXCLUDE_FROM_ALL
    benchmarks/consume.c)
  set_property(TARGET tiny_crypto_c-benchmark-support
    PROPERTY INTERPROCEDURAL_OPTIMIZATION FALSE)
  tc_warnings(tiny_crypto_c-benchmark-support)
  tc_use_test_sanitizers(tiny_crypto_c-benchmark-support)
  set(tc_benchmarks)
  set(tc_benchmark_commands)
  set(tc_benchmark_algorithms hash aes des kdf tlv ec piv_sm)
  find_package(Python3 COMPONENTS Interpreter QUIET)
  if(Python3_Interpreter_FOUND)
    list(APPEND tc_benchmark_algorithms pki)
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/generated/pki_fixtures.h
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/pki_fixtures.py
        --output ${CMAKE_CURRENT_BINARY_DIR}/generated/pki_fixtures.h
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tools/pki_fixtures.py VERBATIM)
  endif()
  foreach(algorithm IN LISTS tc_benchmark_algorithms)
    # Each executable reports skipped operations when its profile omits them.
    add_executable(benchmark_${algorithm} EXCLUDE_FROM_ALL
      benchmarks/${algorithm}.c)
    target_link_libraries(benchmark_${algorithm} PRIVATE
      tiny_crypto_c tiny_crypto_c-benchmark-support)
    target_compile_definitions(benchmark_${algorithm} PRIVATE
      TC_BENCHMARK_BUILD_TYPE="$<CONFIG>"
      TC_BENCHMARK_COMPILER="${CMAKE_C_COMPILER_ID}-${CMAKE_C_COMPILER_VERSION}"
      TC_BENCHMARK_SANITIZE="${TINY_CRYPTO_SANITIZE}")
    tc_warnings(benchmark_${algorithm})
    tc_use_test_sanitizers(benchmark_${algorithm})
    list(APPEND tc_benchmarks benchmark_${algorithm})
    list(APPEND tc_benchmark_commands COMMAND $<TARGET_FILE:benchmark_${algorithm}>)
  endforeach()
  if(TARGET benchmark_pki)
    target_sources(benchmark_pki PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated/pki_fixtures.h)
    target_include_directories(benchmark_pki PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
  endif()
  if(TINY_CRYPTO_ENABLE_PIV_SM_APDU)
    tc_sm_fixture_header(benchmark_piv_sm)
  endif()
  add_custom_target(benchmark ${tc_benchmark_commands}
    DEPENDS ${tc_benchmarks}
    COMMENT "Benchmarking the selected firmware profile")
endif()
