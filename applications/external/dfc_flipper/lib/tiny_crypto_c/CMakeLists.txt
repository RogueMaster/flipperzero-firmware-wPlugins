# SPDX-FileCopyrightText: Mistial Dev
# SPDX-License-Identifier: GPL-2.0-or-later

cmake_minimum_required(VERSION 3.24)
project(tiny_crypto_c VERSION 1.0.1 LANGUAGES C)
include(GNUInstallDirs)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR AND
   NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

include(cmake/Profiles.cmake)

tc_profile_option(TINY_CRYPTO_ENABLE_AES TC_ENABLE_AES "Build AES")
tc_profile_option(TINY_CRYPTO_ENABLE_DES TC_ENABLE_DES "Build DES and 3DES")
tc_profile_option(TINY_CRYPTO_ENABLE_SHA1 TC_ENABLE_SHA1 "Build SHA-1")
tc_profile_option(TINY_CRYPTO_ENABLE_SHA224 TC_ENABLE_SHA224 "Build SHA-224")
tc_profile_option(TINY_CRYPTO_ENABLE_SHA256 TC_ENABLE_SHA256 "Build SHA-256")
tc_profile_option(TINY_CRYPTO_ENABLE_SHA384 TC_ENABLE_SHA384 "Build SHA-384")
tc_profile_option(TINY_CRYPTO_ENABLE_SHA512 TC_ENABLE_SHA512 "Build SHA-512")
tc_profile_option(TINY_CRYPTO_ENABLE_HMAC TC_ENABLE_HMAC "Build HMAC")
tc_profile_option(TINY_CRYPTO_ENABLE_KMAC256 TC_ENABLE_KMAC256 "Build fixed-output KMAC256")
tc_profile_option(TINY_CRYPTO_ENABLE_TLV TC_ENABLE_TLV "Build bounded TLV readers")
tc_profile_option(TINY_CRYPTO_ENABLE_DER TC_ENABLE_DER "Build DER value helpers")
tc_profile_option(TINY_CRYPTO_ENABLE_X509 TC_ENABLE_X509 "Build X.509 certificate parser")
tc_profile_option(TINY_CRYPTO_ENABLE_EAC_CVC TC_ENABLE_EAC_CVC "Build EAC card verifiable certificate parser")
if(TINY_CRYPTO_ENABLE_EAC_CVC AND NOT TINY_CRYPTO_ENABLE_DER)
  message(FATAL_ERROR "EAC CVC parsing requires TINY_CRYPTO_ENABLE_DER")
endif()
if(TINY_CRYPTO_ENABLE_X509 AND NOT TINY_CRYPTO_ENABLE_DER)
  message(FATAL_ERROR "X.509 parsing requires TINY_CRYPTO_ENABLE_DER")
endif()
tc_profile_option(TINY_CRYPTO_ENABLE_PIV_CVC TC_ENABLE_PIV_CVC "Build PIV secure messaging CVC parser")
tc_profile_option(TINY_CRYPTO_ENABLE_PIV_CHUID TC_ENABLE_PIV_CHUID "Build PIV CHUID parser")
tc_profile_option(TINY_CRYPTO_ENABLE_TWIC_CCL TC_ENABLE_TWIC_CCL "Build TWIC canceled card list reader")
tc_profile_option(TINY_CRYPTO_ENABLE_MD5 TC_ENABLE_MD5 "Build legacy MD5 checksums")
tc_profile_option(TINY_CRYPTO_ENABLE_GZIP TC_ENABLE_GZIP "Build bounded GZIP decompression")
if(TINY_CRYPTO_ENABLE_PIV_CHUID AND NOT TINY_CRYPTO_ENABLE_TLV)
  message(FATAL_ERROR "CHUID parsing requires TINY_CRYPTO_ENABLE_TLV")
endif()
if(TINY_CRYPTO_ENABLE_PIV_CVC AND NOT TINY_CRYPTO_ENABLE_DER)
  message(FATAL_ERROR "PIV CVC parsing requires TINY_CRYPTO_ENABLE_DER")
