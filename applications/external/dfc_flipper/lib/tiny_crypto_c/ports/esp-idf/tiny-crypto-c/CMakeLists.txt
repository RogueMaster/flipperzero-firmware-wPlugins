# SPDX-License-Identifier: GPL-2.0-or-later
idf_component_register()
if(CMAKE_BUILD_EARLY_EXPANSION)
  return()
endif()

# Use the same source selection and public definitions as a host consumer.
set(TINY_CRYPTO_BUILD_TESTS OFF)
set(TINY_CRYPTO_BUILD_BENCHMARKS OFF)
set(tc_platform_features TC_ENABLE_SHA512)
if(CONFIG_SECURE_SIGNED_ON_UPDATE AND CONFIG_SECURE_SIGNED_APPS_RSA_SCHEME)
  list(APPEND tc_platform_features TC_ENABLE_RSA)
endif()
if(CONFIG_SECURE_SIGNED_ON_UPDATE AND CONFIG_SECURE_SIGNED_APPS_ECDSA_V2_SCHEME)
  list(APPEND tc_platform_features TC_ENABLE_EC TC_EC_ENABLE_P192 TC_EC_ENABLE_P256)
endif()
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../../.." "${CMAKE_CURRENT_BINARY_DIR}/library")
target_link_libraries(${COMPONENT_LIB} INTERFACE tiny_crypto_c::tiny_crypto_c)
