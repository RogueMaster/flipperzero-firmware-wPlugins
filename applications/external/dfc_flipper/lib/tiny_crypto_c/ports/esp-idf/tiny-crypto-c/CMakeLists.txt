# SPDX-License-Identifier: GPL-2.0-or-later
idf_component_register()
if(CMAKE_BUILD_EARLY_EXPANSION)
  return()
endif()

# Use the same source selection and public definitions as a host consumer.
set(TINY_CRYPTO_BUILD_TESTS OFF)
set(TINY_CRYPTO_BUILD_BENCHMARKS OFF)
include("${CMAKE_CURRENT_LIST_DIR}/platform_features.cmake")
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../../.." "${CMAKE_CURRENT_BINARY_DIR}/library")
target_link_libraries(${COMPONENT_LIB} INTERFACE tiny_crypto_c::tiny_crypto_c)
