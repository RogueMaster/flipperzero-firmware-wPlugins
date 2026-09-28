# SPDX-License-Identifier: GPL-2.0-or-later

if(TINY_CRYPTO_BUILD_TESTS)
  enable_language(CXX)
  set(CMAKE_CXX_STANDARD 11)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
  enable_testing()
  # Direct-source consumers compile disabled translation units too.
  file(GLOB tc_direct_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")
  foreach(feature AES TLV EC RSA)
    add_library(test_direct_${feature} OBJECT ${tc_direct_sources})
    target_include_directories(test_direct_${feature} PUBLIC src)
    target_compile_definitions(test_direct_${feature} PUBLIC
      TC_ENABLE_SHA256=0 TC_ENABLE_AES=$<STREQUAL:${feature},AES>
      TC_ENABLE_TLV=$<STREQUAL:${feature},TLV> TC_ENABLE_EC=$<STREQUAL:${feature},EC>
      TC_ENABLE_RSA=$<STREQUAL:${feature},RSA>)
    tc_warnings(test_direct_${feature})
    tc_use_test_sanitizers(test_direct_${feature})
  endforeach()
  add_test(NAME test_piv_targets COMMAND ${CMAKE_COMMAND}
    -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR} -DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/piv-targets
    -DC_COMPILER=${CMAKE_C_COMPILER} -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/piv_targets.cmake)
  add_test(NAME test_resource_profiles COMMAND ${CMAKE_COMMAND}
    -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR} -DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/resource-profiles
    -DC_COMPILER=${CMAKE_C_COMPILER} -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/resource_profiles.cmake)
  find_package(Python3 COMPONENTS Interpreter QUIET)
  if(Python3_Interpreter_FOUND)
    add_test(NAME test_benchmark_report
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_benchmark_report.py)
    add_test(NAME test_unicode_tables
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_unicode_tables.py)
    add_test(NAME test_package_boundaries
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_package_boundaries.py)
  endif()

  add_test(NAME test_installed_consumer
    COMMAND ${CMAKE_COMMAND}
      -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
      -DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/installed-consumer
      -DC_COMPILER=${CMAKE_C_COMPILER}
      -DCXX_COMPILER=${CMAKE_CXX_COMPILER}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/installed_consumer.cmake)
  # Recursive make exports Debug during sanitizer runs. The package test must
  # still configure, build, and install the same configuration.
  add_test(NAME test_installed_consumer_debug_environment
    COMMAND ${CMAKE_COMMAND} -E env CMAKE_BUILD_TYPE=Debug
      ${CMAKE_COMMAND}
      -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
      -DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/installed-consumer-debug-environment
      -DC_COMPILER=${CMAKE_C_COMPILER}
      -DCXX_COMPILER=${CMAKE_CXX_COMPILER}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/installed_consumer.cmake)
  foreach(sbox runtime fast)
    if(sbox STREQUAL "runtime")
      set(value 2)
    else()
      set(value 3)
    endif()
    add_test(NAME test_benchmark_${sbox}
      COMMAND ${CMAKE_COMMAND}
        -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/benchmark-${sbox}
        -DC_COMPILER=${CMAKE_C_COMPILER} -DSBOX=${sbox} -DSBOX_VALUE=${value}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/benchmark_profile.cmake)
  endforeach()

  add_test(NAME test_reject_des_without_consumer
    COMMAND ${CMAKE_COMMAND}
      -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
      -DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/invalid-des-profile
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/reject_des_without_consumer.cmake)

  # µunit uses C11 atomics when Clang exposes them in C99 mode. Keep the
  # vendored source unchanged and suppress that extension warning locally.
  if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    set_source_files_properties(tests/support/munit.c PROPERTIES
      COMPILE_OPTIONS -Wno-c11-extensions)
  endif()

  include(${CMAKE_CURRENT_LIST_DIR}/TestProfiles.cmake)
  tc_add_c_test(test_hash_dispatch tiny_crypto_c-test tests/hash/dispatch.c)
  tc_add_c_test(test_rsa_mgf tiny_crypto_c-test tests/rsa/mgf.c)
  tc_add_c_test(test_rsa_pss tiny_crypto_c-test tests/rsa/pss.c)
  tc_add_c_test(test_rsa_oaep tiny_crypto_c-test tests/rsa/oaep.c)
  tc_add_c_test(test_rsa_oaep_arguments tiny_crypto_c-test tests/rsa/oaep_arguments.c src/rsa.c)
  target_compile_definitions(test_rsa_oaep_arguments PRIVATE TC_ENABLE_RSA=1)
  foreach(feature AES TLV EC)
    tc_add_c_test(test_direct_${feature}_consumer test_direct_${feature} tests/default_profile.c)
  endforeach()
  foreach(sm_profile dual cs2 cs7 micro mini)
    if(sm_profile STREQUAL "dual")
      set(sm_suffix "")
    else()
      set(sm_suffix "-${sm_profile}")
    endif()
  tc_add_test_library(tiny_crypto_c-test-piv-sm${sm_suffix} src/common.c ${tc_aes_sources}
    src/hash.c src/sha512.c src/sskdf.c src/ec.c src/tlv.c src/der.c src/piv_cvc.c
    src/piv_sm.c src/piv_sm_message.c examples/piv_sm_wire.c)
  target_compile_definitions(tiny_crypto_c-test-piv-sm${sm_suffix} PUBLIC
    TC_RESOURCE_PROFILE=$<IF:$<STREQUAL:${sm_profile},micro>,1,$<IF:$<STREQUAL:${sm_profile},mini>,2,0>>
    TC_ENABLE_PIV_SM=1 TC_AES_ENABLE_DYNAMIC=1 TC_ENABLE_EC=1 TC_ENABLE_SSKDF=1
    TC_ENABLE_SHA384=$<NOT:$<STREQUAL:${sm_profile},cs2>>
    TC_PIV_SM_ENABLE_CS2=$<NOT:$<STREQUAL:${sm_profile},cs7>>
    TC_PIV_SM_ENABLE_CS7=$<NOT:$<STREQUAL:${sm_profile},cs2>>
    TC_EC_ENABLE_P256=$<NOT:$<STREQUAL:${sm_profile},cs7>>
    TC_EC_ENABLE_P384=$<NOT:$<STREQUAL:${sm_profile},cs2>>
    TC_ENABLE_TLV=1 TC_ENABLE_DER=1 TC_ENABLE_PIV_CVC=1)
  tc_add_c_test(test_piv_sm${sm_suffix} tiny_crypto_c-test-piv-sm${sm_suffix} tests/piv/sm.c)
    if(Python3_Interpreter_FOUND)
      set(sm_fixture_args)
      if(NOT sm_profile STREQUAL "cs7")
        list(APPEND sm_fixture_args --bits 256)
      endif()
      if(NOT sm_profile STREQUAL "cs2")
        list(APPEND sm_fixture_args --bits 384)
      endif()
      add_test(NAME test_piv_sm_synthetic${sm_suffix}
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/piv/synthetic_sm.py
          --reader $<TARGET_FILE:test_piv_sm${sm_suffix}> ${sm_fixture_args})
    endif()
  endforeach()
  foreach(small 0 1)
    tc_add_test_library(tiny_crypto_c-test-ec-${small} src/common.c src/ec.c src/rsa.c src/tlv.c src/tlv_walk.c src/der.c src/x509_key.c src/pki_key.c)
    target_compile_definitions(tiny_crypto_c-test-ec-${small} PUBLIC
      TC_ENABLE_EC=1 TC_EC_ENABLE_P192=1 TC_EC_SMALL=${small} TC_ENABLE_AES=0 TC_ENABLE_SHA256=0
      TC_ENABLE_RSA=1 TC_RSA_SMALL=${small}
      TC_ENABLE_TLV=1 TC_ENABLE_DER=1 TC_ENABLE_X509=1)
    tc_add_c_test(test_ec_${small} tiny_crypto_c-test-ec-${small} tests/ec/test.c)
    tc_add_c_test(test_ecdsa_reader_${small} tiny_crypto_c-test-ec-${small} tests/ec/signature_reader.c)
    tc_add_c_test(test_arithmetic_${small} tiny_crypto_c-test-ec-${small} tests/ec/arithmetic.c)
  endforeach()
  foreach(small 0 1)
    tc_add_test_library(tiny_crypto_c-test-rsa-${small} src/common.c src/rsa.c)
    target_compile_definitions(tiny_crypto_c-test-rsa-${small} PUBLIC
      TC_ENABLE_RSA=1 TC_RSA_SMALL=${small} TC_ENABLE_AES=0 TC_ENABLE_SHA256=0)
    tc_add_c_test(test_rsa_public_${small} tiny_crypto_c-test-rsa-${small} tests/rsa/public.c)
    tc_add_c_test(test_rsa_validation_${small} tiny_crypto_c-test-rsa-${small} tests/rsa/validation.c)
    tc_add_c_test(test_rsa_inverse_${small} tiny_crypto_c-test-rsa-${small} tests/rsa/inverse.c)
    tc_add_c_test(test_rsa_prime_${small} tiny_crypto_c-test-rsa-${small} tests/rsa/prime.c)
    tc_add_c_test(test_hash_dispatch_disabled_${small} tiny_crypto_c-test-rsa-${small} tests/hash/dispatch.c)
  endforeach()
  tc_add_c_test(test_rsa_oaep_disabled tiny_crypto_c-test-rsa-0 tests/rsa/oaep_arguments.c)
  foreach(small 0 1)
    tc_add_c_test(test_rsa_oaep_reader_${small} tiny_crypto_c-test tests/rsa/oaep_reader.c src/rsa.c)
    target_compile_definitions(test_rsa_oaep_reader_${small} PRIVATE TC_ENABLE_RSA=1 TC_RSA_SMALL=${small})
  endforeach()
  foreach(curve 256 384)
    tc_add_test_library(tiny_crypto_c-test-ec-p${curve} src/common.c src/ec.c)
    target_compile_definitions(tiny_crypto_c-test-ec-p${curve} PUBLIC
      TC_ENABLE_EC=1 TC_ENABLE_AES=0 TC_ENABLE_SHA256=0
      TC_EC_ENABLE_P256=$<STREQUAL:${curve},256> TC_EC_ENABLE_P384=$<STREQUAL:${curve},384>)
    tc_add_c_test(test_ec_p${curve} tiny_crypto_c-test-ec-p${curve} tests/ec/test.c)
  endforeach()
  tc_add_test_library(tiny_crypto_c-test-sskdf src/common.c src/hash.c src/sha512.c src/sskdf.c)
  option(TINY_CRYPTO_TEST_EC_ORACLE "Compare EC with Python cryptography" OFF)
  set(TINY_CRYPTO_TEST_WYCHEPROOF_ARCHIVE "" CACHE FILEPATH "Pinned C2SP Wycheproof archive")
  if(Python3_Interpreter_FOUND)
    add_test(NAME test_wycheproof_runner
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/wycheproof_test.py)
  endif()
  if(TINY_CRYPTO_TEST_WYCHEPROOF_ARCHIVE)
    if(NOT Python3_Interpreter_FOUND)
      message(FATAL_ERROR "Wycheproof archive tests require Python 3")
    endif()
    add_test(NAME test_wycheproof_ec
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/wycheproof.py
        --archive ${TINY_CRYPTO_TEST_WYCHEPROOF_ARCHIVE}
        --ec-reader $<TARGET_FILE:test_ec_0> --ec-reader $<TARGET_FILE:test_ec_1>)
    add_test(NAME test_wycheproof_rsa_signatures
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/wycheproof.py
        --archive ${TINY_CRYPTO_TEST_WYCHEPROOF_ARCHIVE}
        --rsa-signature-reader $<TARGET_FILE:test_rsa_signature_reader>
        --rsa-signature-reader $<TARGET_FILE:test_rsa_signature_reader_small>)
    add_test(NAME test_wycheproof_rsa_oaep
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/wycheproof.py
        --archive ${TINY_CRYPTO_TEST_WYCHEPROOF_ARCHIVE}
        --rsa-oaep-reader $<TARGET_FILE:test_rsa_oaep_reader_0>
        --rsa-oaep-reader $<TARGET_FILE:test_rsa_oaep_reader_1>)
    add_test(NAME test_wycheproof_ecdsa
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/wycheproof.py
        --archive ${TINY_CRYPTO_TEST_WYCHEPROOF_ARCHIVE}
        --ecdsa-reader $<TARGET_FILE:test_ecdsa_reader_0>
        --ecdsa-reader $<TARGET_FILE:test_ecdsa_reader_1>)
  endif()
  set(TINY_CRYPTO_TEST_EC_CAVP_ARCHIVE "" CACHE FILEPATH "NIST ECCCDH component test archive")
  if(TINY_CRYPTO_TEST_EC_CAVP_ARCHIVE)
    if(NOT Python3_Interpreter_FOUND)
      message(FATAL_ERROR "ECCCDH corpus tests require Python 3")
    endif()
    add_test(NAME test_ec_cavp
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/ec/oracle.py
        --reader $<TARGET_FILE:test_ec_0> --reader $<TARGET_FILE:test_ec_1>
        --cavp-archive ${TINY_CRYPTO_TEST_EC_CAVP_ARCHIVE})
  endif()
  if(TINY_CRYPTO_TEST_EC_ORACLE)
    if(NOT Python3_Interpreter_FOUND)
      message(FATAL_ERROR "EC oracle tests require Python 3 and cryptography")
    endif()
    add_test(NAME test_ec_oracle
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/ec/oracle.py
        --reader $<TARGET_FILE:test_ec_0> --reader $<TARGET_FILE:test_ec_1>)
  endif()
  target_compile_definitions(tiny_crypto_c-test-sskdf PUBLIC
    TC_ENABLE_AES=0 TC_ENABLE_SSKDF=1 TC_ENABLE_SHA256=1 TC_ENABLE_SHA384=1)
  tc_add_c_test(test_sskdf tiny_crypto_c-test-sskdf tests/kdf/sskdf_test.c)
  set(TINY_CRYPTO_TEST_SM_CAPTURE_DIR "" CACHE PATH "PIV secure messaging capture directory")
  if(Python3_Interpreter_FOUND)
    add_test(NAME test_sm_capture_adapter
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/piv/sm_corpus_test.py)
  endif()
  if(TINY_CRYPTO_TEST_SM_CAPTURE_DIR)
    if(NOT Python3_Interpreter_FOUND)
      message(FATAL_ERROR "PIV capture tests require Python 3")
    endif()
    add_test(NAME test_sm_primitives_corpus
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/piv/sm_corpus.py
        --reader $<TARGET_FILE:test_sskdf> --corpus ${TINY_CRYPTO_TEST_SM_CAPTURE_DIR}
        --ec-reader $<TARGET_FILE:test_ec_0> --ec-reader $<TARGET_FILE:test_ec_1>
        --sm-reader $<TARGET_FILE:test_piv_sm>)
  endif()
  tc_add_test_library(tiny_crypto_c-test-aes-dynamic src/common.c ${tc_aes_sources})
  target_compile_definitions(tiny_crypto_c-test-aes-dynamic PUBLIC
    TC_AES_ENABLE_DYNAMIC=1 TC_AES_ENABLE_CTR=0 TC_ENABLE_SHA256=0)
  tc_add_c_test(test_aes_dynamic tiny_crypto_c-test-aes-dynamic tests/aes/dynamic_test.c)
  tc_add_c_test(test_idf_bootloader_hash tiny_crypto_c-test
    tests/esp_idf/bootloader_hash.c ports/esp-idf/bootloader_hash.c)
  foreach(scheme rsa ecdsa)
    foreach(mode efuse single)
      set(policy_target test_idf_signature_policy_${scheme}_${mode})
      tc_add_c_test(${policy_target} tiny_crypto_c-test
        tests/esp_idf/policy.c ports/esp-idf/bootloader_hash.c
        ports/esp-idf/vendor/bootloader_support/src/secure_boot_v2/secure_boot_signatures_app.c)
      target_include_directories(${policy_target} PRIVATE tests/esp_idf/policy_include
        tests/esp_idf/include ports/esp-idf/vendor/bootloader_support/private_include
        ports/esp-idf/vendor/bootloader_support/src/secure_boot_v2)
      set_property(TARGET ${policy_target} PROPERTY C_STANDARD 11)
      if(scheme STREQUAL "ecdsa")
        target_compile_definitions(${policy_target} PRIVATE TC_TEST_IDF_ECDSA=1)
      endif()
      if(mode STREQUAL "single")
        target_compile_definitions(${policy_target} PRIVATE CONFIG_SECURE_SIGNED_ON_UPDATE_NO_SECURE_BOOT=1)
      endif()
    endforeach()
  endforeach()
  target_include_directories(test_idf_bootloader_hash PRIVATE
    tests/esp_idf/include ports/esp-idf/vendor/bootloader_support/private_include)
  tc_add_c_test(test_idf_signed_rsa tiny_crypto_c-test
    tests/esp_idf/signed_image.c ports/esp-idf/signed_update_rsa.c src/rsa.c)
  tc_add_c_test(test_rsa_signature_reader tiny_crypto_c-test tests/rsa/signature_reader.c src/rsa.c)
  target_compile_definitions(test_rsa_signature_reader PRIVATE TC_ENABLE_RSA=1)
  tc_add_c_test(test_rsa_signature_reader_small tiny_crypto_c-test tests/rsa/signature_reader.c src/rsa.c)
  target_compile_definitions(test_rsa_signature_reader_small PRIVATE TC_ENABLE_RSA=1 TC_RSA_SMALL=1)
  target_compile_definitions(test_idf_signed_rsa PRIVATE TC_ENABLE_RSA=1)
  target_include_directories(test_idf_signed_rsa PRIVATE tests/esp_idf/include
    ports/esp-idf/vendor/bootloader_support/src/secure_boot_v2)
  tc_add_test_library(tiny_crypto_c-test-idf-ec src/common.c src/hash.c src/ec.c)
  target_compile_definitions(tiny_crypto_c-test-idf-ec PUBLIC
    TC_ENABLE_EC=1 TC_EC_ENABLE_P192=1 TC_ENABLE_SHA256=1 TC_ENABLE_AES=0)
  tc_add_c_test(test_idf_ecdsa_image tiny_crypto_c-test-idf-ec
    tests/esp_idf/signed_image.c ports/esp-idf/signed_update_ecdsa.c)
  target_compile_definitions(test_idf_ecdsa_image PRIVATE TC_TEST_IDF_ECDSA=1 ESP_SECURE_BOOT_DIGEST_LEN=32)
  target_include_directories(test_idf_ecdsa_image PRIVATE tests/esp_idf/include
    ports/esp-idf/vendor/bootloader_support/src/secure_boot_v2)
  set(TINY_CRYPTO_TEST_ESP_SIGNED_IMAGE "" CACHE FILEPATH "Espressif RSA-3072 signed application fixture")
  foreach(scheme rsa ecdsa)
    set(policy_target test_idf_image_policy_${scheme})
    tc_add_c_test(${policy_target} tiny_crypto_c-test
      tests/esp_idf/policy.c ports/esp-idf/bootloader_hash.c
      ports/esp-idf/signed_update_${scheme}.c
      ports/esp-idf/vendor/bootloader_support/src/secure_boot_v2/secure_boot_signatures_app.c)
    target_compile_definitions(${policy_target} PRIVATE TC_TEST_IDF_REAL_CRYPTO=1)
    if(scheme STREQUAL "ecdsa")
      target_sources(${policy_target} PRIVATE src/ec.c)
      target_compile_definitions(${policy_target} PRIVATE TC_TEST_IDF_ECDSA=1 TC_ENABLE_EC=1 TC_EC_ENABLE_P192=1)
    else()
      target_sources(${policy_target} PRIVATE src/rsa.c)
      target_compile_definitions(${policy_target} PRIVATE TC_ENABLE_RSA=1)
    endif()
    target_include_directories(${policy_target} PRIVATE tests/esp_idf/policy_include
      tests/esp_idf/include ports/esp-idf/vendor/bootloader_support/private_include
      ports/esp-idf/vendor/bootloader_support/src/secure_boot_v2)
    set_property(TARGET ${policy_target} PROPERTY C_STANDARD 11)
  endforeach()
  foreach(curve 192 256)
    set(TINY_CRYPTO_TEST_ESP_ECDSA${curve}_IMAGE "" CACHE FILEPATH "Espressif P-${curve} signed application fixture")
    if(TINY_CRYPTO_TEST_ESP_ECDSA${curve}_IMAGE)
      if(NOT EXISTS "${TINY_CRYPTO_TEST_ESP_ECDSA${curve}_IMAGE}")
        message(FATAL_ERROR "Signed ECDSA application fixture does not exist: ${TINY_CRYPTO_TEST_ESP_ECDSA${curve}_IMAGE}")
      endif()
      add_test(NAME test_idf_ecdsa${curve}_image COMMAND test_idf_ecdsa_image
        --image "${TINY_CRYPTO_TEST_ESP_ECDSA${curve}_IMAGE}")
      add_test(NAME test_idf_ecdsa${curve}_image_policy COMMAND test_idf_image_policy_ecdsa
        --image "${TINY_CRYPTO_TEST_ESP_ECDSA${curve}_IMAGE}")
    endif()
  endforeach()
  if(TINY_CRYPTO_TEST_ESP_SIGNED_IMAGE)
    if(NOT EXISTS "${TINY_CRYPTO_TEST_ESP_SIGNED_IMAGE}")
      message(FATAL_ERROR "Signed application fixture does not exist: ${TINY_CRYPTO_TEST_ESP_SIGNED_IMAGE}")
    endif()
    add_test(NAME test_idf_signed_rsa_image COMMAND test_idf_signed_rsa
      --image "${TINY_CRYPTO_TEST_ESP_SIGNED_IMAGE}")
    add_test(NAME test_idf_rsa_image_policy COMMAND test_idf_image_policy_rsa
      --image "${TINY_CRYPTO_TEST_ESP_SIGNED_IMAGE}")
  endif()
  tc_add_c_test(test_aes_platform tiny_crypto_c-test-aes-dynamic tests/aes/platform.c src/aes.c)
  target_compile_definitions(test_aes_platform PRIVATE TC_AES_PLATFORM=1)
  tc_add_c_test(test_aes_backend_failure tiny_crypto_c-test-aes-dynamic
    tests/aes/backend_failure.c src/aes_mac.c src/aes_ccm.c src/aes_gcm.c)
  target_compile_definitions(test_aes_backend_failure PRIVATE
    TC_AES_ENABLE_CMAC=1 TC_AES_ENABLE_SIV=1 TC_AES_ENABLE_EAX=1 TC_AES_ENABLE_EAX_PRIME=1
    TC_AES_ENABLE_CCM=1 TC_AES_ENABLE_GCM=1
    tc_aes_cipher_rounds=tc_test_cipher_rounds tc_aes_cipher=tc_test_cipher)
  foreach(bits 128 192 256)
    if(bits EQUAL 128)
      set(aead_library tiny_crypto_c-test)
    else()
      set(aead_library tiny_crypto_c-test-aes${bits})
    endif()
    tc_add_c_test(test_wycheproof_aead_${bits} ${aead_library} tests/aes/wycheproof.c)
  endforeach()
  # Archive runners supply fixtures to these executables.
  set_tests_properties(
    test_ecdsa_reader_0 test_ecdsa_reader_1
    test_rsa_signature_reader test_rsa_signature_reader_small
    test_rsa_oaep_reader_0 test_rsa_oaep_reader_1
    test_wycheproof_aead_128 test_wycheproof_aead_192 test_wycheproof_aead_256
    PROPERTIES SKIP_REGULAR_EXPRESSION "No tests run, 1 .* skipped")
  if(TINY_CRYPTO_TEST_WYCHEPROOF_ARCHIVE)
    add_test(NAME test_wycheproof_aead
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/wycheproof.py
        --archive ${TINY_CRYPTO_TEST_WYCHEPROOF_ARCHIVE}
        --aead-reader 128:$<TARGET_FILE:test_wycheproof_aead_128>
        --aead-reader 192:$<TARGET_FILE:test_wycheproof_aead_192>
        --aead-reader 256:$<TARGET_FILE:test_wycheproof_aead_256>)
  endif()

  foreach(profile full relaxed core)
    tc_add_test_library(tiny_crypto_c-test-tlv-${profile}
      src/tlv.c src/tlv_walk.c src/der.c src/aamva.c)
    target_compile_definitions(tiny_crypto_c-test-tlv-${profile} PUBLIC
      TC_ENABLE_TLV=1 TC_ENABLE_AAMVA=1 TC_ENABLE_AES=0 TC_ENABLE_SHA256=0)
    if(profile STREQUAL "core")
      target_compile_definitions(tiny_crypto_c-test-tlv-${profile} PUBLIC
        TC_ENABLE_DER=0 TC_TLV_ENABLE_BER=0 TC_TLV_ENABLE_STREAM=0)
    else()
      target_compile_definitions(tiny_crypto_c-test-tlv-${profile} PUBLIC
        TC_ENABLE_DER=1 TC_TLV_ENABLE_BER=1 TC_TLV_ENABLE_STREAM=1)
    endif()
    if(profile STREQUAL "relaxed")
      target_compile_definitions(tiny_crypto_c-test-tlv-${profile} PUBLIC TC_STRICT=0 TC_ZEROIZE=0)
    endif()
    tc_add_c_test(test_tlv_${profile} tiny_crypto_c-test-tlv-${profile} tests/tlv/test.c)
  endforeach()
  add_executable(test_tlv_corpus_reader tests/tlv/corpus.c)
  target_link_libraries(test_tlv_corpus_reader PRIVATE tiny_crypto_c-test-tlv-full)
  tc_warnings(test_tlv_corpus_reader)
  tc_use_test_sanitizers(test_tlv_corpus_reader)
  set(TINY_CRYPTO_TLV_CORPUS "" CACHE PATH "Optional external PIV/X.509 corpus root")
  tc_add_test_library(tiny_crypto_c-test-key-import
    src/common.c src/tlv.c src/tlv_walk.c src/der.c src/pki_key.c)
  target_compile_definitions(tiny_crypto_c-test-key-import PUBLIC
    TC_ENABLE_AES=0 TC_ENABLE_SHA256=0 TC_ENABLE_X509=0
    TC_ENABLE_TLV=1 TC_ENABLE_DER=1 TC_TLV_ENABLE_BER=0)
  tc_add_c_test(test_rsa_import tiny_crypto_c-test-key-import tests/rsa/import.c)

  tc_add_c_test(test_aamva tiny_crypto_c-test-tlv-full tests/twic/aamva.c src/twic_tpk.c src/common.c)
  target_compile_definitions(test_aamva PRIVATE TC_ENABLE_TWIC_TPK=1)
  tc_add_test_library(tiny_crypto_c-test-twic-cipher src/common.c ${tc_aes_sources} src/twic_cipher.c)
  target_compile_definitions(tiny_crypto_c-test-twic-cipher PUBLIC
    TC_ENABLE_AES=1 TC_AES_ENABLE_ECB=1 TC_ENABLE_TWIC_OBJECT_CRYPTO=1
    TC_ENABLE_SHA256=0)
  tc_add_c_test(test_twic_cipher tiny_crypto_c-test-twic-cipher tests/twic/cipher.c)
  tc_add_c_test(test_twic_reader tiny_crypto_c-test-twic-cipher
    tests/twic/reader.c examples/credential_io.c src/tlv.c src/tlv_walk.c)
  target_compile_definitions(test_twic_reader PRIVATE TC_ENABLE_TLV=1)
  if(APPLE)
    tc_add_c_test(test_twic_pcsc tiny_crypto_c-test-twic-cipher
      tests/twic/pcsc.c examples/credential_pcsc.c)
    add_library(test_credential_command_entry OBJECT examples/credential_check.c)
    target_link_libraries(test_credential_command_entry PRIVATE tiny_crypto_c-test-pki tiny_crypto_c-test-gzip)
    target_compile_definitions(test_credential_command_entry PRIVATE
      main=example_credential_main setrlimit=example_test_setrlimit
      mlock=example_test_mlock munlock=example_test_munlock)
    tc_warnings(test_credential_command_entry)
    tc_use_test_sanitizers(test_credential_command_entry)
    tc_add_c_test(test_twic_command tiny_crypto_c-test-pki
      tests/twic/command.c examples/credential_io.c
      $<TARGET_OBJECTS:test_credential_command_entry>)
    target_link_libraries(test_twic_command PRIVATE tiny_crypto_c-test-gzip)
  endif()
  foreach(profile IN ITEMS runtime fast)
    tc_add_test_library(tiny_crypto_c-test-twic-cipher-${profile}
      src/common.c ${tc_aes_sources} src/twic_cipher.c)
    string(TOUPPER "${profile}" sbox_profile)
    target_compile_definitions(tiny_crypto_c-test-twic-cipher-${profile} PUBLIC
      TC_ENABLE_AES=1 TC_AES_ENABLE_ECB=1 TC_ENABLE_TWIC_OBJECT_CRYPTO=1 TC_ENABLE_SHA256=0
      TC_AES_SBOX_MODE=TC_AES_SBOX_MODE_${sbox_profile})
    tc_add_c_test(test_twic_cipher_${profile}
      tiny_crypto_c-test-twic-cipher-${profile} tests/twic/cipher.c)
  endforeach()
  set(tc_pki_sources src/common.c src/tlv.c src/tlv_walk.c src/der.c src/x509_crl.c src/piv_oid.c src/piv_cms.c src/piv_biometric.c src/piv_certificate.c src/piv_card.c src/piv_printed.c src/key_challenge.c src/lds.c src/piv_security.c src/fascn.c src/twic_uuid.c
    src/piv_cvc.c src/piv_cvc_verify.c src/piv_chuid.c src/credential.c src/validation.c src/x509.c src/x509_crypto.c src/x509_time.c src/x509_key.c src/pki_key.c src/x509_ext.c src/x509_name.c src/x509_path.c src/x509_search.c src/x509_store.c src/cms.c src/cms_validation.c src/x509_revocation.c src/x509_policy.c src/asn1_string.c src/unicode.c src/eac_cvc.c)
  list(APPEND tc_pki_sources src/source.c src/source_der.c src/x509_crl_source.c src/x509_crl_prepare.c)
  tc_add_test_library(tiny_crypto_c-test-pki ${tc_pki_sources})
  target_compile_definitions(tiny_crypto_c-test-pki PUBLIC
    TC_ENABLE_TLV=1 TC_ENABLE_DER=1 TC_TLV_ENABLE_BER=1
    TC_ENABLE_PIV_CVC=1 TC_ENABLE_PIV_CHUID=1 TC_ENABLE_X509=1
    TC_ENABLE_KEY_CHALLENGE=1 TC_ENABLE_FASCN=1 TC_ENABLE_TWIC_UUID=1 TC_ENABLE_PIV_OIDS=1
    TC_ENABLE_X509_PATH=1 TC_ENABLE_X509_REVOCATION=1 TC_ENABLE_CMS=1
    TC_ENABLE_CMS_VALIDATION=1 TC_ENABLE_PIV_OBJECTS=1 TC_ENABLE_CREDENTIAL=1
    TC_ENABLE_AES=0 TC_ENABLE_SHA256=0 TC_ENABLE_EAC_CVC=1)
  tc_add_c_test(test_cms_external_collections tiny_crypto_c-test-pki
    tests/cms/external.c)
  tc_add_test_executable(test_eac_reader tests/eac/reader.c)
  tc_add_c_test(test_eac_cvc tiny_crypto_c-test-pki tests/eac/test.c)
  target_link_libraries(test_eac_reader PRIVATE tiny_crypto_c-test-pki)
  if(Python3_Interpreter_FOUND)
    add_test(NAME test_eac_schema COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/eac/test.py --reader $<TARGET_FILE:test_eac_reader>)
  endif()
  tc_add_c_test(test_piv_cvc tiny_crypto_c-test-pki tests/piv/cvc.c)
  tc_add_c_test(test_piv_chuid tiny_crypto_c-test-pki tests/piv/chuid.c)
  tc_add_c_test(test_piv_oid tiny_crypto_c-test-pki tests/piv/oid.c)
  tc_add_c_test(test_piv_certificate tiny_crypto_c-test-pki tests/piv/certificate.c)
  add_executable(test_piv_certificate_corpus_reader tests/piv/certificate_corpus.c tests/support/munit.c)
  target_include_directories(test_piv_certificate_corpus_reader PRIVATE tests/support)
  target_link_libraries(test_piv_certificate_corpus_reader PRIVATE tiny_crypto_c-test-pki)
  set_target_properties(test_piv_certificate_corpus_reader PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)
  tc_warnings(test_piv_certificate_corpus_reader)
  tc_use_test_sanitizers(test_piv_certificate_corpus_reader)
  tc_add_test_library(tiny_crypto_c-test-gzip src/common.c src/inflate_tree.c
    src/inflate_bits.c src/inflate_tables.c src/inflate.c src/gzip.c src/gzip_api.c)
  target_compile_definitions(tiny_crypto_c-test-gzip PUBLIC
    TC_ENABLE_GZIP=1 TC_ENABLE_AES=0 TC_ENABLE_SHA256=0)
  tc_add_c_test(test_inflate tiny_crypto_c-test-gzip tests/piv/inflate.c)
  if(tc_build_cpp_tests)
    tc_add_linked_test(test_cpp_gzip tiny_crypto_c-test-gzip tests/cpp/gzip.cpp tests/cpp/main.cpp)
    target_include_directories(test_cpp_gzip PRIVATE tests/support)
  endif()
  add_executable(test_gzip_reader tests/gzip/reader.c tests/support/munit.c)
  target_include_directories(test_gzip_reader PRIVATE tests/support)
  target_link_libraries(test_gzip_reader PRIVATE tiny_crypto_c-test-gzip)
  tc_warnings(test_gzip_reader)
  tc_use_test_sanitizers(test_gzip_reader)
  if(Python3_Interpreter_FOUND)
    add_test(NAME test_gzip_differential COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/gzip/differential.py $<TARGET_FILE:test_gzip_reader>)
  endif()
  tc_add_c_test(test_piv_cms_identifiers tiny_crypto_c-test-pki tests/piv/cms_identifiers.c)
  tc_add_c_test(test_lds tiny_crypto_c-test-pki tests/cms/lds.c)
  tc_add_c_test(test_piv_security tiny_crypto_c-test-pki tests/piv/security.c)
  tc_add_c_test(test_piv_biometric tiny_crypto_c-test-pki tests/piv/biometric.c)
  tc_add_c_test(test_piv_printed tiny_crypto_c-test-pki tests/piv/printed.c)
  tc_add_c_test(test_fascn tiny_crypto_c-test-pki tests/piv/fascn.c)
  tc_add_c_test(test_twic_uuid tiny_crypto_c-test-pki tests/twic/uuid.c)
  tc_add_c_test(test_piv_card_identifiers tiny_crypto_c-test-pki tests/piv/card.c)
  tc_add_c_test(test_validation_workspace tiny_crypto_c-test-pki
    tests/validation/workspace.c)
  add_library(test_credential_workflow_entry OBJECT
    examples/credential_workflow.c examples/card_key_policy.c)
  target_link_libraries(test_credential_workflow_entry PRIVATE tiny_crypto_c-test-pki)
  tc_warnings(test_credential_workflow_entry)
  tc_use_test_sanitizers(test_credential_workflow_entry)
  tc_add_test_library(tiny_crypto_c-test-ccl src/twic_ccl.c)
  target_compile_definitions(tiny_crypto_c-test-ccl PUBLIC
    TC_ENABLE_TWIC_CCL=1 TC_ENABLE_AES=0 TC_ENABLE_SHA256=0)
  tc_add_c_test(test_twic_ccl tiny_crypto_c-test-ccl tests/twic/ccl.c)
  target_sources(test_twic_ccl PRIVATE examples/twic_ccl_storage.c examples/twic_ccl_import.c)
  foreach(zeroize IN ITEMS 0 1)
    tc_add_test_library(tiny_crypto_c-test-md5-${zeroize} src/common.c src/md5.c)
    target_compile_definitions(tiny_crypto_c-test-md5-${zeroize} PUBLIC
      TC_ENABLE_MD5=1 TC_ENABLE_AES=0 TC_ENABLE_SHA256=0 TC_ZEROIZE=${zeroize})
    tc_add_c_test(test_md5_${zeroize} tiny_crypto_c-test-md5-${zeroize} tests/hash/md5.c)
    if(tc_build_cpp_tests)
      tc_add_linked_test(test_cpp_md5_${zeroize} tiny_crypto_c-test-md5-${zeroize}
        tests/cpp/md5.cpp tests/cpp/main.cpp)
      target_include_directories(test_cpp_md5_${zeroize} PRIVATE tests/support)
    endif()
  endforeach()
  target_link_libraries(test_twic_ccl PRIVATE tiny_crypto_c-test-md5-1)
  tc_add_c_test(test_x509_key tiny_crypto_c-test-pki tests/x509/key.c)
  tc_add_c_test(test_x509_extensions tiny_crypto_c-test-pki tests/x509/extensions.c)
  tc_add_c_test(test_x509_name tiny_crypto_c-test-pki tests/x509/name.c)
  tc_add_c_test(test_x509_signature tiny_crypto_c-test-pki tests/x509/signature.c)
  tc_add_c_test(test_x509_time tiny_crypto_c-test-pki tests/x509/time.c)
  if(UNIX)
    tc_add_c_test(test_credential_system tiny_crypto_c-test-pki
      tests/twic/system.c examples/credential_system.c)
  endif()
  tc_add_c_test(test_pki_input tiny_crypto_c-test-pki tests/x509/input.c examples/pki_input.c src/twic_ccl.c)
  target_compile_definitions(test_pki_input PRIVATE
    TC_ENABLE_TWIC_CCL=1
    TC_INPUT_FILE="${CMAKE_CURRENT_BINARY_DIR}/pki-input.txt")
  file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/pki-input.txt" CONTENT "sample input\n")
  tc_add_c_test(test_x509_crl tiny_crypto_c-test-pki tests/x509/crl.c)
  tc_add_c_test(test_source tiny_crypto_c-test-pki tests/x509/source.c)
  tc_add_c_test(test_x509_path tiny_crypto_c-test-pki tests/x509/path.c)
  tc_add_c_test(test_x509_store tiny_crypto_c-test-pki tests/x509/store.c)
  tc_add_c_test(test_x509_candidate tiny_crypto_c-test-pki tests/x509/candidate.c)
  target_compile_definitions(test_x509_candidate PRIVATE
    TC_CANDIDATE_FILE="${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/x509/eid_testbeds/csca/CERT_ECARD_CSCA_1.DER")
  tc_add_c_test(test_x509_revocation tiny_crypto_c-test-pki tests/x509/revocation.c)
  tc_add_test_executable(test_x509_path_corpus_reader tests/x509/path_corpus.c tests/support/munit.c)
  target_include_directories(test_x509_path_corpus_reader PRIVATE tests/support)
  target_link_libraries(test_x509_path_corpus_reader PRIVATE tiny_crypto_c-test-pki-native)
  set_target_properties(test_x509_path_corpus_reader PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)
  tc_add_c_test(test_rsa_encoding tiny_crypto_c-test-pki tests/rsa/encoding.c)
  tc_add_c_test(test_cms_attributes tiny_crypto_c-test-pki tests/cms/attributes.c)
  tc_add_c_test(test_cms_algorithm tiny_crypto_c-test-pki tests/cms/algorithm.c)
  tc_add_c_test(test_cms_children tiny_crypto_c-test-pki tests/cms/children.c)
  tc_add_c_test(test_cms_reader tiny_crypto_c-test-pki tests/cms/reader.c examples/cms_reader.c)
  get_target_property(tc_native_pki_sources tiny_crypto_c-test-pki SOURCES)
  tc_add_test_library(tiny_crypto_c-test-pki-native ${tc_native_pki_sources}
    src/hash.c src/sha512.c src/ec.c src/rsa.c ${tc_aes_sources} src/sskdf.c
    src/piv_sm.c src/piv_sm_message.c src/piv_sm_authenticate.c
    src/twic_cipher.c src/twic_tpk.c
    examples/piv_sm_wire.c)
  target_compile_definitions(tiny_crypto_c-test-pki-native PUBLIC
    TC_ENABLE_AES=1 TC_AES_ENABLE_ECB=1 TC_ENABLE_TLV=1 TC_TLV_ENABLE_BER=1 TC_ENABLE_DER=1 TC_ENABLE_X509=1
    TC_ENABLE_KEY_CHALLENGE=1
    TC_ENABLE_PIV_CHUID=1 TC_ENABLE_PIV_CVC=1
    TC_ENABLE_FASCN=1 TC_ENABLE_TWIC_UUID=1 TC_ENABLE_TWIC_TPK=1 TC_ENABLE_PIV_OIDS=1
    TC_ENABLE_TWIC_OBJECT_CRYPTO=1 TC_ENABLE_X509_PATH=1
    TC_ENABLE_X509_REVOCATION=1 TC_ENABLE_CMS=1
    TC_ENABLE_CMS_VALIDATION=1 TC_ENABLE_PIV_OBJECTS=1
    TC_ENABLE_CREDENTIAL=1
    TC_AES_ENABLE_DYNAMIC=1 TC_ENABLE_SSKDF=1 TC_ENABLE_PIV_SM=1
    TC_PIV_SM_ENABLE_CS2=1 TC_PIV_SM_ENABLE_CS7=1 TC_EC_ENABLE_P256=1 TC_EC_ENABLE_P384=1
    TC_ENABLE_EC=1 TC_ENABLE_RSA=1 TC_ENABLE_SHA1=1 TC_ENABLE_SHA224=1
    TC_ENABLE_SHA256=1 TC_ENABLE_SHA384=1 TC_ENABLE_SHA512=1)
  if(tc_build_cpp_tests)
    tc_add_linked_test(test_cpp_credential tiny_crypto_c-test-pki-native
      tests/cpp/credential.cpp tests/cpp/main.cpp)
    target_include_directories(test_cpp_credential PRIVATE tests/support)
  endif()
  tc_add_c_test(test_source_hash tiny_crypto_c-test-pki-native tests/x509/source_hash.c)
  tc_add_c_test(test_piv_sm_authenticate tiny_crypto_c-test-pki-native tests/piv/sm_authenticate.c)
  tc_sm_fixture_header(test_piv_sm_authenticate)
  add_executable(test_piv_cvc_corpus_reader tests/piv/cvc_corpus.c tests/support/munit.c)
  target_include_directories(test_piv_cvc_corpus_reader PRIVATE tests/support)
  target_link_libraries(test_piv_cvc_corpus_reader PRIVATE tiny_crypto_c-test-pki-native)
  set_target_properties(test_piv_cvc_corpus_reader PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)
  tc_warnings(test_piv_cvc_corpus_reader)
  tc_use_test_sanitizers(test_piv_cvc_corpus_reader)
  tc_add_c_test(test_cms_corpus_reader tiny_crypto_c-test-pki-native tests/cms/corpus_reader.c)
  set_tests_properties(test_cms_corpus_reader PROPERTIES
    SKIP_REGULAR_EXPRESSION "No tests run, 2 .* skipped")
  tc_add_c_test(test_cms_octets tiny_crypto_c-test-pki tests/cms/octets.c)
  tc_add_c_test(test_cms_verify tiny_crypto_c-test-pki tests/cms/verify.c)
  tc_add_c_test(test_hash_algorithm tiny_crypto_c-test-pki tests/hash/algorithm.c)
  get_target_property(tc_cms_pki_sources tiny_crypto_c-test-pki SOURCES)
  tc_add_test_library(tiny_crypto_c-test-cms-crypto
    ${tc_cms_pki_sources} src/hash.c src/sha512.c)
  target_compile_definitions(tiny_crypto_c-test-cms-crypto PUBLIC
    TC_ENABLE_AES=0 TC_ENABLE_TLV=1 TC_TLV_ENABLE_BER=1 TC_ENABLE_DER=1 TC_ENABLE_X509=1
    TC_ENABLE_KEY_CHALLENGE=1
    TC_ENABLE_FASCN=1 TC_ENABLE_TWIC_UUID=1 TC_ENABLE_PIV_OIDS=1 TC_ENABLE_X509_PATH=1
    TC_ENABLE_X509_REVOCATION=1 TC_ENABLE_CMS=1
    TC_ENABLE_CMS_VALIDATION=1 TC_ENABLE_PIV_OBJECTS=1
    TC_ENABLE_CREDENTIAL=1 TC_ENABLE_PIV_CHUID=1
    TC_ENABLE_SHA1=1 TC_ENABLE_SHA224=1 TC_ENABLE_SHA256=1 TC_ENABLE_SHA384=1 TC_ENABLE_SHA512=1)
  tc_add_c_test(test_cms_content tiny_crypto_c-test-cms-crypto tests/cms/content.c)
  tc_add_c_test(test_cms_verify_content tiny_crypto_c-test-cms-crypto tests/cms/verify.c)
  tc_add_c_test(test_cms_signer_info tiny_crypto_c-test-pki tests/cms/signer.c examples/cms_reader.c)
  option(TINY_CRYPTO_TEST_OPENSSL "Check PKI and multiprecision arithmetic against OpenSSL 3" OFF)
  if(TINY_CRYPTO_TEST_OPENSSL)
    find_package(OpenSSL 3 REQUIRED COMPONENTS Crypto)
    tc_add_c_test(test_idf_signed_ecdsa tiny_crypto_c-test-ec-0
      tests/esp_idf/signed_ecdsa.c ports/esp-idf/signed_update_ecdsa.c)
    target_compile_definitions(test_idf_signed_ecdsa PRIVATE TC_TEST_IDF_ECDSA=1 ESP_SECURE_BOOT_DIGEST_LEN=32)
    target_include_directories(test_idf_signed_ecdsa PRIVATE tests/esp_idf/include
      ports/esp-idf/vendor/bootloader_support/src/secure_boot_v2)
    target_link_libraries(test_idf_signed_ecdsa PRIVATE OpenSSL::Crypto)
    set_property(TARGET test_idf_signed_ecdsa PROPERTY NO_SYSTEM_FROM_IMPORTED TRUE)
    tc_add_c_test(test_x509_native tiny_crypto_c-test-pki-native tests/x509/native.c examples/x509_client.c)
    tc_add_c_test(test_piv_cvc_verify tiny_crypto_c-test-pki-native tests/piv/cvc_verify.c
      examples/credential_object.c examples/cms_reader.c examples/cms_validate.c examples/pki_input.c)
    target_link_libraries(test_piv_cvc_verify PRIVATE OpenSSL::Crypto)
    set_property(TARGET test_piv_cvc_verify PROPERTY NO_SYSTEM_FROM_IMPORTED TRUE)
    tc_add_c_test(test_card_authentication tiny_crypto_c-test-pki-native
      tests/twic/authentication.c examples/card_key_policy.c
      examples/credential_auth.c examples/credential_validate.c
      examples/credential_io.c examples/pki_input.c src/twic_ccl.c)
    target_compile_definitions(test_card_authentication PRIVATE TC_ENABLE_TWIC_CCL=1)
    target_link_libraries(test_card_authentication PRIVATE OpenSSL::Crypto)
    set_property(TARGET test_card_authentication PROPERTY NO_SYSTEM_FROM_IMPORTED TRUE)
    if(APPLE)
      find_package(ZLIB REQUIRED)
      add_library(twic_authenticate_command OBJECT examples/twic_authenticate.c)
      target_link_libraries(twic_authenticate_command PRIVATE tiny_crypto_c-test-pki-native)
      target_compile_definitions(twic_authenticate_command PRIVATE TC_ENABLE_TWIC_CCL=1 TC_ENABLE_GZIP=1
        main=example_twic_command_main setrlimit=example_twic_setrlimit
        mlock=example_twic_mlock munlock=example_twic_munlock example_read_file=example_twic_read_file
        example_read_created_file=example_twic_read_created_file
        example_twic_cancellation_check=example_twic_command_cancellation_check
        fopen=example_twic_fopen)
      tc_warnings(twic_authenticate_command)
      tc_use_test_sanitizers(twic_authenticate_command)
      tc_add_c_test(test_twic_authenticate_command tiny_crypto_c-test-pki-native
        tests/twic/authenticate_command.c $<TARGET_OBJECTS:twic_authenticate_command>
        examples/card_key_policy.c examples/credential_auth.c
        examples/credential_validate.c examples/credential_io.c
        examples/credential_object.c examples/cms_reader.c examples/cms_validate.c examples/pki_input.c
        examples/x509_revocation.c
        src/twic_ccl.c src/inflate_tree.c src/inflate_bits.c
        src/inflate_tables.c src/inflate.c src/gzip.c src/gzip_api.c)
      target_compile_definitions(test_twic_authenticate_command PRIVATE TC_ENABLE_TWIC_CCL=1 TC_ENABLE_GZIP=1)
      target_link_libraries(test_twic_authenticate_command PRIVATE OpenSSL::Crypto ${ZLIB_LIBRARIES})
      target_include_directories(test_twic_authenticate_command SYSTEM PRIVATE ${ZLIB_INCLUDE_DIRS})
      set_property(TARGET test_twic_authenticate_command PROPERTY NO_SYSTEM_FROM_IMPORTED TRUE)
    endif()
    target_link_libraries(test_x509_native PRIVATE OpenSSL::Crypto)
    set_property(TARGET test_x509_native PROPERTY NO_SYSTEM_FROM_IMPORTED TRUE)
    tc_add_c_test(test_lds_native tiny_crypto_c-test-pki-native tests/cms/lds.c)
    tc_add_c_test(test_cms_native tiny_crypto_c-test-pki-native tests/cms/native.c
      examples/cms_reader.c examples/cms_validate.c examples/credential_object.c examples/x509_revocation.c
      examples/card_key_policy.c examples/credential_workflow.c src/twic_ccl.c)
    target_compile_definitions(test_cms_native PRIVATE TC_ENABLE_TWIC_CCL=1)
    set_tests_properties(test_cms_native PROPERTIES LABELS extended)
    add_test(NAME test_cms_revocation_limits COMMAND $<TARGET_FILE:test_cms_native>
      --param group discovery --param outcome clear /cms/native/revocations)
    add_executable(cms_check examples/cms_check.c examples/cms_reader.c
      examples/cms_validate.c examples/pki_input.c)
    target_link_libraries(cms_check PRIVATE tiny_crypto_c-test-pki-native)
    tc_warnings(cms_check)
    tc_use_test_sanitizers(cms_check)
    if(TINY_CRYPTO_TEST_EC_ORACLE)
      add_test(NAME test_cms_command COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/cms/command.py $<TARGET_FILE:cms_check>)
    endif()
    target_link_libraries(test_cms_native PRIVATE OpenSSL::Crypto)
    set_property(TARGET test_cms_native PROPERTY NO_SYSTEM_FROM_IMPORTED TRUE)
    tc_add_c_test(test_cms_pss tiny_crypto_c-test-pki-native tests/cms/pss.c)
    tc_add_c_test(test_rsa_key_openssl tiny_crypto_c-test-pki-native tests/rsa/key_openssl.c
      examples/rsa_read.c examples/rsa_validate.c examples/rsa_sign.c)
    target_link_libraries(test_rsa_key_openssl PRIVATE OpenSSL::Crypto)
    set_property(TARGET test_rsa_key_openssl PROPERTY NO_SYSTEM_FROM_IMPORTED TRUE)
    target_link_libraries(test_cms_pss PRIVATE OpenSSL::Crypto)
    set_property(TARGET test_cms_pss PROPERTY NO_SYSTEM_FROM_IMPORTED TRUE)
    foreach(rsa_scheme oaep pss)
      foreach(rsa_profile native small)
        set(rsa_test test_rsa_${rsa_scheme}_openssl)
        if(rsa_profile STREQUAL "small")
          string(APPEND rsa_test "_small")
        endif()
        tc_add_c_test(${rsa_test} tiny_crypto_c-test tests/rsa/${rsa_scheme}_openssl.c src/rsa.c)
        if(rsa_scheme STREQUAL "oaep")
          target_sources(${rsa_test} PRIVATE examples/rsa_encrypt.c)
        endif()
        target_compile_definitions(${rsa_test} PRIVATE TC_ENABLE_RSA=1
          TC_RSA_SMALL=$<STREQUAL:${rsa_profile},small>)
        target_link_libraries(${rsa_test} PRIVATE OpenSSL::Crypto)
        set_property(TARGET ${rsa_test} PROPERTY NO_SYSTEM_FROM_IMPORTED TRUE)
      endforeach()
    endforeach()
    foreach(small 0 1)
      tc_add_c_test(test_rsa_oaep_decrypt_${small} tiny_crypto_c-test
        tests/rsa/oaep_decrypt_openssl.c src/rsa.c)
      target_compile_definitions(test_rsa_oaep_decrypt_${small} PRIVATE TC_ENABLE_RSA=1 TC_RSA_SMALL=${small})
      target_link_libraries(test_rsa_oaep_decrypt_${small} PRIVATE OpenSSL::Crypto)
      set_property(TARGET test_rsa_oaep_decrypt_${small} PROPERTY NO_SYSTEM_FROM_IMPORTED TRUE)
      tc_add_c_test(test_rsa_private_openssl_${small} tiny_crypto_c-test-rsa-${small}
        tests/rsa/private_openssl.c examples/rsa_validate.c examples/rsa_sign.c)
      target_link_libraries(test_rsa_private_openssl_${small} PRIVATE OpenSSL::Crypto)
      set_property(TARGET test_rsa_private_openssl_${small} PROPERTY NO_SYSTEM_FROM_IMPORTED TRUE)
      tc_add_c_test(test_rsa_inverse_openssl_${small} tiny_crypto_c-test-rsa-${small} tests/rsa/inverse_openssl.c)
      tc_add_c_test(test_rsa_prime_openssl_${small} tiny_crypto_c-test-rsa-${small} tests/rsa/prime_openssl.c)
      target_link_libraries(test_rsa_prime_openssl_${small} PRIVATE OpenSSL::Crypto)
      set_property(TARGET test_rsa_prime_openssl_${small} PROPERTY NO_SYSTEM_FROM_IMPORTED TRUE)
      target_link_libraries(test_rsa_inverse_openssl_${small} PRIVATE OpenSSL::Crypto)
      set_property(TARGET test_rsa_inverse_openssl_${small} PROPERTY NO_SYSTEM_FROM_IMPORTED TRUE)
      tc_add_c_test(test_ecdsa_openssl_${small} tiny_crypto_c-test-ec-${small} tests/ec/ecdsa_openssl.c)
      target_link_libraries(test_ecdsa_openssl_${small} PRIVATE OpenSSL::Crypto)
      set_property(TARGET test_ecdsa_openssl_${small} PROPERTY NO_SYSTEM_FROM_IMPORTED TRUE)
      tc_add_c_test(test_arithmetic_openssl_${small} tiny_crypto_c-test-ec-${small} tests/ec/arithmetic_openssl.c)
      target_link_libraries(test_arithmetic_openssl_${small} PRIVATE OpenSSL::Crypto)
      set_property(TARGET test_arithmetic_openssl_${small} PROPERTY NO_SYSTEM_FROM_IMPORTED TRUE)
    endforeach()
    tc_add_c_test(test_x509_openssl tiny_crypto_c-test-pki tests/x509/openssl.c)
    target_sources(test_x509_openssl PRIVATE examples/x509_client.c)
    target_link_libraries(test_x509_openssl PRIVATE OpenSSL::Crypto)
    set_property(TARGET test_x509_openssl PROPERTY NO_SYSTEM_FROM_IMPORTED TRUE)
  endif()
  tc_add_c_test(test_unicode tiny_crypto_c-test-pki tests/unicode/test.c)
  set(TINY_CRYPTO_TEST_UNICODE_DIR "" CACHE PATH "Unicode 3.2 data and RFC 3454/4518 reference directory")
  if(TINY_CRYPTO_TEST_UNICODE_DIR)
    if(NOT Python3_Interpreter_FOUND)
      message(FATAL_ERROR "Unicode reference tests require Python 3")
    endif()
    add_test(NAME test_unicode_reference_data COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/unicode_tables.py
      --data-dir ${TINY_CRYPTO_TEST_UNICODE_DIR}
      --stringprep ${TINY_CRYPTO_TEST_UNICODE_DIR}/rfc3454.txt
      --license ${CMAKE_CURRENT_SOURCE_DIR}/LICENSES/Unicode-3.0.txt
      --output ${CMAKE_CURRENT_SOURCE_DIR}/src/unicode_data.inc --check)
    set_tests_properties(test_unicode_reference_data PROPERTIES FIXTURES_SETUP unicode_reference)
    add_test(NAME test_unicode_normalization COMMAND ${CMAKE_COMMAND} -E env
      "TC_UNICODE_NORMALIZATION_FILE=${TINY_CRYPTO_TEST_UNICODE_DIR}/NormalizationTest-3.2.0.txt"
      ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/munit_runner.py
      $<TARGET_FILE:test_unicode> /unicode/normalization-file)
    add_test(NAME test_unicode_oracle COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/unicode/oracle.py --reader $<TARGET_FILE:test_unicode>
      --rfc3454 ${TINY_CRYPTO_TEST_UNICODE_DIR}/rfc3454.txt
      --rfc4518 ${TINY_CRYPTO_TEST_UNICODE_DIR}/rfc4518.txt)
    set_tests_properties(test_unicode_normalization test_unicode_oracle
      PROPERTIES FIXTURES_REQUIRED unicode_reference)
  endif()
  tc_add_test_executable(test_x509_reader tests/x509/reader.c)
  target_link_libraries(test_x509_reader PRIVATE tiny_crypto_c-test-pki)
  if(Python3_Interpreter_FOUND)
    add_test(NAME test_x509_schema COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/x509/test.py --reader $<TARGET_FILE:test_x509_reader>)
  endif()
  tc_add_test_executable(test_piv_object_reader tests/piv/object_reader.c)
  target_link_libraries(test_piv_object_reader PRIVATE tiny_crypto_c-test-pki)
  if(TINY_CRYPTO_TLV_CORPUS AND Python3_Interpreter_FOUND)
    add_test(NAME test_gzip_corpus COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/gzip/corpus.py
      --reader $<TARGET_FILE:test_gzip_reader> --corpus ${TINY_CRYPTO_TLV_CORPUS}/piv)
    add_test(NAME test_piv_certificate_corpus COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/piv/certificate_corpus.py
      --reader $<TARGET_FILE:test_piv_certificate_corpus_reader> --corpus ${TINY_CRYPTO_TLV_CORPUS}/piv)
    if(EXISTS "${TINY_CRYPTO_TLV_CORPUS}/piv/vci_trust_anchors")
      add_test(NAME test_piv_cvc_corpus COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/piv/cvc_corpus.py
        --reader $<TARGET_FILE:test_piv_cvc_corpus_reader> --corpus ${TINY_CRYPTO_TLV_CORPUS}/piv)
    endif()
    if(EXISTS "${TINY_CRYPTO_TLV_CORPUS}/eac/cvc")
      add_test(NAME test_eac_corpus COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/eac/corpus.py
        --reader $<TARGET_FILE:test_eac_reader> --corpus ${TINY_CRYPTO_TLV_CORPUS}/eac/cvc)
    endif()
    add_test(NAME test_x509_corpus COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/x509/corpus.py
      --reader $<TARGET_FILE:test_x509_reader> --corpus ${TINY_CRYPTO_TLV_CORPUS}/x509)
    add_test(NAME test_x509_malformed COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/x509/synthetic.py
      --reader $<TARGET_FILE:test_x509_reader> --corpus ${TINY_CRYPTO_TLV_CORPUS}/x509/synthetic)
    add_test(NAME test_x509_crl_corpus COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/x509/crl_corpus.py
      --reader $<TARGET_FILE:test_x509_crl> --corpus ${TINY_CRYPTO_TLV_CORPUS}/x509/synthetic
      --reference-corpus ${TINY_CRYPTO_TLV_CORPUS}/x509)
    add_test(NAME test_x509_path_corpus COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/x509/path_corpus.py
      --reader $<TARGET_FILE:test_x509_path_corpus_reader> --corpus ${TINY_CRYPTO_TLV_CORPUS}/x509)
    add_test(NAME test_piv_corpus COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/piv/corpus.py
      --reader $<TARGET_FILE:test_piv_object_reader> --corpus ${TINY_CRYPTO_TLV_CORPUS}/piv)
    add_test(NAME test_cms_corpus COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/cms/corpus.py
      --reader $<TARGET_FILE:test_cms_corpus_reader> --corpus ${TINY_CRYPTO_TLV_CORPUS}/piv)
  endif()
  set(TINY_CRYPTO_TLV_MBEDTLS_SUITE "" CACHE FILEPATH "Optional external ASN.1 test data file")
  if(TINY_CRYPTO_TLV_MBEDTLS_SUITE AND Python3_Interpreter_FOUND)
    add_test(NAME test_tlv_external_lengths COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/tlv/corpus.py
      --reader $<TARGET_FILE:test_tlv_corpus_reader> --mbedtls-suite ${TINY_CRYPTO_TLV_MBEDTLS_SUITE})
  endif()
  if(TINY_CRYPTO_TLV_CORPUS AND Python3_Interpreter_FOUND)
    add_test(NAME test_tlv_corpus COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/tlv/corpus.py
      --reader $<TARGET_FILE:test_tlv_corpus_reader> --corpus ${TINY_CRYPTO_TLV_CORPUS})
  endif()
  if(tc_build_cpp_tests)
    foreach(small 0 1)
      tc_add_linked_test(test_cpp_rsa_${small} tiny_crypto_c-test-rsa-${small}
        tests/cpp/rsa.cpp tests/cpp/main.cpp)
      target_include_directories(test_cpp_rsa_${small} PRIVATE tests/support)
      if(TINY_CRYPTO_TEST_OPENSSL)
        tc_add_linked_test(test_cpp_rsa_openssl_${small} tiny_crypto_c-test
          tests/cpp/rsa_openssl.cpp tests/cpp/main.cpp src/rsa.c)
        target_compile_definitions(test_cpp_rsa_openssl_${small} PRIVATE TC_ENABLE_RSA=1 TC_RSA_SMALL=${small})
        target_include_directories(test_cpp_rsa_openssl_${small} PRIVATE tests/support)
        target_link_libraries(test_cpp_rsa_openssl_${small} PRIVATE OpenSSL::Crypto)
        set_property(TARGET test_cpp_rsa_openssl_${small} PROPERTY NO_SYSTEM_FROM_IMPORTED TRUE)
      endif()
    endforeach()
    foreach(pair "sskdf;sskdf" "aes_dynamic;aes-dynamic" "ec;ec-0" "piv_sm;piv-sm")
      list(GET pair 0 name)
      list(GET pair 1 library)
      tc_add_linked_test(test_cpp_${name} tiny_crypto_c-test-${library}
        tests/cpp/${name}.cpp tests/cpp/main.cpp)
      target_include_directories(test_cpp_${name} PRIVATE tests/support)
    endforeach()
    tc_sm_fixture_header(test_cpp_piv_sm)
    foreach(suite cs2 cs7)
      tc_add_linked_test(test_cpp_piv_sm_${suite} tiny_crypto_c-test-piv-sm-${suite}
        tests/cpp/piv_sm.cpp tests/cpp/main.cpp)
      target_include_directories(test_cpp_piv_sm_${suite} PRIVATE tests/support)
      tc_sm_fixture_header(test_cpp_piv_sm_${suite})
    endforeach()
    tc_add_linked_test(test_cpp_tlv tiny_crypto_c-test-tlv-full tests/cpp/tlv.cpp tests/cpp/main.cpp)
    target_include_directories(test_cpp_tlv PRIVATE tests/support)
  endif()
  option(TINY_CRYPTO_BUILD_FUZZERS "Build libFuzzer targets (Clang only)" OFF)
  if(TINY_CRYPTO_BUILD_FUZZERS)
    if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
      message(FATAL_ERROR "Fuzz targets require Clang with libFuzzer")
    endif()
    get_target_property(gzip_fuzz_sources tiny_crypto_c-test-gzip SOURCES)
    get_target_property(gzip_fuzz_definitions tiny_crypto_c-test-gzip COMPILE_DEFINITIONS)
    add_executable(fuzz_gzip tests/gzip/fuzz.c ${gzip_fuzz_sources})
    target_include_directories(fuzz_gzip PRIVATE src)
    target_compile_definitions(fuzz_gzip PRIVATE ${gzip_fuzz_definitions})
    target_compile_options(fuzz_gzip PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(fuzz_gzip PRIVATE -fsanitize=fuzzer,address,undefined)
    tc_warnings(fuzz_gzip)
    add_executable(fuzz_tlv tests/tlv/fuzz.c src/tlv.c src/tlv_walk.c src/der.c)
    target_include_directories(fuzz_tlv PRIVATE src)
    target_compile_definitions(fuzz_tlv PRIVATE TC_ENABLE_TLV=1 TC_ENABLE_DER=1
      TC_TLV_ENABLE_BER=1 TC_TLV_ENABLE_STREAM=1 TC_STRICT=0)
    target_compile_options(fuzz_tlv PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(fuzz_tlv PRIVATE -fsanitize=fuzzer,address,undefined)
    tc_warnings(fuzz_tlv)
    get_target_property(pki_fuzz_sources tiny_crypto_c-test-pki SOURCES)
    get_target_property(pki_fuzz_definitions tiny_crypto_c-test-pki COMPILE_DEFINITIONS)
    list(REMOVE_ITEM pki_fuzz_definitions TC_ENABLE_SHA256=0)
    add_executable(fuzz_pki tests/x509/fuzz.c src/hash.c ${pki_fuzz_sources})
    target_include_directories(fuzz_pki PRIVATE src)
    target_compile_definitions(fuzz_pki PRIVATE ${pki_fuzz_definitions}
      TC_ENABLE_SHA256=1 TC_STRICT=0)
    target_compile_options(fuzz_pki PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(fuzz_pki PRIVATE -fsanitize=fuzzer,address,undefined)
    tc_warnings(fuzz_pki)
    get_target_property(sm_fuzz_sources tiny_crypto_c-test-piv-sm SOURCES)
    get_target_property(sm_fuzz_definitions tiny_crypto_c-test-piv-sm COMPILE_DEFINITIONS)
    add_executable(fuzz_piv_sm tests/piv/sm_fuzz.c ${sm_fuzz_sources})
    target_include_directories(fuzz_piv_sm PRIVATE src)
    target_compile_definitions(fuzz_piv_sm PRIVATE ${sm_fuzz_definitions})
    target_compile_options(fuzz_piv_sm PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(fuzz_piv_sm PRIVATE -fsanitize=fuzzer,address,undefined)
    tc_warnings(fuzz_piv_sm)
  endif()

  tc_add_c_test(test_kmac tiny_crypto_c-test tests/kmac/test.c)
  tc_add_test_library(tiny_crypto_c-test-kmac-relaxed src/common.c src/kmac.c)
  target_compile_definitions(tiny_crypto_c-test-kmac-relaxed PUBLIC
    TC_ENABLE_KMAC256=1 TC_ENABLE_AES=0 TC_ENABLE_SHA256=0
    TC_ZEROIZE=0 TC_STRICT=0)
  tc_add_c_test(test_kmac_relaxed tiny_crypto_c-test-kmac-relaxed tests/kmac/test.c)
  if(TINY_CRYPTO_TEST_WYCHEPROOF_ARCHIVE)
    add_test(NAME test_wycheproof_kmac
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/wycheproof.py
        --archive ${TINY_CRYPTO_TEST_WYCHEPROOF_ARCHIVE}
        --kmac-reader $<TARGET_FILE:test_kmac> --kmac-reader $<TARGET_FILE:test_kmac_relaxed>)
    add_test(NAME test_wycheproof_dynamic_cmac
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/wycheproof.py
        --archive ${TINY_CRYPTO_TEST_WYCHEPROOF_ARCHIVE}
        --cmac-reader $<TARGET_FILE:test_aes_dynamic>)
    add_test(NAME test_wycheproof_hmac
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/wycheproof.py
        --archive ${TINY_CRYPTO_TEST_WYCHEPROOF_ARCHIVE}
        --hmac-reader $<TARGET_FILE:test_hash>)
  endif()

  tc_add_c_test(test_hash tiny_crypto_c-test
    tests/hash/test.c tests/hash/hmac_test.c tests/hash/cavp.c)
  target_include_directories(test_hash PRIVATE tests/hash)
  target_compile_definitions(test_hash PRIVATE
    CAVP_VECTOR_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/hash/cavp"
    HMAC_WYCHEPROOF_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/hash/wycheproof")

  function(tc_add_aes_test target library)
    tc_add_c_test(${target} ${library} tests/aes/test.c tests/aes/cavp.c
      tests/aes/eax_test.c tests/aes/siv_test.c tests/aes/cmac_test.c)
    target_include_directories(${target} PRIVATE tests/aes)
    target_compile_definitions(${target} PRIVATE
      CAVP_VECTOR_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/aes/cavp"
      EAX_VECTOR_FILE="${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/aes/eax/aes_eax_test.json"
      SIV_VECTOR_FILE="${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/aes/siv/aead_aes_siv_cmac_test.json"
      CMAC_WYCHEPROOF_FILE="${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/aes/cmac/aes_cmac_test.json"
      CMAC_CAVP_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/aes/cmac")
  endfunction()

  tc_add_aes_test(test_aes tiny_crypto_c-test)
  tc_add_aes_test(test_aes_192 tiny_crypto_c-test-aes192)
  tc_add_aes_test(test_aes_256 tiny_crypto_c-test-aes256)

  tc_add_c_test(test_des tiny_crypto_c-test
    tests/des/test.c tests/des/test_edge_vectors.c)
  target_include_directories(test_des PRIVATE tests/des)
  target_compile_definitions(test_des PRIVATE
    CAVP_VECTOR_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/des/cavp")

  if(TINY_CRYPTO_TEST_FULL)
    add_executable(test_des_cavp
      tests/des/cavp_main.c tests/des/cavp.c
      tests/support/cavp.c tests/support/munit.c)
    target_link_libraries(test_des_cavp PRIVATE tiny_crypto_c-test)
    target_include_directories(test_des_cavp PRIVATE tests/support tests/des)
    target_compile_definitions(test_des_cavp PRIVATE
      CAVP_VECTOR_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/des/cavp")
    tc_warnings(test_des_cavp)
    tc_use_test_sanitizers(test_des_cavp)
    foreach(group kat mmt mct-ecb mct-cbc mct-cfb1 mct-cfb8 mct-cfb64 mct-ofb)
      add_test(NAME test_des_cavp_${group}
        COMMAND test_des_cavp /tiny-des-cavp/${group})
    endforeach()
  endif()

  # One KDF binary per AES key size: each runs the AES-CMAC CAVP sections for
  # its key size, and the 128-bit binary also runs every non-AES PRF section.
  function(tc_add_kdf_test target library)
    tc_add_c_test(${target} ${library}
      tests/kdf/test.c tests/kdf/kbkdf_test.c tests/kdf/cavp.c)
    target_include_directories(${target} PRIVATE tests/kdf)
    target_compile_definitions(${target} PRIVATE
      KDF_CAVP_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/kdf/cavp")
  endfunction()

  tc_add_kdf_test(test_kdf tiny_crypto_c-test)
  tc_add_kdf_test(test_kdf_192 tiny_crypto_c-test-aes192)
  tc_add_kdf_test(test_kdf_256 tiny_crypto_c-test-aes256)

  tc_add_c_test(test_des_weak_keys_allowed tiny_crypto_c-test
    tests/des/weak_keys_test.c)
  tc_add_c_test(test_des_weak_keys_rejected tiny_crypto_c-test-des-reject-weak
    tests/des/weak_keys_test.c)

  tc_add_c_test(test_default_profile tiny_crypto_c
    tests/default_profile.c)

  tc_add_c_test(test_aes_runtime_sbox tiny_crypto_c-test-aes-runtime-sbox
    tests/aes/runtime_sbox_test.c)

  foreach(ghash_mode 1 2 3 4)
    tc_add_c_test(test_aes_gcm_${ghash_mode}
      tiny_crypto_c-test-gcm-${ghash_mode} tests/aes/gcm_profile_test.c
      tests/aes/gcm_hardware_stub.c)
  endforeach()

  # C++ wrapper suites use the vendored doctest header (tests/support/doctest.h)
  # with one shared main so each binary supports doctest's filters and reporters.
  if(tc_build_cpp_tests)
    foreach(algorithm hash aes des kdf kmac)
      tc_add_linked_test(test_cpp_${algorithm} tiny_crypto_c-test
        tests/cpp/${algorithm}.cpp tests/cpp/main.cpp)
      target_include_directories(test_cpp_${algorithm} PRIVATE
        tests/support tests/${algorithm})
    endforeach()

    foreach(key_bits 192 256)
      foreach(algorithm aes kdf)
        tc_add_linked_test(test_cpp_${algorithm}_${key_bits}
          tiny_crypto_c-test-aes${key_bits}
          tests/cpp/${algorithm}.cpp tests/cpp/main.cpp)
        target_include_directories(test_cpp_${algorithm}_${key_bits} PRIVATE
          tests/support tests/${algorithm})
      endforeach()
    endforeach()
  endif()

  # Compile both umbrellas with each feature family's smallest legal profile.
  # This catches accidental feature coupling and keeps their C API surface equal.
  foreach(header_profile rsa tlv aamva fascn twic_uuid twic_tpk twic_object
      piv_oids x509 key_challenge x509_path x509_revocation cms cms_validation piv_objects credential piv_cvc piv_chuid
      piv_sm twic_ccl)
    set(header_profile_definitions TC_ENABLE_AES=0 TC_ENABLE_SHA256=0)
    if(header_profile STREQUAL "rsa")
      list(APPEND header_profile_definitions TC_ENABLE_RSA=1 TC_TEST_HEADER_RSA=1)
    elseif(header_profile STREQUAL "tlv")
      list(APPEND header_profile_definitions TC_ENABLE_TLV=1 TC_TEST_HEADER_TLV=1)
    elseif(header_profile STREQUAL "aamva")
      list(APPEND header_profile_definitions TC_ENABLE_AAMVA=1 TC_TEST_HEADER_AAMVA=1)
    elseif(header_profile STREQUAL "fascn")
      list(APPEND header_profile_definitions TC_ENABLE_FASCN=1 TC_TEST_HEADER_FASCN=1)
    elseif(header_profile STREQUAL "twic_uuid")
      list(APPEND header_profile_definitions
        TC_ENABLE_FASCN=1 TC_ENABLE_TWIC_UUID=1 TC_TEST_HEADER_TWIC_UUID=1)
    elseif(header_profile STREQUAL "twic_tpk")
      list(APPEND header_profile_definitions
        TC_ENABLE_TLV=1 TC_ENABLE_TWIC_TPK=1 TC_TEST_HEADER_TWIC_TPK=1)
    elseif(header_profile STREQUAL "twic_object")
      list(REMOVE_ITEM header_profile_definitions TC_ENABLE_AES=0)
      list(APPEND header_profile_definitions
        TC_ENABLE_AES=1 TC_AES_ENABLE_ECB=1 TC_ENABLE_TWIC_OBJECT_CRYPTO=1
        TC_TEST_HEADER_TWIC_TPK=1)
    elseif(header_profile STREQUAL "piv_oids")
      list(APPEND header_profile_definitions
        TC_ENABLE_PIV_OIDS=1 TC_TEST_HEADER_PIV_OIDS=1)
    elseif(header_profile STREQUAL "x509")
      list(APPEND header_profile_definitions
        TC_ENABLE_TLV=1 TC_ENABLE_DER=1 TC_ENABLE_X509=1 TC_TEST_HEADER_X509=1)
    elseif(header_profile STREQUAL "key_challenge")
      list(APPEND header_profile_definitions
        TC_ENABLE_TLV=1 TC_ENABLE_DER=1 TC_ENABLE_X509=1
        TC_ENABLE_KEY_CHALLENGE=1 TC_TEST_HEADER_KEY_CHALLENGE=1)
    elseif(header_profile STREQUAL "x509_path")
      list(APPEND header_profile_definitions
        TC_ENABLE_TLV=1 TC_ENABLE_DER=1 TC_ENABLE_X509=1 TC_ENABLE_X509_PATH=1
        TC_TEST_HEADER_X509_PATH=1)
    elseif(header_profile STREQUAL "x509_revocation")
      list(APPEND header_profile_definitions
        TC_ENABLE_TLV=1 TC_ENABLE_DER=1 TC_ENABLE_X509=1 TC_ENABLE_X509_PATH=1
        TC_ENABLE_X509_REVOCATION=1 TC_TEST_HEADER_X509_REVOCATION=1)
    elseif(header_profile STREQUAL "cms")
      list(APPEND header_profile_definitions
        TC_ENABLE_TLV=1 TC_ENABLE_DER=1 TC_TLV_ENABLE_BER=1 TC_ENABLE_X509=1 TC_ENABLE_PIV_OIDS=1
        TC_ENABLE_CMS=1 TC_TEST_HEADER_CMS=1)
    elseif(header_profile STREQUAL "cms_validation")
      list(APPEND header_profile_definitions
        TC_ENABLE_TLV=1 TC_ENABLE_DER=1 TC_TLV_ENABLE_BER=1 TC_ENABLE_X509=1 TC_ENABLE_PIV_OIDS=1
        TC_ENABLE_X509_PATH=1 TC_ENABLE_X509_REVOCATION=1 TC_ENABLE_CMS=1
        TC_ENABLE_CMS_VALIDATION=1 TC_TEST_HEADER_CMS_VALIDATION=1)
    elseif(header_profile STREQUAL "piv_objects")
      list(APPEND header_profile_definitions
        TC_ENABLE_TLV=1 TC_ENABLE_DER=1 TC_TLV_ENABLE_BER=1 TC_ENABLE_X509=1 TC_ENABLE_PIV_OIDS=1
        TC_ENABLE_CMS=1 TC_ENABLE_FASCN=1
        TC_ENABLE_TWIC_UUID=1 TC_ENABLE_PIV_OBJECTS=1 TC_TEST_HEADER_PIV_OBJECTS=1)
    elseif(header_profile STREQUAL "credential")
      list(APPEND header_profile_definitions
        TC_ENABLE_TLV=1 TC_ENABLE_DER=1 TC_TLV_ENABLE_BER=1 TC_ENABLE_X509=1 TC_ENABLE_PIV_OIDS=1 TC_ENABLE_X509_PATH=1
        TC_ENABLE_X509_REVOCATION=1 TC_ENABLE_CMS=1 TC_ENABLE_CMS_VALIDATION=1 TC_ENABLE_FASCN=1
        TC_ENABLE_TWIC_UUID=1 TC_ENABLE_PIV_OBJECTS=1 TC_ENABLE_PIV_CHUID=1
        TC_ENABLE_CREDENTIAL=1 TC_TEST_HEADER_CREDENTIAL=1)
    elseif(header_profile STREQUAL "piv_cvc")
      list(APPEND header_profile_definitions
        TC_ENABLE_TLV=1 TC_ENABLE_DER=1 TC_ENABLE_PIV_CVC=1 TC_TEST_HEADER_PIV_CVC=1)
    elseif(header_profile STREQUAL "piv_chuid")
      list(APPEND header_profile_definitions
        TC_ENABLE_TLV=1 TC_ENABLE_PIV_CHUID=1 TC_TEST_HEADER_PIV_CHUID=1)
    elseif(header_profile STREQUAL "piv_sm")
      list(REMOVE_ITEM header_profile_definitions TC_ENABLE_AES=0 TC_ENABLE_SHA256=0)
      list(APPEND header_profile_definitions
        TC_ENABLE_AES=1 TC_AES_ENABLE_DYNAMIC=1 TC_ENABLE_SHA256=1
        TC_ENABLE_SSKDF=1 TC_ENABLE_EC=1 TC_EC_ENABLE_P256=1
        TC_ENABLE_PIV_SM=1 TC_PIV_SM_ENABLE_CS2=1 TC_PIV_SM_ENABLE_CS7=0
        TC_TEST_HEADER_PIV_SM=1)
    elseif(header_profile STREQUAL "twic_ccl")
      list(APPEND header_profile_definitions TC_ENABLE_TWIC_CCL=1 TC_TEST_HEADER_TWIC_CCL=1)
    endif()

    add_library(test_c_umbrella_${header_profile} OBJECT tests/headers/umbrella_compile.c)
    target_include_directories(test_c_umbrella_${header_profile} PRIVATE src)
    target_compile_definitions(test_c_umbrella_${header_profile} PRIVATE ${header_profile_definitions})
    tc_warnings(test_c_umbrella_${header_profile})

    add_library(test_cpp_umbrella_${header_profile} OBJECT tests/headers/umbrella_compile.cpp)
    target_include_directories(test_cpp_umbrella_${header_profile} PRIVATE src)
    target_compile_definitions(test_cpp_umbrella_${header_profile} PRIVATE ${header_profile_definitions})
    set_property(TARGET test_cpp_umbrella_${header_profile} PROPERTY CXX_STANDARD 11)
    tc_warnings(test_cpp_umbrella_${header_profile})
  endforeach()

  add_library(test_cpp_headers_cxx17 OBJECT tests/cpp/header_compile.cpp)
  target_include_directories(test_cpp_headers_cxx17 PRIVATE src)
  set_property(TARGET test_cpp_headers_cxx17 PROPERTY CXX_STANDARD 17)
  target_compile_definitions(test_cpp_headers_cxx17 PRIVATE
    ${tc_full_definitions} TC_AES_KEY_BITS=128 TC_AES_ENABLE_EAX_PRIME=1 TC_ENABLE_TLV=1
    TC_ENABLE_DER=1 TC_ENABLE_X509=1 TC_ENABLE_PIV_CHUID=1 TC_ENABLE_PIV_CVC=1 TC_ENABLE_EAC_CVC=1
    TC_ENABLE_PIV_SM=1 TC_ENABLE_EC=1 TC_ENABLE_SSKDF=1 TC_ENABLE_SHA384=1 TC_AES_ENABLE_DYNAMIC=1)

  find_program(TC_AVR_CXX NAMES avr-g++)
  find_program(TC_AVR_CC NAMES avr-gcc)
  if(TC_AVR_CC)
    foreach(rsa_source src/rsa.c examples/rsa_validate.c examples/rsa_sign.c examples/rsa_encrypt.c)
      get_filename_component(rsa_name ${rsa_source} NAME_WE)
      add_test(NAME test_${rsa_name}_compile_avr
        COMMAND ${TC_AVR_CC} -std=c99 -Wall -Wextra -Werror -Os -mmcu=atmega2560
          -DTC_ENABLE_RSA=1 -I${CMAKE_CURRENT_SOURCE_DIR}/src
          -c ${CMAKE_CURRENT_SOURCE_DIR}/${rsa_source}
          -o ${CMAKE_CURRENT_BINARY_DIR}/tiny_crypto_c-${rsa_name}-compile.o)
    endforeach()
    foreach(sm_source piv_sm piv_sm_message)
      add_test(NAME test_${sm_source}_compile_avr
        COMMAND ${TC_AVR_CC} -std=c99 -Wall -Wextra -Werror -Os -mmcu=atmega328p
          -DTC_RESOURCE_PROFILE=1 -DTC_ENABLE_PIV_SM=1 -DTC_ENABLE_EC=1
          -DTC_ENABLE_SSKDF=1 -DTC_ENABLE_SHA384=1 -DTC_AES_ENABLE_DYNAMIC=1
          -DTC_ENABLE_TLV=1 -DTC_ENABLE_DER=1 -DTC_ENABLE_PIV_CVC=1
          -I${CMAKE_CURRENT_SOURCE_DIR}/src
          -c ${CMAKE_CURRENT_SOURCE_DIR}/src/${sm_source}.c
          -o ${CMAKE_CURRENT_BINARY_DIR}/tiny_crypto_c-${sm_source}-compile.o)
    endforeach()
    add_test(NAME test_ec_compile_avr
      COMMAND ${TC_AVR_CC} -std=c99 -Wall -Wextra -Werror -Os -mmcu=atmega328p
        -DTC_ENABLE_EC=1 -I${CMAKE_CURRENT_SOURCE_DIR}/src
        -c ${CMAKE_CURRENT_SOURCE_DIR}/src/ec.c
        -o ${CMAKE_CURRENT_BINARY_DIR}/tiny_crypto_c-ec-compile.o)
    add_test(NAME test_sskdf_compile_avr
      COMMAND ${TC_AVR_CC} -std=c99 -Wall -Wextra -Werror -mmcu=atmega328p
        -DTC_ENABLE_SSKDF=1 -DTC_ENABLE_SHA384=1 -I${CMAKE_CURRENT_SOURCE_DIR}/src
        -c ${CMAKE_CURRENT_SOURCE_DIR}/src/sskdf.c
        -o ${CMAKE_CURRENT_BINARY_DIR}/tiny_crypto_c-sskdf-compile.o)
  endif()
  if(TC_AVR_CXX)
    add_test(NAME test_cpp_headers_avr
      COMMAND ${TC_AVR_CXX} -std=gnu++11 -fno-exceptions -fno-rtti
        -DTC_ENABLE_KMAC256=1 -DTC_ENABLE_TLV=1
        -DTC_ENABLE_DER=1 -DTC_ENABLE_X509=1 -DTC_ENABLE_PIV_CHUID=1 -DTC_ENABLE_PIV_CVC=1
        -DTC_ENABLE_EAC_CVC=1 -DTC_ENABLE_SSKDF=1 -DTC_ENABLE_SHA384=1 -DTC_AES_ENABLE_DYNAMIC=1 -DTC_ENABLE_EC=1
        -DTC_ENABLE_PIV_SM=1
        -mmcu=atmega328p -I${CMAKE_CURRENT_SOURCE_DIR}/src
        -c ${CMAKE_CURRENT_SOURCE_DIR}/tests/cpp/header_compile.cpp
        -o ${CMAKE_CURRENT_BINARY_DIR}/tiny_crypto_c-header-compile.o)
  endif()

  set(tc_extended_tests
    test_rsa_validation_1
    test_wycheproof_ec
    test_wycheproof_rsa_signatures
    test_wycheproof_rsa_oaep
    test_wycheproof_ecdsa
    test_rsa_key_openssl
    test_rsa_oaep_openssl
    test_rsa_oaep_openssl_small
    test_rsa_pss_openssl
    test_rsa_pss_openssl_small
    test_rsa_oaep_decrypt_0
    test_rsa_oaep_decrypt_1
    test_rsa_private_openssl_0
    test_rsa_private_openssl_1)
  foreach(tc_extended_test IN LISTS tc_extended_tests)
    if(TEST ${tc_extended_test})
      set_tests_properties(${tc_extended_test} PROPERTIES LABELS extended)
    endif()
  endforeach()
  if(TINY_CRYPTO_TEST_FULL)
    set_tests_properties(
      test_des_cavp_kat test_des_cavp_mmt test_des_cavp_mct-ecb
      test_des_cavp_mct-cbc test_des_cavp_mct-cfb1 test_des_cavp_mct-cfb8
      test_des_cavp_mct-cfb64 test_des_cavp_mct-ofb
      PROPERTIES LABELS extended)
  endif()

endif()