endif()
tc_profile_option(TINY_CRYPTO_TLV_BER TC_TLV_ENABLE_BER "Accept ASN.1 BER including indefinite lengths")
tc_profile_option(TINY_CRYPTO_TLV_STREAM TC_TLV_ENABLE_STREAM "Expose incremental TLV parsing")
if((TINY_CRYPTO_ENABLE_DER OR TINY_CRYPTO_TLV_BER OR TINY_CRYPTO_TLV_STREAM)
   AND NOT TINY_CRYPTO_ENABLE_TLV)
  message(FATAL_ERROR "DER, BER, and incremental parsing require TINY_CRYPTO_ENABLE_TLV")
endif()
tc_profile_option(TINY_CRYPTO_ENABLE_KDF TC_ENABLE_KDF "Build SP 800-108 KBKDF over the enabled HMAC/CMAC PRFs")
tc_profile_option(TINY_CRYPTO_ENABLE_SSKDF TC_ENABLE_SSKDF "Build the single-step hash KDF")
tc_profile_option(TINY_CRYPTO_ENABLE_EC TC_ENABLE_EC "Build elliptic-curve key agreement")
tc_profile_option(TINY_CRYPTO_ENABLE_RSA TC_ENABLE_RSA "Build RSA signature verification")
tc_profile_option(TINY_CRYPTO_RSA_SMALL TC_RSA_SMALL "Use byte limbs for RSA arithmetic")
tc_profile_option(TINY_CRYPTO_EC_P192 TC_EC_ENABLE_P192 "Enable P-192 for legacy protocols")
tc_profile_option(TINY_CRYPTO_EC_P256 TC_EC_ENABLE_P256 "Enable P-256 key agreement")
tc_profile_option(TINY_CRYPTO_EC_P384 TC_EC_ENABLE_P384 "Enable P-384 key agreement")
tc_profile_option(TINY_CRYPTO_EC_SMALL TC_EC_SMALL "Use byte limbs for elliptic-curve arithmetic")
if(TINY_CRYPTO_ENABLE_EC AND NOT TINY_CRYPTO_EC_P256 AND NOT TINY_CRYPTO_EC_P384)
  message(FATAL_ERROR "EC requires at least one curve")
endif()
if(TINY_CRYPTO_ENABLE_SSKDF AND NOT TINY_CRYPTO_ENABLE_SHA256 AND NOT TINY_CRYPTO_ENABLE_SHA384)
  message(FATAL_ERROR "Single-step KDF requires SHA-256 or SHA-384")
endif()
tc_profile_option(TINY_CRYPTO_ZEROIZE TC_ZEROIZE "Wipe secret state after use")
tc_profile_option(TINY_CRYPTO_STRICT TC_STRICT "Validate public API pointers")
tc_profile_option(TINY_CRYPTO_AVR_PROGMEM TC_AVR_PROGMEM "Keep constant tables in AVR flash")

set(TINY_CRYPTO_AES_KEY_BITS 128 CACHE STRING "AES key size: 128, 192, or 256")
set_property(CACHE TINY_CRYPTO_AES_KEY_BITS PROPERTY STRINGS 128 192 256)
tc_profile_option(TINY_CRYPTO_AES_DYNAMIC TC_AES_ENABLE_DYNAMIC "Build length-aware AES key, CBC, and CMAC APIs")
if(TINY_CRYPTO_AES_DYNAMIC AND NOT TINY_CRYPTO_ENABLE_AES)
  message(FATAL_ERROR "Dynamic AES requires TINY_CRYPTO_ENABLE_AES")
endif()
tc_profile_option(TINY_CRYPTO_AES_CBC TC_AES_ENABLE_CBC "Build AES-CBC")
tc_profile_option(TINY_CRYPTO_AES_ECB TC_AES_ENABLE_ECB "Build AES-ECB")
tc_profile_option(TINY_CRYPTO_AES_CTR TC_AES_ENABLE_CTR "Build AES-CTR")
tc_profile_option(TINY_CRYPTO_AES_OFB TC_AES_ENABLE_OFB "Build AES-OFB")
tc_profile_option(TINY_CRYPTO_AES_GCM TC_AES_ENABLE_GCM "Build AES-GCM")
tc_profile_option(TINY_CRYPTO_AES_CCM TC_AES_ENABLE_CCM "Build AES-CCM")
tc_profile_option(TINY_CRYPTO_AES_EAX TC_AES_ENABLE_EAX "Build AES-EAX")
tc_profile_option(TINY_CRYPTO_AES_EAX_PRIME TC_AES_ENABLE_EAX_PRIME "Build ANSI C12.22 EAX-prime")
tc_profile_option(TINY_CRYPTO_AES_SIV TC_AES_ENABLE_SIV "Build AES-SIV")
tc_profile_option(TINY_CRYPTO_AES_CMAC TC_AES_ENABLE_CMAC "Build AES-CMAC")
set(TINY_CRYPTO_AES_SBOX "constant-time" CACHE STRING
    "AES S-box: constant-time, runtime, or fast")
