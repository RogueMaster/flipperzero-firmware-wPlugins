# SPDX-FileCopyrightText: Mistial Dev
# SPDX-License-Identifier: Unlicense

cmake_minimum_required(VERSION 3.10)
project(tiny-DES-c VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Product library defaults: CTR + Triple-DES only
option(TINY_DES_ENABLE_ECB "Enable ECB mode" OFF)
option(TINY_DES_ENABLE_CBC "Enable CBC mode" OFF)
option(TINY_DES_ENABLE_CTR "Enable CTR mode" ON)
option(TINY_DES_ENABLE_OFB "Enable OFB mode" OFF)
option(TINY_DES_ENABLE_CFB1 "Enable CFB1 mode" OFF)
option(TINY_DES_ENABLE_CFB8 "Enable CFB8 mode" OFF)
option(TINY_DES_ENABLE_CFB64 "Enable CFB64 mode" OFF)
option(TINY_DES_ENABLE_TDES "Enable Triple-DES (3DES)" ON)
option(TINY_DES_ENABLE_CMAC "Enable CMAC (NIST SP 800-38B)" OFF)
option(TINY_DES_ZEROIZE "Wipe stack secrets in one-shot paths" ON)
option(TINY_DES_STRICT "Enable NULL checks on classical buffer APIs" OFF)
option(TINY_DES_BUILD_TESTS "Build unit test executable" ON)
option(TINY_DES_CAVP "Run the complete TDES CAVP response corpora" OFF)

function(tiny_des_bool_to_01 opt var)
  if(${opt})
    set(${var} 1 PARENT_SCOPE)
  else()
    set(${var} 0 PARENT_SCOPE)
  endif()
endfunction()

tiny_des_bool_to_01(TINY_DES_ENABLE_ECB DES_ECB)
tiny_des_bool_to_01(TINY_DES_ENABLE_CBC DES_CBC)
tiny_des_bool_to_01(TINY_DES_ENABLE_CTR DES_CTR)
tiny_des_bool_to_01(TINY_DES_ENABLE_OFB DES_OFB)
tiny_des_bool_to_01(TINY_DES_ENABLE_CFB1 DES_CFB1)
tiny_des_bool_to_01(TINY_DES_ENABLE_CFB8 DES_CFB8)
tiny_des_bool_to_01(TINY_DES_ENABLE_CFB64 DES_CFB64)
tiny_des_bool_to_01(TINY_DES_ENABLE_TDES DES_TDES)
tiny_des_bool_to_01(TINY_DES_ENABLE_CMAC DES_CMAC)
tiny_des_bool_to_01(TINY_DES_ZEROIZE DES_ZEROIZE_VAL)
tiny_des_bool_to_01(TINY_DES_STRICT DES_STRICT_VAL)

set(TINY_DES_LIBRARY_DEFINITIONS
  DES_ENABLE_ECB=${DES_ECB}
  DES_ENABLE_CBC=${DES_CBC}
  DES_ENABLE_CTR=${DES_CTR}
  DES_ENABLE_OFB=${DES_OFB}
  DES_ENABLE_CFB1=${DES_CFB1}
  DES_ENABLE_CFB8=${DES_CFB8}
  DES_ENABLE_CFB64=${DES_CFB64}
  DES_ENABLE_TDES=${DES_TDES}
  DES_ENABLE_CMAC=${DES_CMAC}
  DES_ZEROIZE=${DES_ZEROIZE_VAL}
  DES_STRICT=${DES_STRICT_VAL}
)

# Static library (product defaults)
add_library(tiny-des-c STATIC des.c)
target_include_directories(tiny-des-c PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:include>
)
target_compile_definitions(tiny-des-c PUBLIC ${TINY_DES_LIBRARY_DEFINITIONS})

# Full-feature definitions for tests (all modes + CMAC)
set(TINY_DES_TEST_DEFINITIONS
  DES_ENABLE_ECB=1
  DES_ENABLE_CBC=1
  DES_ENABLE_CTR=1
  DES_ENABLE_OFB=1
  DES_ENABLE_CFB1=1
  DES_ENABLE_CFB8=1
  DES_ENABLE_CFB64=1
  DES_ENABLE_TDES=1
  DES_ENABLE_CMAC=1
  DES_ZEROIZE=${DES_ZEROIZE_VAL}
  DES_STRICT=${DES_STRICT_VAL}
)

if(TINY_DES_CAVP)
  list(APPEND TINY_DES_TEST_DEFINITIONS DES_CAVP=1)
else()
  list(APPEND TINY_DES_TEST_DEFINITIONS DES_CAVP=0)
endif()

if(TINY_DES_BUILD_TESTS)
  enable_testing()

  add_library(tiny-des-c-test OBJECT des.c)
  target_include_directories(tiny-des-c-test PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
  target_compile_definitions(tiny-des-c-test PUBLIC ${TINY_DES_TEST_DEFINITIONS})

  add_executable(test_des test.c test_edge_vectors.c munit.c cavp.c)
  target_link_libraries(test_des PRIVATE tiny-des-c-test)
  target_include_directories(test_des PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_compile_definitions(test_des PRIVATE
    ${TINY_DES_TEST_DEFINITIONS}
    CAVP_VECTOR_DIR="${CMAKE_CURRENT_LIST_DIR}/test_vectors/cavp"
  )
  add_test(NAME test_des COMMAND test_des)

  # C++ integration test (full modes so ECB/CBC/CMAC wrappers exist)
  add_executable(test_cpp test_cpp.cpp)
  target_link_libraries(test_cpp PRIVATE tiny-des-c-test)
  target_include_directories(test_cpp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_compile_definitions(test_cpp PRIVATE ${TINY_DES_TEST_DEFINITIONS})
  add_test(NAME test_cpp COMMAND test_cpp)
endif()
