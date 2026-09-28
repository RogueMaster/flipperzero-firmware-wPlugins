# SPDX-License-Identifier: GPL-2.0-or-later

# Full-API tests link a test library compiled once per ABI
# instead of recompiling the cores into every executable. The shipped
# tiny_crypto_c target stays on the configured firmware profile and is
# exercised by test_default_profile.
set(tc_full_definitions
  TC_ENABLE_AES=1 TC_ENABLE_DES=1 TC_ENABLE_SHA1=1 TC_ENABLE_SHA224=1
  TC_ENABLE_SHA256=1 TC_ENABLE_SHA384=1 TC_ENABLE_SHA512=1
  TC_ENABLE_HMAC=1 TC_ENABLE_KDF=1 TC_ENABLE_KMAC256=1 TC_ZEROIZE=1 TC_STRICT=1 TC_AVR_PROGMEM=1
  TC_AES_ENABLE_CBC=1 TC_AES_ENABLE_ECB=1 TC_AES_ENABLE_CTR=1
  TC_AES_ENABLE_OFB=1 TC_AES_ENABLE_GCM=1
  TC_AES_ENABLE_CCM=1 TC_AES_ENABLE_EAX=1
  TC_AES_ENABLE_SIV=1 TC_AES_ENABLE_CMAC=1 TC_AES_CMAC_MIN_TAG_LEN=4
  TC_AES_SBOX_MODE=1
  TC_AES_GCM_GHASH_MODE=0 TC_AES_WIDE_OPS=0 TC_AES_TINY=0
  TC_DES_ENABLE_ECB=1 TC_DES_ENABLE_CBC=1 TC_DES_ENABLE_CTR=1
  TC_DES_ENABLE_OFB=1 TC_DES_ENABLE_CFB1=1 TC_DES_ENABLE_CFB8=1
  TC_DES_ENABLE_CFB64=1 TC_DES_ENABLE_TDES=1 TC_DES_ENABLE_CMAC=1)
if(TINY_CRYPTO_TEST_FULL)
  list(APPEND tc_full_definitions TC_HASH_CAVP=1 TC_AES_CAVP=1 TC_DES_CAVP=1
       TC_KDF_CAVP=1)
else()
  list(APPEND tc_full_definitions TC_HASH_CAVP=0 TC_AES_CAVP=0 TC_DES_CAVP=0
       TC_KDF_CAVP=0)
endif()

# MSan needs an instrumented C++ standard library; skip the C++ suites.
if(TINY_CRYPTO_SANITIZE MATCHES "memory")
  set(tc_build_cpp_tests OFF)
else()
  set(tc_build_cpp_tests ON)
endif()