set_property(CACHE TINY_CRYPTO_AES_SBOX PROPERTY STRINGS constant-time runtime fast)
set(TINY_CRYPTO_AES_GHASH "profile" CACHE STRING
    "GCM multiply: profile, auto, bitwise, wide, fast-table, or hardware")
set_property(CACHE TINY_CRYPTO_AES_GHASH PROPERTY STRINGS
             profile auto bitwise wide fast-table hardware)
tc_profile_option(TINY_CRYPTO_AES_WIDE_OPS TC_AES_WIDE_OPS "Use native-width AES helpers")
tc_profile_option(TINY_CRYPTO_AES_TINY TC_AES_TINY "Reject large per-context GHASH tables")

tc_profile_option(TINY_CRYPTO_DES_ECB TC_DES_ENABLE_ECB "Build DES-ECB")
tc_profile_option(TINY_CRYPTO_DES_CBC TC_DES_ENABLE_CBC "Build DES-CBC")
tc_profile_option(TINY_CRYPTO_DES_CTR TC_DES_ENABLE_CTR "Build DES-CTR")
tc_profile_option(TINY_CRYPTO_DES_OFB TC_DES_ENABLE_OFB "Build DES-OFB")
tc_profile_option(TINY_CRYPTO_DES_CFB1 TC_DES_ENABLE_CFB1 "Build DES-CFB1")
tc_profile_option(TINY_CRYPTO_DES_CFB8 TC_DES_ENABLE_CFB8 "Build DES-CFB8")
tc_profile_option(TINY_CRYPTO_DES_CFB64 TC_DES_ENABLE_CFB64 "Build DES-CFB64")
tc_profile_option(TINY_CRYPTO_DES_TDES TC_DES_ENABLE_TDES "Build 3DES")
tc_profile_option(TINY_CRYPTO_DES_CMAC TC_DES_ENABLE_CMAC "Build TDEA-CMAC")
tc_profile_option(TINY_CRYPTO_DES_REJECT_WEAK_KEYS TC_DES_REJECT_WEAK_KEYS "Reject weak DES keys and degenerate TDEA bundles")

include(cmake/Features.cmake)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  option(TINY_CRYPTO_BUILD_TESTS "Build host tests" ON)
else()
  option(TINY_CRYPTO_BUILD_TESTS "Build host tests" OFF)
endif()
option(TINY_CRYPTO_TEST_FULL "Run checked-in CAVP corpora" OFF)
set(TINY_CRYPTO_SANITIZE "" CACHE STRING
    "Test sanitizers, for example address,undefined")

set(tc_any_sha OFF)
if(TINY_CRYPTO_ENABLE_SHA1 OR TINY_CRYPTO_ENABLE_SHA224 OR
   TINY_CRYPTO_ENABLE_SHA256 OR TINY_CRYPTO_ENABLE_SHA384 OR
   TINY_CRYPTO_ENABLE_SHA512)
  set(tc_any_sha ON)
endif()
set(tc_any_module OFF)
foreach(macro_name IN LISTS tc_module_features)
  set(option_name ${tc_module_option_${macro_name}})
  if(${option_name})
    set(tc_any_module ON)
  endif()
endforeach()
if(NOT TINY_CRYPTO_ENABLE_AES AND NOT TINY_CRYPTO_ENABLE_DES AND NOT tc_any_sha
   AND NOT TINY_CRYPTO_ENABLE_KMAC256 AND NOT TINY_CRYPTO_ENABLE_TLV
   AND NOT TINY_CRYPTO_ENABLE_EC AND NOT TINY_CRYPTO_ENABLE_RSA
   AND NOT TINY_CRYPTO_ENABLE_TWIC_CCL AND NOT TINY_CRYPTO_ENABLE_MD5
   AND NOT TINY_CRYPTO_ENABLE_GZIP AND NOT tc_any_module)
  message(FATAL_ERROR "Enable at least one algorithm or parser")
endif()
if(TINY_CRYPTO_ENABLE_HMAC AND NOT tc_any_sha)
  message(FATAL_ERROR "HMAC requires at least one SHA algorithm")
endif()
if(TINY_CRYPTO_ENABLE_DES AND NOT TINY_CRYPTO_DES_ECB AND
   NOT TINY_CRYPTO_DES_CBC AND NOT TINY_CRYPTO_DES_CTR AND
   NOT TINY_CRYPTO_DES_OFB AND NOT TINY_CRYPTO_DES_CFB1 AND
   NOT TINY_CRYPTO_DES_CFB8 AND NOT TINY_CRYPTO_DES_CFB64 AND
   NOT TINY_CRYPTO_DES_CMAC)
  message(FATAL_ERROR "DES requires at least one enabled mode or CMAC")
endif()
set(tc_kdf_prf OFF)
if(TINY_CRYPTO_ENABLE_HMAC AND tc_any_sha)
  set(tc_kdf_prf ON)
endif()
if(TINY_CRYPTO_ENABLE_AES AND TINY_CRYPTO_AES_CMAC)
  set(tc_kdf_prf ON)
endif()
if(TINY_CRYPTO_ENABLE_DES AND TINY_CRYPTO_DES_CMAC)
  set(tc_kdf_prf ON)
endif()
if(TINY_CRYPTO_ENABLE_KDF AND NOT tc_kdf_prf)
  message(FATAL_ERROR "KDF requires a PRF: HMAC with a SHA, AES-CMAC, or TDEA-CMAC")
endif()
if(NOT TINY_CRYPTO_AES_KEY_BITS MATCHES "^(128|192|256)$")
  message(FATAL_ERROR "TINY_CRYPTO_AES_KEY_BITS must be 128, 192, or 256")
endif()
tc_validate_module_features()

tc_profile_option(TINY_CRYPTO_ENABLE_PIV_SM TC_ENABLE_PIV_SM "Build PIV secure messaging for the client")
tc_profile_option(TINY_CRYPTO_PIV_SM_CS2 TC_PIV_SM_ENABLE_CS2 "Enable PIV SM cipher suite 2")
tc_profile_option(TINY_CRYPTO_PIV_SM_CS7 TC_PIV_SM_ENABLE_CS7 "Enable PIV SM cipher suite 7")
if(TINY_CRYPTO_ENABLE_PIV_SM)
  if(NOT TINY_CRYPTO_ENABLE_AES OR NOT TINY_CRYPTO_AES_DYNAMIC OR NOT TINY_CRYPTO_ENABLE_SHA256 OR
     NOT TINY_CRYPTO_ENABLE_SSKDF OR NOT TINY_CRYPTO_ENABLE_EC)
    message(FATAL_ERROR "PIV SM requires dynamic AES, SHA-256, single-step KDF, and EC")
  endif()
  if(NOT TINY_CRYPTO_PIV_SM_CS2 AND NOT TINY_CRYPTO_PIV_SM_CS7)
    message(FATAL_ERROR "PIV SM requires at least one cipher suite")
  endif()
  if(TINY_CRYPTO_PIV_SM_CS2 AND NOT TINY_CRYPTO_EC_P256)
    message(FATAL_ERROR "CS2 requires P-256")
  endif()
  if(TINY_CRYPTO_PIV_SM_CS7 AND (NOT TINY_CRYPTO_EC_P384 OR NOT TINY_CRYPTO_ENABLE_SHA384))
    message(FATAL_ERROR "CS7 requires P-384 and SHA-384")
  endif()
endif()

function(tc_bool option output)
  if(${option})
    set(${output} 1 PARENT_SCOPE)
  else()
    set(${output} 0 PARENT_SCOPE)
  endif()
endfunction()

function(tc_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4)
  elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endfunction()