function(tc_add_test_library name)
  add_library(${name} STATIC ${ARGN})
  target_include_directories(${name} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
  tc_warnings(${name})
  if(tc_sanitize_flags)
    target_compile_options(${name} PRIVATE ${tc_sanitize_flags})
  endif()
endfunction()

function(tc_add_test_executable target)
  add_executable(${target} ${ARGN})
  if(MSVC)
    target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
  endif()
  tc_warnings(${target})
  tc_use_test_sanitizers(${target})
endfunction()

function(tc_add_linked_test target library)
  tc_add_test_executable(${target} ${ARGN})
  target_link_libraries(${target} PRIVATE ${library})
  add_test(NAME ${target} COMMAND ${target})
endfunction()

function(tc_add_c_test target library)
  tc_add_linked_test(${target} ${library} ${ARGN}
    tests/support/cavp.c tests/support/test_util.c tests/support/munit.c)
  target_include_directories(${target} PRIVATE tests/support)
endfunction()

set(tc_test_sources src/common.c ${tc_aes_sources} src/des.c src/hash.c src/sha512.c
    src/kdf.c src/kmac.c)
tc_add_test_library(tiny_crypto_c-test ${tc_test_sources})
target_compile_definitions(tiny_crypto_c-test PUBLIC
  ${tc_full_definitions} TC_AES_KEY_BITS=128 TC_AES_ENABLE_EAX_PRIME=1
  TC_DES_REJECT_WEAK_KEYS=0)

# AES-192/256 variants only need AES and its CMAC-backed KDF. Do not compile
# unrelated SHA, HMAC and DES cores once per AES key size.
foreach(key_bits 192 256)
  tc_add_test_library(tiny_crypto_c-test-aes${key_bits}
    src/common.c ${tc_aes_sources} src/kdf.c)
  target_compile_definitions(tiny_crypto_c-test-aes${key_bits} PUBLIC
    TC_ENABLE_AES=1 TC_ENABLE_DES=0 TC_ENABLE_SHA1=0 TC_ENABLE_SHA224=0
    TC_ENABLE_SHA256=0 TC_ENABLE_SHA384=0 TC_ENABLE_SHA512=0
    TC_ENABLE_HMAC=0 TC_ENABLE_KDF=1 TC_ZEROIZE=1 TC_STRICT=1
    TC_AVR_PROGMEM=1 TC_AES_KEY_BITS=${key_bits}
    TC_AES_ENABLE_CBC=1 TC_AES_ENABLE_ECB=1 TC_AES_ENABLE_CTR=1
    TC_AES_ENABLE_OFB=1 TC_AES_ENABLE_GCM=1 TC_AES_ENABLE_CCM=1
    TC_AES_ENABLE_EAX=1 TC_AES_ENABLE_EAX_PRIME=0 TC_AES_ENABLE_SIV=1
    TC_AES_ENABLE_CMAC=1 TC_AES_CMAC_MIN_TAG_LEN=4
    TC_AES_SBOX_MODE=1 TC_AES_GCM_GHASH_MODE=0 TC_AES_WIDE_OPS=0
    TC_AES_TINY=0 TC_AES_CAVP=$<BOOL:${TINY_CRYPTO_TEST_FULL}>
    TC_KDF_CAVP=$<BOOL:${TINY_CRYPTO_TEST_FULL}>)
endforeach()

tc_add_test_library(tiny_crypto_c-test-des-reject-weak
  src/common.c src/des.c)
target_compile_definitions(tiny_crypto_c-test-des-reject-weak PUBLIC
  TC_ENABLE_AES=0 TC_ENABLE_DES=1 TC_ENABLE_SHA1=0 TC_ENABLE_SHA224=0
  TC_ENABLE_SHA256=0 TC_ENABLE_SHA384=0 TC_ENABLE_SHA512=0
  TC_ENABLE_HMAC=0 TC_ENABLE_KDF=0 TC_ZEROIZE=1 TC_STRICT=1
  TC_AVR_PROGMEM=1 TC_DES_ENABLE_ECB=1 TC_DES_ENABLE_CBC=0
  TC_DES_ENABLE_CTR=0 TC_DES_ENABLE_OFB=0 TC_DES_ENABLE_CFB1=0
  TC_DES_ENABLE_CFB8=0 TC_DES_ENABLE_CFB64=0 TC_DES_ENABLE_TDES=1
  TC_DES_ENABLE_CMAC=0 TC_DES_REJECT_WEAK_KEYS=1)

tc_add_test_library(tiny_crypto_c-test-aes-runtime-sbox
  src/common.c ${tc_aes_sources})
target_compile_definitions(tiny_crypto_c-test-aes-runtime-sbox PUBLIC
  TC_AES_KEY_BITS=128 TC_AES_SBOX_MODE=2 TC_AES_ENABLE_CMAC=1)

# Compile-only profile matrix for legal feature-gate combinations.
function(tc_add_compile_profile name)
  tc_add_test_library(${name} src/common.c ${ARGN})
  if(MSVC)
    target_compile_options(${name} PRIVATE /WX)
  else()
    target_compile_options(${name} PRIVATE -Werror)
  endif()
endfunction()

tc_add_compile_profile(tiny_crypto_c-profile-des-ecb src/des.c)
target_compile_definitions(tiny_crypto_c-profile-des-ecb PRIVATE
  TC_ENABLE_AES=0 TC_ENABLE_DES=1 TC_ENABLE_SHA256=0 TC_ZEROIZE=0 TC_STRICT=0
  TC_DES_ENABLE_ECB=1 TC_DES_ENABLE_CBC=0 TC_DES_ENABLE_CTR=0
  TC_DES_ENABLE_OFB=0 TC_DES_ENABLE_CFB1=0 TC_DES_ENABLE_CFB8=0
  TC_DES_ENABLE_CFB64=0 TC_DES_ENABLE_TDES=0 TC_DES_ENABLE_CMAC=0)

tc_add_compile_profile(tiny_crypto_c-profile-des-cmac src/des.c)
target_compile_definitions(tiny_crypto_c-profile-des-cmac PRIVATE
  TC_ENABLE_AES=0 TC_ENABLE_DES=1 TC_ENABLE_SHA256=0
  TC_DES_ENABLE_ECB=0 TC_DES_ENABLE_CBC=0 TC_DES_ENABLE_CTR=0
  TC_DES_ENABLE_OFB=0 TC_DES_ENABLE_CFB1=0 TC_DES_ENABLE_CFB8=0
  TC_DES_ENABLE_CFB64=0 TC_DES_ENABLE_TDES=1 TC_DES_ENABLE_CMAC=1)

tc_add_compile_profile(tiny_crypto_c-profile-des-all-no-cmac src/des.c)
target_compile_definitions(tiny_crypto_c-profile-des-all-no-cmac PRIVATE
  TC_ENABLE_AES=0 TC_ENABLE_DES=1 TC_ENABLE_SHA256=0 TC_ZEROIZE=1 TC_STRICT=1
  TC_DES_ENABLE_ECB=1 TC_DES_ENABLE_CBC=1 TC_DES_ENABLE_CTR=1
  TC_DES_ENABLE_OFB=1 TC_DES_ENABLE_CFB1=1 TC_DES_ENABLE_CFB8=1
  TC_DES_ENABLE_CFB64=1 TC_DES_ENABLE_TDES=1 TC_DES_ENABLE_CMAC=0)

tc_add_compile_profile(tiny_crypto_c-profile-aes-siv ${tc_aes_sources})
target_compile_definitions(tiny_crypto_c-profile-aes-siv PRIVATE
  TC_ENABLE_AES=1 TC_ENABLE_DES=0 TC_ENABLE_SHA256=0
  TC_AES_ENABLE_CBC=0 TC_AES_ENABLE_ECB=0 TC_AES_ENABLE_CTR=0
  TC_AES_ENABLE_OFB=0 TC_AES_ENABLE_GCM=0 TC_AES_ENABLE_CCM=0
  TC_AES_ENABLE_EAX=0 TC_AES_ENABLE_EAX_PRIME=0 TC_AES_ENABLE_SIV=1
  TC_AES_ENABLE_CMAC=0)

tc_add_compile_profile(tiny_crypto_c-profile-aes-cmac-minimal ${tc_aes_sources})
target_compile_definitions(tiny_crypto_c-profile-aes-cmac-minimal PRIVATE
  TC_ENABLE_AES=1 TC_ENABLE_DES=0 TC_ENABLE_SHA256=0 TC_ZEROIZE=0 TC_STRICT=0
  TC_AES_ENABLE_CBC=0 TC_AES_ENABLE_ECB=0 TC_AES_ENABLE_CTR=0
  TC_AES_ENABLE_OFB=0 TC_AES_ENABLE_GCM=0 TC_AES_ENABLE_CCM=0
  TC_AES_ENABLE_EAX=0 TC_AES_ENABLE_EAX_PRIME=0 TC_AES_ENABLE_SIV=0
  TC_AES_ENABLE_CMAC=1)

foreach(ghash_mode 1 2 3 4)
  tc_add_test_library(tiny_crypto_c-test-gcm-${ghash_mode}
    src/common.c ${tc_aes_sources})
  target_compile_definitions(tiny_crypto_c-test-gcm-${ghash_mode} PUBLIC
    TC_AES_KEY_BITS=128 TC_AES_ENABLE_GCM=1
    TC_AES_GCM_GHASH_MODE=${ghash_mode})
endforeach()