foreach(pair
    "TINY_CRYPTO_ENABLE_AES;TC_ENABLE_AES"
    "TINY_CRYPTO_ENABLE_DES;TC_ENABLE_DES"
    "TINY_CRYPTO_ENABLE_SHA1;TC_ENABLE_SHA1"
    "TINY_CRYPTO_ENABLE_SHA224;TC_ENABLE_SHA224"
    "TINY_CRYPTO_ENABLE_SHA256;TC_ENABLE_SHA256"
    "TINY_CRYPTO_ENABLE_SHA384;TC_ENABLE_SHA384"
    "TINY_CRYPTO_ENABLE_SHA512;TC_ENABLE_SHA512"
    "TINY_CRYPTO_ENABLE_HMAC;TC_ENABLE_HMAC"
    "TINY_CRYPTO_ENABLE_KMAC256;TC_ENABLE_KMAC256"
    "TINY_CRYPTO_ENABLE_TLV;TC_ENABLE_TLV"
    "TINY_CRYPTO_ENABLE_DER;TC_ENABLE_DER"
    "TINY_CRYPTO_ENABLE_X509;TC_ENABLE_X509"
    "TINY_CRYPTO_ENABLE_EAC_CVC;TC_ENABLE_EAC_CVC"
    "TINY_CRYPTO_ENABLE_PIV_CVC;TC_ENABLE_PIV_CVC"
    "TINY_CRYPTO_ENABLE_PIV_CHUID;TC_ENABLE_PIV_CHUID"
    "TINY_CRYPTO_ENABLE_TWIC_CCL;TC_ENABLE_TWIC_CCL"
    "TINY_CRYPTO_ENABLE_MD5;TC_ENABLE_MD5"
    "TINY_CRYPTO_ENABLE_GZIP;TC_ENABLE_GZIP"
    "TINY_CRYPTO_TLV_BER;TC_TLV_ENABLE_BER"
    "TINY_CRYPTO_TLV_STREAM;TC_TLV_ENABLE_STREAM"
    "TINY_CRYPTO_ENABLE_KDF;TC_ENABLE_KDF"
    "TINY_CRYPTO_ENABLE_SSKDF;TC_ENABLE_SSKDF"
    "TINY_CRYPTO_ENABLE_PIV_SM;TC_ENABLE_PIV_SM"
    "TINY_CRYPTO_PIV_SM_CS2;TC_PIV_SM_ENABLE_CS2"
    "TINY_CRYPTO_PIV_SM_CS7;TC_PIV_SM_ENABLE_CS7"
    "TINY_CRYPTO_ENABLE_EC;TC_ENABLE_EC"
    "TINY_CRYPTO_ENABLE_RSA;TC_ENABLE_RSA"
    "TINY_CRYPTO_RSA_SMALL;TC_RSA_SMALL"
    "TINY_CRYPTO_EC_P256;TC_EC_ENABLE_P256"
    "TINY_CRYPTO_EC_P192;TC_EC_ENABLE_P192"
    "TINY_CRYPTO_EC_P384;TC_EC_ENABLE_P384"
    "TINY_CRYPTO_EC_SMALL;TC_EC_SMALL"
    "TINY_CRYPTO_ZEROIZE;TC_ZEROIZE"
    "TINY_CRYPTO_STRICT;TC_STRICT"
    "TINY_CRYPTO_AVR_PROGMEM;TC_AVR_PROGMEM"
    "TINY_CRYPTO_AES_CBC;TC_AES_ENABLE_CBC"
    "TINY_CRYPTO_AES_DYNAMIC;TC_AES_ENABLE_DYNAMIC"
    "TINY_CRYPTO_AES_ECB;TC_AES_ENABLE_ECB"
    "TINY_CRYPTO_AES_CTR;TC_AES_ENABLE_CTR"
    "TINY_CRYPTO_AES_OFB;TC_AES_ENABLE_OFB"
    "TINY_CRYPTO_AES_GCM;TC_AES_ENABLE_GCM"
    "TINY_CRYPTO_AES_CCM;TC_AES_ENABLE_CCM"
    "TINY_CRYPTO_AES_EAX;TC_AES_ENABLE_EAX"
    "TINY_CRYPTO_AES_EAX_PRIME;TC_AES_ENABLE_EAX_PRIME"
    "TINY_CRYPTO_AES_SIV;TC_AES_ENABLE_SIV"
    "TINY_CRYPTO_AES_CMAC;TC_AES_ENABLE_CMAC"
    "TINY_CRYPTO_AES_WIDE_OPS;TC_AES_WIDE_OPS"
    "TINY_CRYPTO_AES_TINY;TC_AES_TINY"
    "TINY_CRYPTO_DES_ECB;TC_DES_ENABLE_ECB"
    "TINY_CRYPTO_DES_CBC;TC_DES_ENABLE_CBC"
    "TINY_CRYPTO_DES_CTR;TC_DES_ENABLE_CTR"
    "TINY_CRYPTO_DES_OFB;TC_DES_ENABLE_OFB"
    "TINY_CRYPTO_DES_CFB1;TC_DES_ENABLE_CFB1"
    "TINY_CRYPTO_DES_CFB8;TC_DES_ENABLE_CFB8"
    "TINY_CRYPTO_DES_CFB64;TC_DES_ENABLE_CFB64"
    "TINY_CRYPTO_DES_TDES;TC_DES_ENABLE_TDES"
    "TINY_CRYPTO_DES_CMAC;TC_DES_ENABLE_CMAC"
    "TINY_CRYPTO_DES_REJECT_WEAK_KEYS;TC_DES_REJECT_WEAK_KEYS")
  list(GET pair 0 option_name)
  list(GET pair 1 macro_name)
  tc_bool(${option_name} macro_value)
  list(APPEND TC_PUBLIC_DEFINITIONS ${macro_name}=${macro_value})
endforeach()
foreach(macro_name IN LISTS tc_module_features)
  set(option_name ${tc_module_option_${macro_name}})
  tc_bool(${option_name} macro_value)
  list(APPEND TC_PUBLIC_DEFINITIONS ${macro_name}=${macro_value})
endforeach()

set(sbox_values constant-time runtime fast)
list(FIND sbox_values "${TINY_CRYPTO_AES_SBOX}" sbox_index)
if(sbox_index EQUAL -1)
  message(FATAL_ERROR "Unknown TINY_CRYPTO_AES_SBOX value")
endif()
math(EXPR sbox_value "${sbox_index} + 1")

set(ghash_values auto bitwise wide fast-table hardware)
if(TINY_CRYPTO_AES_GHASH STREQUAL "profile")
  set(ghash_value ${tc_profile_default_TC_AES_GCM_GHASH_MODE})
else()
  list(FIND ghash_values "${TINY_CRYPTO_AES_GHASH}" ghash_value)
endif()
if(ghash_value EQUAL -1)
  message(FATAL_ERROR "Unknown TINY_CRYPTO_AES_GHASH value")
endif()
if(TINY_CRYPTO_AES_TINY AND TINY_CRYPTO_AES_GHASH STREQUAL "fast-table")
  message(FATAL_ERROR "The tiny profile rejects the 256-byte GHASH table")
endif()

list(APPEND TC_PUBLIC_DEFINITIONS
  TC_AES_KEY_BITS=${TINY_CRYPTO_AES_KEY_BITS}
  TC_AES_SBOX_MODE=${sbox_value}
  TC_AES_GCM_GHASH_MODE=${ghash_value})

set(tc_aes_sources src/aes.c src/aes_gcm.c src/aes_ccm.c src/aes_mac.c)

# One utility translation unit keeps zeroization and constant-time comparison
# consistent across algorithms, and avoids duplicate code in firmware images.
set(tc_sources src/common.c)
if(TINY_CRYPTO_ENABLE_TLV)
  list(APPEND tc_sources src/tlv.c src/tlv_walk.c)
endif()
if(TINY_CRYPTO_ENABLE_DER)
  list(APPEND tc_sources src/der.c src/pki_key.c)
endif()
if(TINY_CRYPTO_ENABLE_X509)
  list(APPEND tc_sources src/x509.c src/x509_crypto.c src/x509_time.c
    src/x509_key.c src/x509_ext.c src/x509_name.c
    src/asn1_string.c src/unicode.c)
endif()
if(TINY_CRYPTO_ENABLE_EAC_CVC)
  list(APPEND tc_sources src/eac_cvc.c)
endif()
if(TINY_CRYPTO_ENABLE_PIV_CVC)
  list(APPEND tc_sources src/piv_cvc.c src/piv_cvc_verify.c)
endif()
if(TINY_CRYPTO_ENABLE_PIV_CHUID)
  list(APPEND tc_sources src/piv_chuid.c)
endif()
if(TINY_CRYPTO_ENABLE_TWIC_CCL)
  list(APPEND tc_sources src/twic_ccl.c)
endif()
if(TINY_CRYPTO_ENABLE_GZIP)
  list(APPEND tc_sources src/gzip_api.c src/gzip.c src/inflate.c
    src/inflate_bits.c src/inflate_tables.c src/inflate_tree.c)
endif()
if(TINY_CRYPTO_ENABLE_MD5)
  list(APPEND tc_sources src/md5.c)
endif()
if(TINY_CRYPTO_ENABLE_KMAC256)
  list(APPEND tc_sources src/kmac.c)
endif()
if(TINY_CRYPTO_ENABLE_AES)
  list(APPEND tc_sources ${tc_aes_sources})
endif()
if(TINY_CRYPTO_ENABLE_DES)
  list(APPEND tc_sources src/des.c)
endif()
if(TINY_CRYPTO_ENABLE_SHA1 OR TINY_CRYPTO_ENABLE_SHA224 OR TINY_CRYPTO_ENABLE_SHA256)
  list(APPEND tc_sources src/hash.c)
endif()
if(TINY_CRYPTO_ENABLE_SHA384 OR TINY_CRYPTO_ENABLE_SHA512)
  list(APPEND tc_sources src/sha512.c)
endif()
if(TINY_CRYPTO_ENABLE_KDF)
  list(APPEND tc_sources src/kdf.c)
endif()
if(TINY_CRYPTO_ENABLE_SSKDF)
  list(APPEND tc_sources src/sskdf.c)
endif()
if(TINY_CRYPTO_ENABLE_EC)
  list(APPEND tc_sources src/ec.c)
endif()
if(TINY_CRYPTO_ENABLE_RSA)
  list(APPEND tc_sources src/rsa.c)
endif()
if(TINY_CRYPTO_ENABLE_PIV_SM)
  list(APPEND tc_sources src/piv_sm.c src/piv_sm_message.c)
  if(TINY_CRYPTO_ENABLE_PIV_CVC AND TINY_CRYPTO_ENABLE_X509)
    list(APPEND tc_sources src/piv_sm_authenticate.c)
  endif()
endif()
tc_append_module_sources(tc_sources)

add_library(tiny_crypto_c STATIC ${tc_sources})
add_library(tiny_crypto_c::tiny_crypto_c ALIAS tiny_crypto_c)
target_include_directories(tiny_crypto_c PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_definitions(tiny_crypto_c PUBLIC ${TC_PUBLIC_DEFINITIONS})
tc_warnings(tiny_crypto_c)

include(cmake/Sanitizers.cmake)
tc_use_test_sanitizers(tiny_crypto_c)

find_program(TC_SIZE_TOOL NAMES llvm-size size)
if(TC_SIZE_TOOL)
  set(tc_size_command ${TC_SIZE_TOOL} $<TARGET_FILE:tiny_crypto_c>)
else()
  # Keep the frontend target portable even on toolchains without binutils.
  set(tc_size_command ${CMAKE_COMMAND} -E echo
      "No size utility found; inspect $<TARGET_FILE:tiny_crypto_c>")
endif()
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(tc_size_target size)
else()
  set(tc_size_target tiny_crypto_c-size)
endif()
add_custom_target(${tc_size_target}
  COMMAND ${tc_size_command}
  DEPENDS tiny_crypto_c
  COMMENT "Static-library section sizes for the selected profile")

include(cmake/Fixtures.cmake)
include(cmake/Tests.cmake)
include(cmake/Benchmarks.cmake)

if(ESP_PLATFORM)
  # ESP-IDF components link SDK targets that are not standalone CMake packages.
  return()
endif()

include(CMakePackageConfigHelpers)
install(TARGETS tiny_crypto_c EXPORT tiny_crypto_cTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY src/tiny_crypto DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES LICENSE LICENSES/Unicode-3.0.txt
  DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/tiny_crypto_c)

set(tc_cmake_dir ${CMAKE_INSTALL_LIBDIR}/cmake/tiny_crypto_c)
configure_package_config_file(cmake/tiny_crypto_cConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/tiny_crypto_cConfig.cmake
  INSTALL_DESTINATION ${tc_cmake_dir})
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/tiny_crypto_cConfigVersion.cmake
  VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/tiny_crypto_cConfig.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/tiny_crypto_cConfigVersion.cmake
        DESTINATION ${tc_cmake_dir})
install(EXPORT tiny_crypto_cTargets FILE tiny_crypto_cTargets.cmake
  NAMESPACE tiny_crypto_c:: DESTINATION ${tc_cmake_dir})
