# SPDX-License-Identifier: GPL-2.0-or-later

if(TINY_CRYPTO_BUILD_TESTS)
  enable_language(CXX)
  set(CMAKE_CXX_STANDARD 11)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
  enable_testing()
  if(CMAKE_NM AND NOT MSVC)
    add_test(NAME test_heap_free COMMAND ${CMAKE_COMMAND}
      -DNM=${CMAKE_NM} -DARCHIVE=$<TARGET_FILE:tiny_crypto_c>
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/heap_free.cmake)
    get_property(tc_feature_options GLOBAL PROPERTY TC_FEATURE_OPTIONS)
    string(REPLACE ";" "," tc_feature_option_list "${tc_feature_options}")
    add_test(NAME test_heap_free_all_features COMMAND ${CMAKE_COMMAND}
      -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR} -DOPTIONS=${tc_feature_option_list}
      -DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/heap-free-all
      -DC_COMPILER=${CMAKE_C_COMPILER} -DNM=${CMAKE_NM} -DAR=${CMAKE_AR}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/heap_free_all.cmake)
    set_tests_properties(test_heap_free_all_features PROPERTIES LABELS extended)
    add_test(NAME test_minimal_core COMMAND ${CMAKE_COMMAND}
      -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR} -DOPTIONS=${tc_feature_option_list}
      -DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/minimal-core
      -DC_COMPILER=${CMAKE_C_COMPILER} -DAR=${CMAKE_AR}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/minimal_core.cmake)
    add_test(NAME test_option_names COMMAND ${CMAKE_COMMAND}
      -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
      -DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/option-names
      -DC_COMPILER=${CMAKE_C_COMPILER}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/option_names.cmake)
  endif()
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
  if(NOT MSVC)
    add_test(NAME test_trust_anchor_options COMMAND ${CMAKE_COMMAND}
      -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
      -DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/trust-anchor-options
      -DC_COMPILER=${CMAKE_C_COMPILER}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/trust_anchor_options.cmake)
  endif()
  find_package(Python3 COMPONENTS Interpreter QUIET)
  if(Python3_Interpreter_FOUND)
    add_test(NAME test_benchmark_report
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_benchmark_report.py)
    add_test(NAME test_unicode_tables
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_unicode_tables.py)
    add_test(NAME test_package_boundaries
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_package_boundaries.py)
    add_test(NAME test_vector_manifests
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_vector_manifests.py)
    add_test(NAME test_cavp_rsp
      COMMAND ${Python3_EXECUTABLE} -m unittest tests.test_cavp_rsp)
    set_tests_properties(test_cavp_rsp PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME test_c_emitter
      COMMAND ${Python3_EXECUTABLE} -m unittest tests.test_c_emitter)
    set_tests_properties(test_c_emitter PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME test_argument_order
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_argument_order.py)
    add_test(NAME test_no_wall_clock
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_no_wall_clock.py)
    add_test(NAME test_feature_registry
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_feature_registry.py)
    if(CMAKE_NM AND NOT MSVC)
      # Each top-level feature alone, with the dependencies config.h requires.
      set(tc_feature_build_cxx)
      if(CMAKE_CXX_COMPILER)
        set(tc_feature_build_cxx --cxx ${CMAKE_CXX_COMPILER})
      endif()
      add_test(NAME test_single_features
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/feature_builds.py
          --cc ${CMAKE_C_COMPILER} --nm ${CMAKE_NM} ${tc_feature_build_cxx}
          --binary-dir ${CMAKE_CURRENT_BINARY_DIR}/single-features)
      set_tests_properties(test_single_features PROPERTIES LABELS extended)
    endif()
    add_test(NAME test_doc_sync
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_doc_sync.py)
    # Self-contained documentation code blocks compile with GCC-compatible
    # drivers. MSVC and clang-cl run the text checks only.
    if(NOT MSVC AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang" AND
       CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
      set_tests_properties(test_doc_sync PROPERTIES ENVIRONMENT
        "TC_DOC_SYNC_CC=${CMAKE_C_COMPILER};TC_DOC_SYNC_CXX=${CMAKE_CXX_COMPILER}")
    endif()
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
  add_test(NAME test_reject_ocsp_without_sha1
    COMMAND ${CMAKE_COMMAND}
      -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
      -DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/invalid-ocsp-profile
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/reject_ocsp_without_sha1.cmake)
  if(NOT MSVC)
    add_test(NAME test_reject_tag_length_config
      COMMAND ${CMAKE_COMMAND}
        -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/tag-length-config
        -DC_COMPILER=${CMAKE_C_COMPILER}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/reject_tag_length_config.cmake)
    add_test(NAME test_reject_removed_switches
      COMMAND ${CMAKE_COMMAND}
        -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/removed-switches
        -DC_COMPILER=${CMAKE_C_COMPILER}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/reject_removed_switches.cmake)
    add_test(NAME test_config_rules
      COMMAND ${CMAKE_COMMAND}
        -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/config-rules
        -DC_COMPILER=${CMAKE_C_COMPILER}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/config_rules.cmake)
    add_test(NAME test_esp_idf_platform
      COMMAND ${CMAKE_COMMAND}
        -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/esp-idf-platform
        -DC_COMPILER=${CMAKE_C_COMPILER}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/esp_idf_platform.cmake)
    add_test(NAME test_header_config_rules
      COMMAND ${CMAKE_COMMAND}
        -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/header-config-rules
        -DC_COMPILER=${CMAKE_C_COMPILER}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/header_config_rules.cmake)
  endif()

  # µunit uses C11 atomics when Clang exposes them in C99 mode. Keep the
  # vendored source unchanged and suppress that extension warning locally.
  if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    set_source_files_properties(tests/support/munit.c PROPERTIES
      COMPILE_OPTIONS -Wno-c11-extensions)
  endif()

  include(${CMAKE_CURRENT_LIST_DIR}/TestProfiles.cmake)
  tc_add_c_test(test_support_hex tiny_crypto_c-test tests/support/hex_test.c)
  tc_add_c_test(test_support_record tiny_crypto_c-test tests/support/record_test.c)
  target_compile_definitions(test_support_record PRIVATE
    TC_TEST_RECORD_DIR="${CMAKE_CURRENT_BINARY_DIR}")
  tc_add_c_test(test_hash_dispatch tiny_crypto_c-test tests/hash/dispatch.c)
  tc_add_c_test(test_rsa_mgf tiny_crypto_c-test tests/rsa/mgf.c)
  tc_add_c_test(test_rsa_pss tiny_crypto_c-test tests/rsa/pss.c)
  tc_add_c_test(test_rsa_oaep tiny_crypto_c-test tests/rsa/oaep.c)
  tc_add_c_test(test_rsa_oaep_arguments tiny_crypto_c-test tests/rsa/oaep_arguments.c ${tc_rsa_sources})
  target_compile_definitions(test_rsa_oaep_arguments PRIVATE TC_ENABLE_RSA=1 TC_RSA_ENABLE_1024=1)
  tc_add_c_test(test_rsa_work tiny_crypto_c-test tests/rsa/work.c ${tc_rsa_sources})
  target_compile_definitions(test_rsa_work PRIVATE TC_ENABLE_RSA=1 TC_RSA_ENABLE_1024=1)
  tc_add_c_test(test_rsa_work_small tiny_crypto_c-test tests/rsa/work.c ${tc_rsa_sources})
  target_compile_definitions(test_rsa_work_small PRIVATE TC_ENABLE_RSA=1 TC_RSA_ENABLE_1024=1 TC_RSA_SMALL=1)
  foreach(feature AES TLV EC)
    tc_add_c_test(test_direct_${feature}_consumer test_direct_${feature} tests/default_profile.c)
  endforeach()
  # PIV card commands: the APDU channel, the TLV readers and the command sources.
  set(tc_piv_command_sources
    src/apdu_encode.c src/apdu_response.c src/apdu_channel.c
    src/tlv.c src/tlv_walk.c src/tlv_write.c src/piv_container_internal.c src/piv_aid.c
    src/piv_link.c src/piv_select.c src/piv_get_data.c src/piv_verify.c src/piv_status.c
    src/piv_template_internal.c)
  # Secure messaging on the card link: the session, CVC reader and commands.
  set(tc_piv_sm_link_sources src/common.c ${tc_aes_sources}
    ${tc_hash_sources} src/sskdf.c src/ec.c src/der.c src/piv_cvc.c
    src/piv_sm.c src/piv_sm_message.c ${tc_piv_command_sources}
    src/piv_sm_apdu.c src/piv_sm_key_request.c)
  foreach(sm_profile dual cs2 cs7 micro mini)
    if(sm_profile STREQUAL "dual")
      set(sm_suffix "")
    else()
      set(sm_suffix "-${sm_profile}")
    endif()
  tc_add_test_library(tiny_crypto_c-test-piv-sm${sm_suffix} ${tc_piv_sm_link_sources})
  target_compile_definitions(tiny_crypto_c-test-piv-sm${sm_suffix} PUBLIC
    TC_RESOURCE_PROFILE=$<IF:$<STREQUAL:${sm_profile},micro>,1,$<IF:$<STREQUAL:${sm_profile},mini>,2,0>>
    TC_ENABLE_PIV_SM=1 TC_AES_ENABLE_DYNAMIC=1 TC_ENABLE_EC=1 TC_ENABLE_SSKDF=1
    TC_ENABLE_SHA384=$<NOT:$<STREQUAL:${sm_profile},cs2>>
    TC_PIV_SM_ENABLE_CS2=$<NOT:$<STREQUAL:${sm_profile},cs7>>
    TC_PIV_SM_ENABLE_CS7=$<NOT:$<STREQUAL:${sm_profile},cs2>>
    TC_EC_ENABLE_P256=$<NOT:$<STREQUAL:${sm_profile},cs7>>
    TC_EC_ENABLE_P384=$<NOT:$<STREQUAL:${sm_profile},cs2>>
    TC_ENABLE_TLV=1 TC_ENABLE_DER=1 TC_ENABLE_PIV_CVC=1
    TC_ENABLE_APDU=1 TC_ENABLE_PIV_COMMAND=1 TC_ENABLE_PIV_SM_APDU=1)
  tc_add_c_test(test_piv_sm${sm_suffix} tiny_crypto_c-test-piv-sm${sm_suffix} tests/piv/sm.c)
  tc_add_c_test(test_piv_sm_apdu${sm_suffix} tiny_crypto_c-test-piv-sm${sm_suffix}
    tests/piv/sm_apdu.c tests/support/sm_card.c tests/support/sm_card_session.c)
  tc_sm_fixture_header(test_piv_sm_apdu${sm_suffix})
  # Driven by tests/piv/synthetic_sm.py and tests/piv/sm_corpus.py.
  tc_add_c_test_executable(test_piv_sm_apdu_replay${sm_suffix}
    tiny_crypto_c-test-piv-sm${sm_suffix} tests/piv/sm_apdu_replay.c)
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
          --reader $<TARGET_FILE:test_piv_sm_apdu_replay${sm_suffix}> ${sm_fixture_args})
    endif()
  endforeach()
  # The virtual contact interface adds the Discovery Object reader. The
  # object-reader switches satisfy config.h, and only piv_discovery.c of that
  # module is linked.
  tc_add_test_library(tiny_crypto_c-test-piv-vci ${tc_piv_sm_link_sources}
    src/piv_discovery.c src/piv_discovery_get.c src/piv_vci.c)
  target_compile_definitions(tiny_crypto_c-test-piv-vci PUBLIC
    TC_ENABLE_PIV_SM=1 TC_AES_ENABLE_DYNAMIC=1 TC_ENABLE_EC=1 TC_ENABLE_SSKDF=1
    TC_ENABLE_SHA384=1 TC_PIV_SM_ENABLE_CS2=1 TC_PIV_SM_ENABLE_CS7=1
    TC_EC_ENABLE_P256=1 TC_EC_ENABLE_P384=1
    TC_ENABLE_TLV=1 TC_ENABLE_DER=1 TC_ENABLE_PIV_CVC=1
    TC_ENABLE_APDU=1 TC_ENABLE_PIV_COMMAND=1 TC_ENABLE_PIV_SM_APDU=1
    TC_TLV_ENABLE_BER=1 TC_ENABLE_X509=1 TC_ENABLE_PIV_OIDS=1 TC_ENABLE_CMS=1
    TC_ENABLE_FASCN=1 TC_ENABLE_TWIC_UUID=1 TC_ENABLE_PIV_OBJECTS=1 TC_ENABLE_PIV_VCI=1)
  tc_add_c_test(test_piv_vci tiny_crypto_c-test-piv-vci tests/piv/vci.c
    tests/support/sm_card.c tests/support/sm_card_session.c
    tests/support/scripted_transport.c)
  tc_sm_fixture_header(test_piv_vci)
  # The PIV card simulator on the SD 33 fixtures written by
  # tests/piv/capture_fixture.py.
  set(tc_card_simulator_sources tests/support/card_simulator.c tests/support/card_fixture.c
    tests/support/sm_card_session.c)
  tc_add_c_test(test_piv_card_simulator tiny_crypto_c-test-piv-vci tests/piv/card_simulator.c
    ${tc_card_simulator_sources})
  target_compile_definitions(test_piv_card_simulator PRIVATE
    TC_CARD_FIXTURE_DIR="${PROJECT_SOURCE_DIR}/tests/vectors/piv/sm_captures/fixtures")
  # The catalog and inventory over the simulator, plain and secured.
  tc_add_test_library(tiny_crypto_c-test-piv-catalog ${tc_piv_sm_link_sources}
    src/piv_discovery.c src/piv_discovery_get.c src/piv_vci.c src/piv_catalog.c
    src/piv_inventory.c)
  target_compile_definitions(tiny_crypto_c-test-piv-catalog PUBLIC
    TC_ENABLE_PIV_SM=1 TC_AES_ENABLE_DYNAMIC=1 TC_ENABLE_EC=1 TC_ENABLE_SSKDF=1
    TC_ENABLE_SHA384=1 TC_PIV_SM_ENABLE_CS2=1 TC_PIV_SM_ENABLE_CS7=1
    TC_EC_ENABLE_P256=1 TC_EC_ENABLE_P384=1
    TC_ENABLE_TLV=1 TC_ENABLE_DER=1 TC_ENABLE_PIV_CVC=1
    TC_ENABLE_APDU=1 TC_ENABLE_PIV_COMMAND=1 TC_ENABLE_PIV_SM_APDU=1
    TC_TLV_ENABLE_BER=1 TC_ENABLE_X509=1 TC_ENABLE_PIV_OIDS=1 TC_ENABLE_CMS=1
    TC_ENABLE_FASCN=1 TC_ENABLE_TWIC_UUID=1 TC_ENABLE_PIV_OBJECTS=1 TC_ENABLE_PIV_VCI=1
    TC_ENABLE_PIV_CATALOG=1)
  tc_add_c_test(test_piv_inventory tiny_crypto_c-test-piv-catalog tests/piv/inventory.c
    ${tc_card_simulator_sources} tests/support/scripted_transport.c)
  target_compile_definitions(test_piv_inventory PRIVATE
    TC_CARD_FIXTURE_DIR="${PROJECT_SOURCE_DIR}/tests/vectors/piv/sm_captures/fixtures")
  if(Python3_Interpreter_FOUND)
    add_test(NAME test_piv_capture_fixtures COMMAND ${Python3_EXECUTABLE}
      ${PROJECT_SOURCE_DIR}/tests/piv/capture_fixture.py --check)
  endif()
  foreach(small 0 1)
    tc_add_test_library(tiny_crypto_c-test-ec-${small} src/common.c src/ec.c ${tc_rsa_sources} src/tlv.c src/tlv_walk.c src/der.c src/x509_key.c src/pki_key.c)
    target_compile_definitions(tiny_crypto_c-test-ec-${small} PUBLIC
      TC_ENABLE_EC=1 TC_EC_ENABLE_P192=1 TC_EC_SMALL=${small} TC_ENABLE_AES=0 TC_ENABLE_SHA256=0
      TC_ENABLE_RSA=1 TC_RSA_ENABLE_1024=1 TC_RSA_SMALL=${small}
      TC_ENABLE_TLV=1 TC_ENABLE_DER=1 TC_ENABLE_X509=1)
    tc_add_c_test(test_ec_${small} tiny_crypto_c-test-ec-${small} tests/ec/test.c)
    tc_add_c_test(test_ecdsa_reader_${small} tiny_crypto_c-test-ec-${small} tests/ec/signature_reader.c)
    tc_add_c_test(test_nist_dss_ec_reader_${small} tiny_crypto_c-test-ec-${small} tests/ec/nist_dss_reader.c)
    tc_add_c_test(test_arithmetic_${small} tiny_crypto_c-test-ec-${small} tests/ec/arithmetic.c)
    # Sign-then-verify with a fault seam, with the check enabled and disabled.
    foreach(check 0 1)
      tc_add_test_library(tiny_crypto_c-test-ec-fault-${small}-${check} src/common.c src/ec.c)
      target_compile_definitions(tiny_crypto_c-test-ec-fault-${small}-${check} PUBLIC
        TC_ENABLE_EC=1 TC_EC_SMALL=${small} TC_ENABLE_AES=0 TC_ENABLE_SHA256=0
        TC_TEST_ECDSA_FAULT=1 TC_ECDSA_SIGN_VERIFY=${check})
      tc_add_c_test(test_ecdsa_sign_verify_${small}_${check}
        tiny_crypto_c-test-ec-fault-${small}-${check} tests/ec/sign_verify.c)
    endforeach()
  endforeach()
  foreach(small 0 1)
    tc_add_test_library(tiny_crypto_c-test-rsa-${small} src/common.c ${tc_rsa_sources})
    target_compile_definitions(tiny_crypto_c-test-rsa-${small} PUBLIC
      TC_ENABLE_RSA=1 TC_RSA_ENABLE_1024=1 TC_RSA_SMALL=${small} TC_ENABLE_AES=0 TC_ENABLE_SHA256=0)
    tc_add_c_test(test_rsa_public_${small} tiny_crypto_c-test-rsa-${small} tests/rsa/public.c)
    tc_add_c_test(test_rsa_validation_${small} tiny_crypto_c-test-rsa-${small} tests/rsa/validation.c)
    tc_add_c_test(test_rsa_inverse_${small} tiny_crypto_c-test-rsa-${small} tests/rsa/inverse.c)
    tc_add_c_test(test_rsa_prime_${small} tiny_crypto_c-test-rsa-${small} tests/rsa/prime.c)
    tc_add_c_test(test_hash_dispatch_disabled_${small} tiny_crypto_c-test-rsa-${small} tests/hash/dispatch.c)
  endforeach()
  tc_add_c_test(test_rsa_oaep_disabled tiny_crypto_c-test-rsa-0 tests/rsa/oaep_arguments.c)
  foreach(small 0 1)
    tc_add_c_test(test_rsa_oaep_reader_${small} tiny_crypto_c-test tests/rsa/oaep_reader.c ${tc_rsa_sources})
    target_compile_definitions(test_rsa_oaep_reader_${small} PRIVATE TC_ENABLE_RSA=1 TC_RSA_ENABLE_1024=1 TC_RSA_SMALL=${small})
  endforeach()
  foreach(curve 256 384)
    tc_add_test_library(tiny_crypto_c-test-ec-p${curve} src/common.c src/ec.c)
    target_compile_definitions(tiny_crypto_c-test-ec-p${curve} PUBLIC
      TC_ENABLE_EC=1 TC_ENABLE_AES=0 TC_ENABLE_SHA256=0
      TC_EC_ENABLE_P256=$<STREQUAL:${curve},256> TC_EC_ENABLE_P384=$<STREQUAL:${curve},384>)
    tc_add_c_test(test_ec_p${curve} tiny_crypto_c-test-ec-p${curve} tests/ec/test.c)
  endforeach()
  tc_add_test_library(tiny_crypto_c-test-ec-rfc6979
    src/common.c src/hash_core.c src/hash.c src/sha512.c src/ec.c)
  target_compile_definitions(tiny_crypto_c-test-ec-rfc6979 PUBLIC
    TC_ENABLE_EC=1 TC_EC_ENABLE_P256=1 TC_EC_ENABLE_P384=1
    TC_ENABLE_SHA256=1 TC_ENABLE_SHA384=1)
  tc_add_c_test(test_ec_rfc6979 tiny_crypto_c-test-ec-rfc6979 tests/ec/rfc6979.c)
  tc_add_test_library(tiny_crypto_c-test-sskdf src/common.c ${tc_hash_sources} src/sskdf.c)
  option(TINY_CRYPTO_TEST_EC_ORACLE "Compare EC with Python cryptography" OFF)
  set(TINY_CRYPTO_TEST_WYCHEPROOF_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/wycheproof"
    CACHE PATH "Pinned C2SP Wycheproof vectors (contains testvectors_v1)")
  set(tc_wycheproof_vectors "${TINY_CRYPTO_TEST_WYCHEPROOF_DIR}/testvectors_v1")
  if(Python3_Interpreter_FOUND)
    add_test(NAME test_wycheproof_runner
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/wycheproof_test.py)
  endif()
  if(TINY_CRYPTO_TEST_WYCHEPROOF_DIR)
    if(NOT Python3_Interpreter_FOUND)
      message(FATAL_ERROR "Wycheproof tests require Python 3")
    endif()
    add_test(NAME test_wycheproof_ec
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/wycheproof.py
        --vectors ${TINY_CRYPTO_TEST_WYCHEPROOF_DIR}
        --ec-reader $<TARGET_FILE:test_ec_0> --ec-reader $<TARGET_FILE:test_ec_1>)
    add_test(NAME test_wycheproof_rsa_signatures
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/wycheproof.py
        --vectors ${TINY_CRYPTO_TEST_WYCHEPROOF_DIR}
        --rsa-signature-reader $<TARGET_FILE:test_rsa_signature_reader>
        --rsa-signature-reader $<TARGET_FILE:test_rsa_signature_reader_small>)
    add_test(NAME test_wycheproof_rsa_generation
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/wycheproof.py
        --vectors ${TINY_CRYPTO_TEST_WYCHEPROOF_DIR}
        --rsa-generation-reader $<TARGET_FILE:test_rsa_generation_reader>)
    add_test(NAME test_wycheproof_primality
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/wycheproof.py
        --vectors ${TINY_CRYPTO_TEST_WYCHEPROOF_DIR}
        --primality-reader $<TARGET_FILE:test_rsa_prime_0>)
    # OAEP decryption dominates the Wycheproof run. Shards split it by limb
    # width and modulus size. Together they cover every supported size.
    foreach(small 0 1)
      foreach(shard "1024;2048" "3072" "4096")
        string(REPLACE ";" "_" shard_name "${shard}")
        set(shard_arguments)
        foreach(bits IN LISTS shard)
          list(APPEND shard_arguments --rsa-oaep-key-size ${bits})
        endforeach()
        add_test(NAME test_wycheproof_rsa_oaep_${small}_${shard_name}
          COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/wycheproof.py
            --vectors ${TINY_CRYPTO_TEST_WYCHEPROOF_DIR} ${shard_arguments}
            --rsa-oaep-reader $<TARGET_FILE:test_rsa_oaep_reader_${small}>)
        list(APPEND tc_wycheproof_oaep_shards test_wycheproof_rsa_oaep_${small}_${shard_name})
      endforeach()
    endforeach()
    add_test(NAME test_wycheproof_ecdsa
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/wycheproof.py
        --vectors ${TINY_CRYPTO_TEST_WYCHEPROOF_DIR}
        --ecdsa-reader $<TARGET_FILE:test_ecdsa_reader_0>
        --ecdsa-reader $<TARGET_FILE:test_ecdsa_reader_1>)
  endif()
  set(TINY_CRYPTO_TEST_EC_CAVP_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/nist_ecccdh"
    CACHE PATH "NIST ECCCDH component test vectors")
  set(TINY_CRYPTO_TEST_ECDSA_DSS_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/nist_dss/186-4ecdsa"
    CACHE PATH "Pinned NIST FIPS 186-4 ECDSA test vectors")
  set(TINY_CRYPTO_TEST_RSA_DSS_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/nist_dss/186-3rsa"
    CACHE PATH "Pinned NIST FIPS 186-3 RSA test vectors")
  if(TINY_CRYPTO_TEST_ECDSA_DSS_DIR OR TINY_CRYPTO_TEST_RSA_DSS_DIR)
    if(NOT Python3_Interpreter_FOUND)
      message(FATAL_ERROR "NIST DSS tests require Python 3")
    endif()
    if(TINY_CRYPTO_TEST_ECDSA_DSS_DIR)
      foreach(small 0 1)
        add_test(NAME test_nist_dss_ec_${small}
          COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/nist_dss.py
            --ecdsa-dir ${TINY_CRYPTO_TEST_ECDSA_DSS_DIR}
            --ecdsa-reader $<TARGET_FILE:test_nist_dss_ec_reader_${small}>)
        add_test(NAME test_nist_dss_ecdsa_signatures_${small}
          COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/nist_dss.py
            --ecdsa-dir ${TINY_CRYPTO_TEST_ECDSA_DSS_DIR}
            --ecdsa-signature-reader $<TARGET_FILE:test_ecdsa_reader_${small}>)
      endforeach()
    endif()
    if(TINY_CRYPTO_TEST_RSA_DSS_DIR)
      add_test(NAME test_nist_dss_rsa_generation
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/nist_dss.py
          --rsa-dir ${TINY_CRYPTO_TEST_RSA_DSS_DIR}
          --rsa-generation-reader $<TARGET_FILE:test_rsa_generation_reader>)
      add_test(NAME test_nist_dss_rsa_signatures
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/nist_dss.py
          --rsa-dir ${TINY_CRYPTO_TEST_RSA_DSS_DIR}
          --rsa-signature-reader $<TARGET_FILE:test_rsa_signature_reader>)
      add_test(NAME test_nist_dss_rsa_keygen_validation
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/nist_dss.py
          --rsa-dir ${TINY_CRYPTO_TEST_RSA_DSS_DIR}
          --rsa-keygen-reader $<TARGET_FILE:test_rsa_keygen_reader>)
    endif()
  endif()
  if(TINY_CRYPTO_TEST_EC_CAVP_DIR)
    if(NOT Python3_Interpreter_FOUND)
      message(FATAL_ERROR "ECCCDH corpus tests require Python 3")
    endif()
    add_test(NAME test_ec_cavp
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/ec/oracle.py
        --reader $<TARGET_FILE:test_ec_0> --reader $<TARGET_FILE:test_ec_1>
        --cavp-dir ${TINY_CRYPTO_TEST_EC_CAVP_DIR})
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
    TC_ENABLE_AES=0 TC_ENABLE_SSKDF=1 TC_ENABLE_SHA1=1 TC_ENABLE_SHA224=1
    TC_ENABLE_SHA256=1 TC_ENABLE_SHA384=1 TC_ENABLE_SHA512=1)
  tc_add_c_test(test_sskdf tiny_crypto_c-test-sskdf tests/kdf/sskdf_test.c)
  # SP 800-90A DRBGs with every mechanism, hash and AES key size available.
  tc_add_test_library(tiny_crypto_c-test-drbg
    src/common.c ${tc_aes_sources} ${tc_hash_sources} ${tc_drbg_sources})
  target_compile_definitions(tiny_crypto_c-test-drbg PUBLIC
    TC_ENABLE_DRBG=1 TC_DRBG_ENABLE_HASH=1 TC_DRBG_ENABLE_HMAC=1 TC_DRBG_ENABLE_CTR=1
    TC_ENABLE_AES=1 TC_AES_ENABLE_DYNAMIC=1 TC_ENABLE_DES=0 TC_ENABLE_HMAC=1
    TC_ENABLE_SHA1=1 TC_ENABLE_SHA224=1 TC_ENABLE_SHA256=1 TC_ENABLE_SHA384=1
    TC_ENABLE_SHA512=1)
  tc_add_c_test(test_internal tiny_crypto_c-test tests/support/internal_test.c)
  tc_add_c_test(test_drbg tiny_crypto_c-test-drbg tests/drbg/test.c)
  tc_add_c_test(test_drbg_cavp tiny_crypto_c-test-drbg tests/drbg/cavp.c)
  tc_add_c_test(test_drbg_example tiny_crypto_c-test-drbg
    tests/drbg/example_test.c examples/drbg.c)
  target_include_directories(test_drbg_example PRIVATE examples)
  # The PlatformIO examples also build and run on the host through main().
  if(tc_build_cpp_tests)
    foreach(example aes_ctr sha256 des_ctr kbkdf)
      tc_add_linked_test(test_example_${example} tiny_crypto_c-test examples/${example}.cpp)
      target_include_directories(test_example_${example} PRIVATE examples)
    endforeach()
  endif()
  target_compile_definitions(test_drbg_cavp PRIVATE
    DRBG_CAVP_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/drbg/cavp")
  set(TINY_CRYPTO_TEST_SM_CAPTURE_DIR "" CACHE PATH "PIV secure messaging capture directory")
  if(Python3_Interpreter_FOUND)
    add_test(NAME test_sm_capture_adapter
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/piv/sm_corpus_test.py)
    # The vendored SD 33 captures replay wire exchanges through the library
    # link. TINY_CRYPTO_TEST_SM_CAPTURE_DIR adds an external capture set.
    set(tc_sm_corpus_dirs ${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/piv/sm_captures)
    if(TINY_CRYPTO_TEST_SM_CAPTURE_DIR)
      list(APPEND tc_sm_corpus_dirs ${TINY_CRYPTO_TEST_SM_CAPTURE_DIR})
    endif()
    set(tc_sm_corpus_suffix "")
    foreach(corpus IN LISTS tc_sm_corpus_dirs)
      add_test(NAME test_sm_primitives_corpus${tc_sm_corpus_suffix}
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/piv/sm_corpus.py
          --reader $<TARGET_FILE:test_sskdf> --corpus ${corpus}
          --ec-reader $<TARGET_FILE:test_ec_0> --ec-reader $<TARGET_FILE:test_ec_1>
          --sm-reader $<TARGET_FILE:test_piv_sm_apdu_replay>)
      set(tc_sm_corpus_suffix "_external")
    endforeach()
  elseif(TINY_CRYPTO_TEST_SM_CAPTURE_DIR)
    message(FATAL_ERROR "PIV capture tests require Python 3")
  endif()
  tc_add_test_library(tiny_crypto_c-test-aes-dynamic src/common.c ${tc_aes_sources})
  target_compile_definitions(tiny_crypto_c-test-aes-dynamic PUBLIC
    TC_AES_ENABLE_DYNAMIC=1 TC_AES_ENABLE_CTR=0 TC_AES_ENABLE_KW=1 TC_ENABLE_SHA256=0)
  tc_add_c_test(test_aes_dynamic tiny_crypto_c-test-aes-dynamic tests/aes/dynamic_test.c
    tests/aes/kw_test.c)
  tc_add_c_test(test_idf_bootloader_hash tiny_crypto_c-test
    tests/esp_idf/bootloader_hash.c ports/esp-idf/bootloader_hash.c)
  # The vendored ESP-IDF source tests sdkconfig macros it may leave undefined.
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties(
      ports/esp-idf/vendor/bootloader_support/src/secure_boot_v2/secure_boot_signatures_app.c
      PROPERTIES COMPILE_OPTIONS -Wno-undef)
  endif()
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
    tests/esp_idf/signed_image.c ports/esp-idf/signed_update_rsa.c ${tc_rsa_sources})
  tc_add_c_test(test_rsa_signature_reader tiny_crypto_c-test tests/rsa/signature_reader.c ${tc_rsa_sources})
  target_compile_definitions(test_rsa_signature_reader PRIVATE TC_ENABLE_RSA=1 TC_RSA_ENABLE_1024=1)
  tc_add_c_test(test_rsa_signature_reader_small tiny_crypto_c-test tests/rsa/signature_reader.c ${tc_rsa_sources})
  target_compile_definitions(test_rsa_signature_reader_small PRIVATE TC_ENABLE_RSA=1 TC_RSA_ENABLE_1024=1 TC_RSA_SMALL=1)
  tc_add_c_test(test_rsa_generation_reader tiny_crypto_c-test tests/rsa/generation_reader.c ${tc_rsa_sources})
  target_compile_definitions(test_rsa_generation_reader PRIVATE TC_ENABLE_RSA=1 TC_RSA_ENABLE_1024=1)
  tc_add_c_test(test_rsa_keygen_reader tiny_crypto_c-test tests/rsa/keygen_reader.c ${tc_rsa_sources})
  target_compile_definitions(test_rsa_keygen_reader PRIVATE TC_ENABLE_RSA=1 TC_RSA_ENABLE_1024=1)
  target_compile_definitions(test_idf_signed_rsa PRIVATE TC_ENABLE_RSA=1 TC_RSA_ENABLE_1024=1)
  target_include_directories(test_idf_signed_rsa PRIVATE tests/esp_idf/include
    ports/esp-idf/vendor/bootloader_support/src/secure_boot_v2)
  tc_add_test_library(tiny_crypto_c-test-idf-ec src/common.c ${tc_hash_sources} src/ec.c)
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
      target_sources(${policy_target} PRIVATE ${tc_rsa_sources})
      target_compile_definitions(${policy_target} PRIVATE TC_ENABLE_RSA=1 TC_RSA_ENABLE_1024=1)
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
  # The mode sources are compiled into the test with the block cipher
  # renamed, so the test's wrappers can fail chosen blocks. The library
  # supplies the real cipher under the same mode configuration.
  tc_add_test_library(tiny_crypto_c-test-aes-modes src/common.c ${tc_aes_sources})
  target_compile_definitions(tiny_crypto_c-test-aes-modes PUBLIC
    TC_ENABLE_DES=0 TC_ENABLE_SHA256=0 TC_AES_ENABLE_DYNAMIC=1 TC_AES_ENABLE_ECB=1
    TC_AES_ENABLE_CBC=1 TC_AES_ENABLE_CTR=1 TC_AES_ENABLE_OFB=1)
  tc_add_c_test(test_aes_mode_failure tiny_crypto_c-test-aes-modes tests/aes/mode_failure.c
    src/aes_modes.c src/block_modes.c)
  target_compile_definitions(test_aes_mode_failure PRIVATE
    tc_aes_cipher_rounds=tc_test_cipher_rounds tc_aes_inverse_rounds=tc_test_inverse_rounds
    tc_aes_cipher=tc_test_cipher)
  tc_add_c_test(test_aes_backend_failure tiny_crypto_c-test-aes-dynamic
    tests/aes/backend_failure.c src/aes_mac.c src/aes_cmac.c src/aes_eax.c src/aes_siv.c
    src/aes_ccm.c src/aes_ghash.c src/aes_gcm.c src/aes_kw.c)
  target_compile_definitions(test_aes_backend_failure PRIVATE
    TC_AES_ENABLE_CMAC=1 TC_AES_ENABLE_SIV=1 TC_AES_ENABLE_EAX=1 TC_AES_ENABLE_EAX_PRIME=1
    TC_AES_ENABLE_CCM=1 TC_AES_ENABLE_GCM=1 TC_AES_ENABLE_KW=1
    tc_aes_cipher_rounds=tc_test_cipher_rounds tc_aes_inverse_rounds=tc_test_inverse_rounds
    tc_aes_cipher=tc_test_cipher)
  foreach(bits 128 192 256)
    if(bits EQUAL 128)
      set(aead_library tiny_crypto_c-test)
    else()
      set(aead_library tiny_crypto_c-test-aes${bits})
    endif()
    tc_add_c_test(test_wycheproof_aead_${bits} ${aead_library} tests/aes/wycheproof.c)
    tc_add_c_test(test_wycheproof_keywrap_${bits} ${aead_library} tests/aes/kw_wycheproof.c)
  endforeach()
  tc_add_c_test(test_wycheproof_keywrap_dynamic tiny_crypto_c-test-aes-dynamic
    tests/aes/kw_wycheproof.c)
  # Archive runners supply fixtures to these executables.
  set_tests_properties(
    test_ecdsa_reader_0 test_ecdsa_reader_1
    test_rsa_signature_reader test_rsa_signature_reader_small
    test_rsa_oaep_reader_0 test_rsa_oaep_reader_1
    test_wycheproof_aead_128 test_wycheproof_aead_192 test_wycheproof_aead_256
    test_wycheproof_keywrap_128 test_wycheproof_keywrap_192 test_wycheproof_keywrap_256
    test_wycheproof_keywrap_dynamic
    PROPERTIES SKIP_REGULAR_EXPRESSION "No tests run, 1 .* skipped")
  if(TINY_CRYPTO_TEST_WYCHEPROOF_DIR)
    add_test(NAME test_wycheproof_aead
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/wycheproof.py
        --vectors ${TINY_CRYPTO_TEST_WYCHEPROOF_DIR}
        --aead-reader 128:$<TARGET_FILE:test_wycheproof_aead_128>
        --aead-reader 192:$<TARGET_FILE:test_wycheproof_aead_192>
        --aead-reader 256:$<TARGET_FILE:test_wycheproof_aead_256>)
    add_test(NAME test_wycheproof_keywrap
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/wycheproof.py
        --vectors ${TINY_CRYPTO_TEST_WYCHEPROOF_DIR}
        --keywrap-reader 128:$<TARGET_FILE:test_wycheproof_keywrap_128>
        --keywrap-reader 192:$<TARGET_FILE:test_wycheproof_keywrap_192>
        --keywrap-reader 256:$<TARGET_FILE:test_wycheproof_keywrap_256>
        --keywrap-dynamic-reader $<TARGET_FILE:test_wycheproof_keywrap_dynamic>)
  endif()

  foreach(profile full core)
    tc_add_test_library(tiny_crypto_c-test-tlv-${profile}
      src/tlv.c src/tlv_walk.c src/tlv_write.c src/der.c src/aamva.c src/credential_text_internal.c)
    target_compile_definitions(tiny_crypto_c-test-tlv-${profile} PUBLIC
      TC_ENABLE_TLV=1 TC_ENABLE_AAMVA=1 TC_ENABLE_AES=0 TC_ENABLE_SHA256=0)
    if(profile STREQUAL "core")
      target_compile_definitions(tiny_crypto_c-test-tlv-${profile} PUBLIC
        TC_ENABLE_DER=0 TC_TLV_ENABLE_BER=0 TC_TLV_ENABLE_STREAM=0)
    else()
      target_compile_definitions(tiny_crypto_c-test-tlv-${profile} PUBLIC
        TC_ENABLE_DER=1 TC_TLV_ENABLE_BER=1 TC_TLV_ENABLE_STREAM=1)
    endif()
    tc_add_c_test(test_tlv_${profile} tiny_crypto_c-test-tlv-${profile} tests/tlv/test.c)
    tc_add_c_test(test_tlv_write_${profile} tiny_crypto_c-test-tlv-${profile} tests/tlv/write.c)
  endforeach()
  add_executable(test_tlv_corpus_reader tests/tlv/corpus.c)
  target_link_libraries(test_tlv_corpus_reader PRIVATE tiny_crypto_c-test-tlv-full)
  target_include_directories(test_tlv_corpus_reader PRIVATE tests/support)
  tc_warnings(test_tlv_corpus_reader)
  tc_use_test_sanitizers(test_tlv_corpus_reader)
  # Parser corpus tests default to the checked-in capture root. Set the option
  # to another root for an external corpus, or to an empty string to skip them.
  set(tc_default_tlv_corpus "")
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/x509")
    set(tc_default_tlv_corpus "${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors")
  endif()
  set(TINY_CRYPTO_TEST_TLV_CORPUS "${tc_default_tlv_corpus}" CACHE PATH
    "PIV/X.509 parser corpus root (empty skips the corpus tests)")
  tc_add_test_library(tiny_crypto_c-test-key-import
    src/common.c src/tlv.c src/tlv_walk.c src/der.c src/pki_key.c)
  target_compile_definitions(tiny_crypto_c-test-key-import PUBLIC
    TC_ENABLE_AES=0 TC_ENABLE_SHA256=0 TC_ENABLE_X509=0
    TC_ENABLE_TLV=1 TC_ENABLE_DER=1 TC_TLV_ENABLE_BER=0)
  tc_add_c_test(test_rsa_import tiny_crypto_c-test-key-import tests/rsa/import.c)

  # The APDU codec depends on no other module (ISO/IEC 7816-4 5.2 to 5.6).
  tc_add_test_library(tiny_crypto_c-test-apdu
    src/common.c src/apdu_encode.c src/apdu_response.c src/apdu_channel.c)
  target_compile_definitions(tiny_crypto_c-test-apdu PUBLIC
    TC_ENABLE_APDU=1 TC_ENABLE_AES=0 TC_ENABLE_SHA256=0)
  tc_add_c_test(test_apdu_encode tiny_crypto_c-test-apdu tests/apdu/encode.c)
  tc_add_c_test(test_apdu_response tiny_crypto_c-test-apdu tests/apdu/response.c)
  tc_add_c_test(test_apdu_channel tiny_crypto_c-test-apdu tests/apdu/channel.c
    tests/support/scripted_transport.c)

  # PIV card commands need the APDU channel and the TLV readers. The catalog
  # needs nothing more, so the plain inventory builds here too.
  tc_add_test_library(tiny_crypto_c-test-piv-command src/common.c ${tc_piv_command_sources}
    src/piv_catalog.c src/piv_inventory.c)
  target_compile_definitions(tiny_crypto_c-test-piv-command PUBLIC
    TC_ENABLE_APDU=1 TC_ENABLE_TLV=1 TC_ENABLE_PIV_COMMAND=1 TC_ENABLE_PIV_CATALOG=1
    TC_ENABLE_AES=0 TC_ENABLE_SHA256=0)
  tc_add_c_test(test_piv_command tiny_crypto_c-test-piv-command tests/piv/command.c
    tests/support/scripted_transport.c)

  tc_add_c_test(test_aamva tiny_crypto_c-test-tlv-full tests/twic/aamva.c
    src/twic_tpk.c src/common.c)
  target_compile_definitions(test_aamva PRIVATE TC_ENABLE_TWIC_TPK=1)
  # The core TLV library enables AAMVA alone, the smallest credential text user.
  tc_add_c_test(test_credential_text tiny_crypto_c-test-tlv-core tests/piv/credential_text.c)
  tc_add_test_library(tiny_crypto_c-test-twic-cipher src/common.c ${tc_aes_sources} src/twic_cipher.c)
  target_compile_definitions(tiny_crypto_c-test-twic-cipher PUBLIC
    TC_ENABLE_AES=1 TC_AES_ENABLE_ECB=1 TC_ENABLE_TWIC_OBJECT_CRYPTO=1
    TC_ENABLE_SHA256=0)
  tc_add_c_test(test_twic_cipher tiny_crypto_c-test-twic-cipher tests/twic/cipher.c)

  if(Python3_Interpreter_FOUND)
    add_test(NAME test_twic_apdu_corpus COMMAND ${Python3_EXECUTABLE}
      ${PROJECT_SOURCE_DIR}/tests/twic/apdu_replay.py --check)
  endif()
  if(APPLE)
    tc_add_c_test(test_twic_pcsc tiny_crypto_c-test-twic-cipher
      tests/twic/pcsc.c examples/credential_pcsc.c)
    add_library(test_credential_command_entry OBJECT examples/credential_check.c)
    target_link_libraries(test_credential_command_entry PRIVATE tiny_crypto_c-test-pki-native)
    target_compile_definitions(test_credential_command_entry PRIVATE
      main=example_credential_main setrlimit=example_test_setrlimit
      mlock=example_test_mlock munlock=example_test_munlock)
    tc_warnings(test_credential_command_entry)
    tc_use_test_sanitizers(test_credential_command_entry)
    tc_add_c_test(test_twic_command tiny_crypto_c-test-pki-native
      tests/twic/command.c $<TARGET_OBJECTS:test_credential_command_entry>)
    # The piv_inspect command builds here for its warnings. The installed
    # consumer links and runs it.
    add_library(test_piv_inspect_main OBJECT examples/piv_inspect_main.c)
    target_link_libraries(test_piv_inspect_main PRIVATE tiny_crypto_c-test-pki-native)
    tc_warnings(test_piv_inspect_main)
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
  set(tc_pki_sources src/common.c src/tlv.c src/tlv_walk.c src/der.c src/x509_crl.c src/x509_crl_extensions.c src/x509_crl_selected.c src/x509_crl_evidence.c src/x509_crl_entries.c src/pki_storage.c src/piv_oid.c src/piv_container_internal.c src/credential_text_internal.c src/piv_cms.c src/piv_biometric.c src/piv_certificate.c src/piv_card.c src/piv_printed.c src/key_challenge.c src/lds.c src/piv_security.c src/fascn.c src/twic_uuid.c
    src/piv_cvc.c src/piv_cvc_verify.c src/piv_chuid.c src/credential.c src/credential_policy.c src/credential_session.c src/credential_security.c src/credential_signer.c src/validation.c src/x509.c src/x509_crypto.c src/x509_time.c src/x509_key.c src/pki_key.c src/pki_signature_oid.c src/x509_ext.c src/x509_name.c src/x509_name_constraints.c src/x509_path.c src/x509_path_extensions.c src/x509_path_workspace.c src/x509_search.c src/x509_store.c src/x509_store_anchor.c src/snapshot.c src/cms.c src/cms_collections.c src/cms_path.c src/x509_revocation.c src/x509_crl_scope.c src/x509_crl_scope_storage.c src/x509_crl_delta.c src/x509_policy.c src/asn1_string.c src/unicode.c src/eac_cvc.c)
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
    tests/cms/external.c tests/support/cms_crl_harness.c tests/support/x509_crl_harness.c)
  tc_add_test_executable(test_eac_reader tests/eac/reader.c)
  tc_add_c_test(test_eac_cvc tiny_crypto_c-test-pki tests/eac/test.c)
  # Build eac_cvc.c with the POSIX file and process headers visible so a
  # static helper that reuses a POSIX name such as open() fails the build.
  include(CheckIncludeFile)
  check_include_file(fcntl.h TC_TEST_HAVE_FCNTL_H)
  check_include_file(unistd.h TC_TEST_HAVE_UNISTD_H)
  if(NOT MSVC AND TC_TEST_HAVE_FCNTL_H AND TC_TEST_HAVE_UNISTD_H)
    add_library(tiny_crypto_c-test-eac-posix-names OBJECT src/eac_cvc.c)
    target_link_libraries(tiny_crypto_c-test-eac-posix-names PRIVATE tiny_crypto_c-test-pki)
    target_compile_options(tiny_crypto_c-test-eac-posix-names PRIVATE
      "SHELL:-include fcntl.h" "SHELL:-include unistd.h")
    tc_warnings(tiny_crypto_c-test-eac-posix-names)
  endif()
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
  tc_add_c_test(test_inflate tiny_crypto_c-test-gzip tests/gzip/inflate.c)
  tc_add_c_test(test_gzip_decode tiny_crypto_c-test-gzip tests/gzip/decode.c)
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
  # The card object readers need only the TLV readers, the shared AID table
  # and GZIP for compressed certificates.
  tc_add_test_library(tiny_crypto_c-test-piv-objects
    src/common.c src/tlv.c src/tlv_walk.c src/piv_container_internal.c src/piv_aid.c
    src/piv_discovery.c src/piv_ccc.c src/piv_key_history.c src/piv_bit_group.c
    src/piv_pairing_code.c src/piv_certificate.c src/piv_certificate_decode.c
    src/piv_card_objects_internal.c)
  target_compile_definitions(tiny_crypto_c-test-piv-objects PUBLIC
    TC_ENABLE_TLV=1 TC_ENABLE_DER=1 TC_TLV_ENABLE_BER=1 TC_ENABLE_X509=1 TC_ENABLE_PIV_OIDS=1
    TC_ENABLE_CMS=1 TC_ENABLE_FASCN=1 TC_ENABLE_TWIC_UUID=1 TC_ENABLE_PIV_OBJECTS=1
    TC_ENABLE_GZIP=1
    TC_ENABLE_AES=0 TC_ENABLE_SHA256=0)
  target_link_libraries(tiny_crypto_c-test-piv-objects PUBLIC tiny_crypto_c-test-gzip)
  tc_add_c_test(test_piv_card_objects tiny_crypto_c-test-piv-objects tests/piv/card_objects.c)
  target_compile_definitions(test_piv_card_objects PRIVATE
    TC_PIV_VECTOR_DIR="${PROJECT_SOURCE_DIR}/tests/vectors/piv")
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
  # The workflow applies the key policy of the key proofs.
  add_library(test_credential_workflow_entry OBJECT examples/credential_workflow.c)
  target_link_libraries(test_credential_workflow_entry PRIVATE tiny_crypto_c-test-pki-native)
  # The workflow checks the TWIC canceled card list, which tiny_crypto_c-test-ccl links.
  target_compile_definitions(test_credential_workflow_entry PRIVATE TC_ENABLE_TWIC_CCL=1)
  tc_warnings(test_credential_workflow_entry)
  tc_use_test_sanitizers(test_credential_workflow_entry)
  tc_add_test_library(tiny_crypto_c-test-ccl src/twic_ccl.c src/snapshot.c src/credential_text_internal.c)
  target_compile_definitions(tiny_crypto_c-test-ccl PUBLIC
    TC_ENABLE_TWIC_CCL=1 TC_ENABLE_AES=0 TC_ENABLE_SHA256=0)
  tc_add_c_test(test_twic_ccl tiny_crypto_c-test-ccl tests/twic/ccl.c)
  target_sources(test_twic_ccl PRIVATE examples/twic_ccl_storage.c examples/twic_ccl_import.c)
  tc_add_test_library(tiny_crypto_c-test-md5 src/common.c src/md5.c src/hash_core.c)
  target_compile_definitions(tiny_crypto_c-test-md5 PUBLIC
    TC_ENABLE_MD5=1 TC_ENABLE_AES=0 TC_ENABLE_SHA256=0)
  tc_add_c_test(test_md5 tiny_crypto_c-test-md5 tests/hash/md5.c)
  if(tc_build_cpp_tests)
    tc_add_linked_test(test_cpp_md5 tiny_crypto_c-test-md5 tests/cpp/md5.cpp tests/cpp/main.cpp)
    target_include_directories(test_cpp_md5 PRIVATE tests/support)
  endif()
  target_link_libraries(test_twic_ccl PRIVATE tiny_crypto_c-test-md5)
  tc_add_c_test(test_x509_key tiny_crypto_c-test-pki tests/x509/key.c)
  tc_add_c_test(test_x509_extensions tiny_crypto_c-test-pki tests/x509/extensions.c)
  target_compile_definitions(test_x509_extensions PRIVATE
    TC_FPKI_CERTIFICATE="${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/x509/fpki/fcpcag2.crt")
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
  tc_add_c_test(test_x509_crl tiny_crypto_c-test-pki tests/x509/crl.c tests/support/cms_crl_harness.c tests/support/x509_crl_harness.c)
  tc_add_c_test(test_source tiny_crypto_c-test-pki tests/x509/source.c)
  tc_add_c_test(test_x509_path tiny_crypto_c-test-pki tests/x509/path.c)
  target_compile_definitions(test_x509_path PRIVATE
    TC_PKITS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/x509/nist/pkits/certs")
  tc_add_c_test(test_x509_anchor_constraints tiny_crypto_c-test-pki-native
    tests/x509/anchor_constraints.c)
  target_compile_definitions(test_x509_anchor_constraints PRIVATE
    TC_TWIC_SYNTHETIC_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/twic/synthetic")
  tc_add_c_test(test_x509_anchor_controls tiny_crypto_c-test-pki-native
    tests/x509/anchor_controls.c)
  target_compile_definitions(test_x509_anchor_controls PRIVATE
    TC_PKITS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/x509/nist/pkits/certs")
  tc_add_c_test(test_x509_store tiny_crypto_c-test-pki tests/x509/store.c)
  tc_add_c_test(test_x509_candidate tiny_crypto_c-test-pki tests/x509/candidate.c)
  target_compile_definitions(test_x509_candidate PRIVATE
    TC_CANDIDATE_FILE="${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/x509/eid_testbeds/csca/CERT_ECARD_CSCA_1.DER")
  tc_add_c_test(test_x509_revocation tiny_crypto_c-test-pki tests/x509/revocation.c
    tests/support/x509_crl_harness.c)
  tc_add_test_executable(test_x509_path_corpus_reader tests/x509/path_corpus.c tests/support/munit.c)
  target_include_directories(test_x509_path_corpus_reader PRIVATE tests/support)
  target_link_libraries(test_x509_path_corpus_reader PRIVATE tiny_crypto_c-test-pki-native)
  set_target_properties(test_x509_path_corpus_reader PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)
  tc_add_c_test(test_rsa_encoding tiny_crypto_c-test-pki tests/rsa/encoding.c)
  tc_add_c_test(test_cms_attributes tiny_crypto_c-test-pki tests/cms/attributes.c)
  tc_add_c_test(test_cms_algorithm tiny_crypto_c-test-pki tests/cms/algorithm.c)
  tc_add_c_test(test_cms_children tiny_crypto_c-test-pki tests/cms/children.c
    tests/support/cms_crl_harness.c tests/support/x509_crl_harness.c)
  tc_add_c_test(test_cms_reader tiny_crypto_c-test-pki tests/cms/reader.c examples/cms_reader.c)
  get_target_property(tc_native_pki_sources tiny_crypto_c-test-pki SOURCES)
  # The native PKI library also carries the PIV card stack up to key proofs,
  # so card tests can verify with the native signature provider.
  set(tc_native_card_sources ${tc_piv_command_sources} src/piv_sm_apdu.c
    src/piv_sm_key_request.c src/piv_discovery.c src/piv_discovery_get.c src/piv_vci.c
    src/piv_catalog.c src/piv_inventory.c src/piv_key_policy.c src/piv_key_proof.c
    src/piv_card_check.c src/piv_card_check_certificates.c src/piv_card_check_signed.c
    src/piv_card_check_keys.c src/piv_card_check_report.c src/piv_card_crl_targets.c src/inflate_tree.c src/inflate_bits.c src/inflate_tables.c src/inflate.c src/gzip.c
    src/gzip_api.c src/piv_certificate_decode.c src/piv_bit_group.c src/piv_ccc.c
    src/piv_key_history.c src/piv_pairing_code.c src/piv_card_objects_internal.c)
  list(REMOVE_ITEM tc_native_card_sources ${tc_native_pki_sources})
  tc_add_test_library(tiny_crypto_c-test-pki-native ${tc_native_pki_sources}
    src/x509_trust_anchor.c src/x509_ocsp.c
    ${tc_hash_sources} src/ec.c ${tc_rsa_sources} ${tc_aes_sources} src/sskdf.c
    src/piv_sm.c src/piv_sm_message.c src/piv_sm_authenticate.c
    src/twic_cipher.c src/twic_tpk.c ${tc_native_card_sources})
  target_compile_definitions(tiny_crypto_c-test-pki-native PUBLIC
    TC_ENABLE_AES=1 TC_AES_ENABLE_ECB=1 TC_ENABLE_TLV=1 TC_TLV_ENABLE_BER=1 TC_ENABLE_DER=1 TC_ENABLE_X509=1
    TC_ENABLE_KEY_CHALLENGE=1
    TC_ENABLE_PIV_CHUID=1 TC_ENABLE_PIV_CVC=1
    TC_ENABLE_FASCN=1 TC_ENABLE_TWIC_UUID=1 TC_ENABLE_TWIC_TPK=1 TC_ENABLE_PIV_OIDS=1
    TC_ENABLE_TWIC_OBJECT_CRYPTO=1 TC_ENABLE_X509_PATH=1
    TC_ENABLE_TRUST_ANCHOR_FORMAT=1
    TC_ENABLE_X509_REVOCATION=1 TC_ENABLE_X509_OCSP=1 TC_ENABLE_CMS=1
    TC_ENABLE_CMS_VALIDATION=1 TC_ENABLE_PIV_OBJECTS=1
    TC_ENABLE_CREDENTIAL=1 TC_ENABLE_APDU=1 TC_ENABLE_PIV_COMMAND=1 TC_ENABLE_PIV_SM_APDU=1
    TC_ENABLE_PIV_VCI=1 TC_ENABLE_PIV_CATALOG=1 TC_ENABLE_PIV_KEY_PROOF=1 TC_ENABLE_GZIP=1
    TC_ENABLE_PIV_CARD_CHECK=1 TC_AES_ENABLE_DYNAMIC=1 TC_ENABLE_SSKDF=1 TC_ENABLE_PIV_SM=1
    TC_PIV_SM_ENABLE_CS2=1 TC_PIV_SM_ENABLE_CS7=1 TC_EC_ENABLE_P192=1 TC_EC_ENABLE_P256=1 TC_EC_ENABLE_P384=1
    TC_ENABLE_EC=1 TC_ENABLE_RSA=1 TC_RSA_ENABLE_1024=1 TC_ENABLE_SHA1=1 TC_ENABLE_SHA224=1
    TC_ENABLE_SHA256=1 TC_ENABLE_SHA384=1 TC_ENABLE_SHA512=1)
  tc_add_c_test(test_key_challenge_rsa tiny_crypto_c-test-pki-native
    tests/x509/key_challenge_rsa.c)
  # The TWIC replays verify the card authentication proof.
  tc_add_c_test(test_twic_apdu_replay tiny_crypto_c-test-pki-native tests/twic/apdu_replay.c)
  target_compile_definitions(test_twic_apdu_replay PRIVATE
    TC_TWIC_VECTOR_DIR="${PROJECT_SOURCE_DIR}/tests/vectors/twic/synthetic")
  # Key proofs over the SD 33 simulators with the native provider.
  tc_add_c_test(test_piv_key_proof tiny_crypto_c-test-pki-native tests/piv/key_proof.c
    ${tc_card_simulator_sources} tests/support/scripted_transport.c)
  target_compile_definitions(test_piv_key_proof PRIVATE
    TC_CARD_FIXTURE_DIR="${PROJECT_SOURCE_DIR}/tests/vectors/piv/sm_captures/fixtures")
  # The composed card check over the SD 33 simulators, CRLs and OCSP.
  tc_add_c_test(test_piv_card_check tiny_crypto_c-test-pki-native tests/piv/card_check.c
    ${tc_card_simulator_sources})
  target_compile_definitions(test_piv_card_check PRIVATE
    TC_CARD_FIXTURE_DIR="${PROJECT_SOURCE_DIR}/tests/vectors/piv/sm_captures/fixtures"
    TC_VECTOR_DIR="${PROJECT_SOURCE_DIR}/tests/vectors")
  # examples/piv_inspect over the SD 33 simulators, compared with the golden
  # outputs in tests/vectors/piv/inspect. The PIN and pairing codes are the
  # published SD 33 test values.
  set(tc_piv_inspect_sources examples/piv_inspect.c examples/piv_inspect_trust.c examples/piv_inspect_crl.c
    examples/piv_inspect_print_objects.c examples/piv_inspect_print_certificates.c
    examples/piv_inspect_print_report.c)
  tc_add_c_test_executable(test_piv_inspect_replay tiny_crypto_c-test-pki-native
    tests/piv/inspect_replay.c ${tc_piv_inspect_sources} ${tc_card_simulator_sources}
    tests/piv/hardware/guard.c)
  target_compile_definitions(test_piv_inspect_replay PRIVATE
    TC_CARD_FIXTURE_DIR="${PROJECT_SOURCE_DIR}/tests/vectors/piv/sm_captures/fixtures"
    TC_VECTOR_DIR="${PROJECT_SOURCE_DIR}/tests/vectors")
  add_test(NAME test_example_piv_inspect COMMAND test_piv_inspect_replay
    /piv/inspect/card2-without-pin /piv/inspect/card2-tampered
    /piv/inspect/card2-unrequired-failure /piv/inspect/card2-key-establishment-refused
    /piv/inspect/arguments)
  foreach(case IN ITEMS card2_contactless card2_contact card4_contactless card2_wrong_pin
      card2_guarded card2_guard_identity)
    string(REPLACE "_" "-" case_name "${case}")
    set(case_pairing 00000002)
    if(case MATCHES "^card4")
      set(case_pairing 00000004)
    endif()
    add_test(NAME test_example_piv_inspect_${case}
      COMMAND test_piv_inspect_replay /piv/inspect/${case_name})
    set_tests_properties(test_example_piv_inspect_${case} PROPERTIES
      ENVIRONMENT "TC_PIV_PIN=123456;TC_PIV_PAIRING_CODE=${case_pairing}")
  endforeach()
  # The PIV card hardware scenarios over the SD 33 simulators, with the
  # transmit guard in front of the card. The PIN and pairing codes are the
  # published SD 33 test values.
  set(tc_piv_card_sources tests/piv/hardware/piv_card.c tests/piv/hardware/guard.c
    tests/piv/hardware/card_config.c examples/piv_inspect_trust.c examples/pki_input.c)
  if(NOT WIN32)
    tc_add_c_test_executable(test_piv_card_simulated tiny_crypto_c-test-pki-native
      ${tc_piv_card_sources} tests/piv/hardware/backend_simulated.c ${tc_card_simulator_sources})
    target_compile_definitions(test_piv_card_simulated PRIVATE
      TC_CARD_FIXTURE_DIR="${PROJECT_SOURCE_DIR}/tests/vectors/piv/sm_captures/fixtures"
      TC_VECTOR_DIR="${PROJECT_SOURCE_DIR}/tests/vectors")
    foreach(case IN ITEMS card2_contactless card2_contact card4_contactless)
      string(REGEX MATCH "^card[0-9]" case_card "${case}")
      string(REGEX REPLACE "^card[0-9]_" "" case_interface "${case}")
      string(REPLACE "card" "" case_number "${case_card}")
      set(case_environment "TC_PIV_PIN=123456;TC_PIV_PAIRING_CODE=0000000${case_number}"
        "TC_PIV_CARD_EXPECT=sd33-${case_card};TC_PIV_CARD_INTERFACE=${case_interface}")
      if(case STREQUAL "card2_contact")
        list(APPEND case_environment TC_PIV_CARD_EXTENDED=1 TC_PIV_CARD_REVOCATION=required)
      endif()
      add_test(NAME test_piv_card_simulated_${case} COMMAND test_piv_card_simulated)
      set_tests_properties(test_piv_card_simulated_${case} PROPERTIES
        ENVIRONMENT "${case_environment}")
    endforeach()
  endif()
  # The same scenarios and examples/piv_inspect against a card on a PC/SC
  # reader. Nothing sets this option in CI. The tests skip unless
  # TC_PIV_CARD_READER names a reader (docs/testing.md).
  if(TINY_CRYPTO_TEST_PIV_CARD AND NOT WIN32)
    if(APPLE)
      find_library(TC_PCSC_LIBRARY PCSC REQUIRED)
      set(tc_pcsc_link ${TC_PCSC_LIBRARY})
    else()
      find_package(PkgConfig REQUIRED)
      pkg_check_modules(TC_PCSC REQUIRED IMPORTED_TARGET libpcsclite)
      set(tc_pcsc_link PkgConfig::TC_PCSC)
    endif()
    set(tc_piv_card_host_sources examples/credential_pcsc.c examples/credential_system.c
      tests/support/card_fixture.c)
    tc_add_c_test_executable(test_piv_card_hardware tiny_crypto_c-test-pki-native
      ${tc_piv_card_sources} tests/piv/hardware/backend_pcsc.c ${tc_piv_card_host_sources})
    target_compile_definitions(test_piv_card_hardware PRIVATE
      TC_CARD_FIXTURE_DIR="${PROJECT_SOURCE_DIR}/tests/vectors/piv/sm_captures/fixtures"
      TC_VECTOR_DIR="${PROJECT_SOURCE_DIR}/tests/vectors")
    target_link_libraries(test_piv_card_hardware PRIVATE ${tc_pcsc_link})
    add_library(test_piv_inspect_live_main OBJECT examples/piv_inspect_main.c)
    target_link_libraries(test_piv_inspect_live_main PRIVATE tiny_crypto_c-test-pki-native)
    target_compile_definitions(test_piv_inspect_live_main PRIVATE
      main=example_piv_inspect_main EXAMPLE_PIV_INSPECT_GUARD=1)
    tc_warnings(test_piv_inspect_live_main)
    tc_add_c_test_executable(test_piv_inspect_live tiny_crypto_c-test-pki-native
      tests/piv/hardware/inspect_live.c tests/piv/hardware/guard.c
      tests/piv/hardware/card_config.c examples/pki_input.c ${tc_piv_inspect_sources}
      ${tc_piv_card_host_sources} $<TARGET_OBJECTS:test_piv_inspect_live_main>)
    target_compile_definitions(test_piv_inspect_live PRIVATE
      TC_CARD_FIXTURE_DIR="${PROJECT_SOURCE_DIR}/tests/vectors/piv/sm_captures/fixtures"
      TC_VECTOR_DIR="${PROJECT_SOURCE_DIR}/tests/vectors")
    target_link_libraries(test_piv_inspect_live PRIVATE ${tc_pcsc_link})
    add_test(NAME test_piv_card_hardware COMMAND test_piv_card_hardware)
    add_test(NAME test_piv_inspect_live COMMAND test_piv_inspect_live)
    set_tests_properties(test_piv_card_hardware test_piv_inspect_live PROPERTIES
      LABELS hardware RUN_SERIAL TRUE SKIP_RETURN_CODE 77)
    set_tests_properties(test_piv_inspect_live PROPERTIES
      DEPENDS test_piv_card_hardware FAIL_REGULAR_EXPRESSION "  FAILED  ")
  endif()
  # The evaluation time the PIV hardware environment selects.
  if(NOT WIN32)
    tc_add_c_test(test_piv_hardware_config tiny_crypto_c-test-pki-native tests/piv/hardware_config.c
      tests/piv/hardware/card_config.c examples/pki_input.c)
    target_compile_definitions(test_piv_hardware_config PRIVATE
      TC_VECTOR_DIR="${PROJECT_SOURCE_DIR}/tests/vectors")
  endif()
  # Rules of the PIV hardware transmit guard. The guard needs no reader.
  tc_add_c_test(test_piv_hardware_guard tiny_crypto_c-test-piv-command tests/piv/hardware_guard.c
    tests/piv/hardware/guard.c)
  # Revocation evidence policy and Security Object digests over vendored PKIs.
  tc_add_c_test(test_credential_revocation_policy tiny_crypto_c-test-pki-native
    tests/credential/revocation_policy.c)
  target_compile_definitions(test_credential_revocation_policy PRIVATE
    TC_PKITS_DIR="${PROJECT_SOURCE_DIR}/tests/vectors/x509/nist/pkits")
  tc_add_c_test(test_credential_security_digest tiny_crypto_c-test-pki-native
    tests/credential/security_digest.c tests/support/card_fixture.c)
  target_compile_definitions(test_credential_security_digest PRIVATE
    TC_CARD_FIXTURE_DIR="${PROJECT_SOURCE_DIR}/tests/vectors/piv/sm_captures/fixtures"
    TC_VECTOR_DIR="${PROJECT_SOURCE_DIR}/tests/vectors")
  tc_add_c_test(test_x509_native_sizes tiny_crypto_c-test-pki-native tests/x509/native_sizes.c)
  tc_add_c_test(test_x509_ocsp_sd33 tiny_crypto_c-test-pki-native tests/x509/ocsp_sd33.c
    examples/x509_ocsp.c)
  target_compile_definitions(test_x509_ocsp_sd33 PRIVATE
    TC_SD33_OCSP_ROOT="${PROJECT_SOURCE_DIR}/tests/vectors/x509/ocsp/sd33"
    TC_SD33_CERT_ROOT="${PROJECT_SOURCE_DIR}/tests/vectors/x509/piv/sd33")
  tc_add_c_test(test_x509_ocsp_icam tiny_crypto_c-test-pki-native tests/x509/ocsp_icam.c)
  target_compile_definitions(test_x509_ocsp_icam PRIVATE
    TC_ICAM_OCSP_ROOT="${PROJECT_SOURCE_DIR}/tests/vectors/x509/ocsp/icam")
  if(tc_build_cpp_tests)
    tc_add_linked_test(test_cpp_credential tiny_crypto_c-test-pki-native
      tests/cpp/credential.cpp tests/cpp/main.cpp)
    target_include_directories(test_cpp_credential PRIVATE tests/support)
    tc_add_linked_test(test_cpp_trust_anchor tiny_crypto_c-test-pki-native
      tests/cpp/trust_anchor.cpp tests/cpp/main.cpp)
    target_include_directories(test_cpp_trust_anchor PRIVATE tests/support)
    target_compile_definitions(test_cpp_trust_anchor PRIVATE
      TC_TWIC_SYNTHETIC_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/twic/synthetic")
  endif()
  tc_add_c_test(test_source_hash tiny_crypto_c-test-pki-native tests/x509/source_hash.c
    tests/support/x509_crl_harness.c)
  tc_add_c_test(test_twic_synthetic_fixture tiny_crypto_c-test-pki-native
    tests/twic/synthetic_fixture.c)
  target_compile_definitions(test_twic_synthetic_fixture PRIVATE
    TC_TWIC_SYNTHETIC_ROOT="${PROJECT_SOURCE_DIR}/tests/vectors/twic/synthetic")
  tc_add_c_test(test_twic_synthetic_validation tiny_crypto_c-test-pki-native
    tests/twic/synthetic_validation.c)
  target_compile_definitions(test_twic_synthetic_validation PRIVATE
    TC_TWIC_SYNTHETIC_ROOT="${PROJECT_SOURCE_DIR}/tests/vectors/twic/synthetic")
  tc_add_c_test(test_x509_trust_anchor tiny_crypto_c-test-pki-native
    tests/x509/trust_anchor.c)
  target_compile_definitions(test_x509_trust_anchor PRIVATE
    TC_TWIC_SYNTHETIC_ROOT="${PROJECT_SOURCE_DIR}/tests/vectors/twic/synthetic")
  tc_add_c_test(test_piv_sm_authenticate tiny_crypto_c-test-pki-native tests/piv/sm_authenticate.c)
  tc_sm_fixture_header(test_piv_sm_authenticate)
  add_executable(test_piv_cvc_corpus_reader tests/piv/cvc_corpus.c tests/support/munit.c)
  target_include_directories(test_piv_cvc_corpus_reader PRIVATE tests/support)
  target_link_libraries(test_piv_cvc_corpus_reader PRIVATE tiny_crypto_c-test-pki-native)
  set_target_properties(test_piv_cvc_corpus_reader PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)
  tc_warnings(test_piv_cvc_corpus_reader)
  tc_use_test_sanitizers(test_piv_cvc_corpus_reader)
  tc_add_c_test(test_cms_corpus_reader tiny_crypto_c-test-pki-native tests/cms/corpus_reader.c
    tests/support/cms_crl_harness.c tests/support/x509_crl_harness.c)
  set_tests_properties(test_cms_corpus_reader PROPERTIES
    SKIP_REGULAR_EXPRESSION "No tests run, 2 .* skipped")
  tc_add_c_test(test_cms_octets tiny_crypto_c-test-pki tests/cms/octets.c)
  tc_add_c_test(test_cms_verify tiny_crypto_c-test-pki tests/cms/verify.c)
  tc_add_c_test(test_hash_algorithm tiny_crypto_c-test-pki tests/hash/algorithm.c)
  get_target_property(tc_cms_pki_sources tiny_crypto_c-test-pki SOURCES)
  tc_add_test_library(tiny_crypto_c-test-cms-crypto
    ${tc_cms_pki_sources} ${tc_hash_sources})
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
  tc_add_c_test(test_cms_signer_info tiny_crypto_c-test-pki tests/cms/signer.c examples/cms_reader.c
    tests/support/cms_crl_harness.c tests/support/x509_crl_harness.c)
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
      tests/twic/authentication.c examples/credential_validate.c
      examples/pki_input.c src/twic_ccl.c)
    target_compile_definitions(test_card_authentication PRIVATE TC_ENABLE_TWIC_CCL=1)
    target_link_libraries(test_card_authentication PRIVATE OpenSSL::Crypto)
    set_property(TARGET test_card_authentication PROPERTY NO_SYSTEM_FROM_IMPORTED TRUE)
    if(APPLE)
      find_package(ZLIB REQUIRED)
      add_library(twic_authenticate_command OBJECT examples/twic_authenticate.c)
      target_link_libraries(twic_authenticate_command PRIVATE tiny_crypto_c-test-pki-native)
      target_compile_definitions(twic_authenticate_command PRIVATE TC_ENABLE_TWIC_CCL=1
        main=example_twic_command_main setrlimit=example_twic_setrlimit
        mlock=example_twic_mlock munlock=example_twic_munlock example_read_file=example_twic_read_file
        example_read_created_file=example_twic_read_created_file
        example_twic_cancellation_check=example_twic_command_cancellation_check
        fopen=example_twic_fopen)
      tc_warnings(twic_authenticate_command)
      tc_use_test_sanitizers(twic_authenticate_command)
      tc_add_c_test(test_twic_authenticate_command tiny_crypto_c-test-pki-native
        tests/twic/authenticate_command.c $<TARGET_OBJECTS:twic_authenticate_command>
        examples/credential_validate.c examples/credential_object.c examples/cms_reader.c examples/cms_validate.c examples/pki_input.c
        examples/x509_revocation.c
        src/twic_ccl.c)
      target_compile_definitions(test_twic_authenticate_command PRIVATE TC_ENABLE_TWIC_CCL=1)
      target_link_libraries(test_twic_authenticate_command PRIVATE OpenSSL::Crypto ${ZLIB_LIBRARIES})
      target_include_directories(test_twic_authenticate_command SYSTEM PRIVATE ${ZLIB_INCLUDE_DIRS})
      set_property(TARGET test_twic_authenticate_command PROPERTY NO_SYSTEM_FROM_IMPORTED TRUE)
    endif()
    target_link_libraries(test_x509_native PRIVATE OpenSSL::Crypto)
    # The TWIC synthetic fixture builder must keep reproducing the checked-in
    # vectors byte for byte. The fixtures carry no CMS signingTime, and only
    # OpenSSL 3.2 and later can sign without one (CMS_NO_SIGNING_TIME).
    include(CheckSymbolExists)
    set(CMAKE_REQUIRED_INCLUDES ${OPENSSL_INCLUDE_DIR})
    check_symbol_exists(CMS_NO_SIGNING_TIME "openssl/cms.h" TC_OPENSSL_CMS_NO_SIGNING_TIME)
    unset(CMAKE_REQUIRED_INCLUDES)
    if(TC_OPENSSL_CMS_NO_SIGNING_TIME)
      tc_add_test_executable(twic_synthetic_fixture_builder
        tests/twic/generate_synthetic_fixture.c tests/support/munit.c)
      target_include_directories(twic_synthetic_fixture_builder PRIVATE tests/support src)
      target_link_libraries(twic_synthetic_fixture_builder PRIVATE OpenSSL::Crypto)
      set_property(TARGET twic_synthetic_fixture_builder PROPERTY NO_SYSTEM_FROM_IMPORTED TRUE)
      add_test(NAME test_twic_synthetic_fixture_builder COMMAND ${CMAKE_COMMAND}
        -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -DBUILDER=$<TARGET_FILE:twic_synthetic_fixture_builder>
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/twic-synthetic-fixture
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/twic_fixture_builder.cmake)
    else()
      message(STATUS "OpenSSL before 3.2 cannot sign CMS without signingTime; "
                     "test_twic_synthetic_fixture_builder is not built")
    endif()
    set_property(TARGET test_x509_native PROPERTY NO_SYSTEM_FROM_IMPORTED TRUE)
    tc_add_c_test(test_lds_native tiny_crypto_c-test-pki-native tests/cms/lds.c)
    # native.c covers SignedData, CHUID and security objects, path.c the
    # signer path builder and credential validation, revocation.c signed CRLs.
    foreach(cms_suite native path revocation)
      tc_add_c_test(test_cms_${cms_suite} tiny_crypto_c-test-pki-native tests/cms/${cms_suite}.c
        tests/support/cms_crl_harness.c tests/support/x509_crl_harness.c
        examples/cms_reader.c examples/cms_validate.c examples/credential_object.c
        examples/x509_revocation.c examples/credential_workflow.c
        src/twic_ccl.c)
      target_compile_definitions(test_cms_${cms_suite} PRIVATE TC_ENABLE_TWIC_CCL=1)
      target_link_libraries(test_cms_${cms_suite} PRIVATE OpenSSL::Crypto)
      set_property(TARGET test_cms_${cms_suite} PROPERTY NO_SYSTEM_FROM_IMPORTED TRUE)
    endforeach()
    add_executable(cms_check examples/cms_check.c examples/cms_reader.c
      examples/cms_validate.c examples/pki_input.c)
    target_link_libraries(cms_check PRIVATE tiny_crypto_c-test-pki-native)
    tc_warnings(cms_check)
    tc_use_test_sanitizers(cms_check)
    if(TINY_CRYPTO_TEST_EC_ORACLE)
      add_test(NAME test_cms_command COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/cms/command.py $<TARGET_FILE:cms_check>)
    endif()
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
        tc_add_c_test(${rsa_test} tiny_crypto_c-test tests/rsa/${rsa_scheme}_openssl.c ${tc_rsa_sources})
        if(rsa_scheme STREQUAL "oaep")
          target_sources(${rsa_test} PRIVATE examples/rsa_encrypt.c)
        endif()
        target_compile_definitions(${rsa_test} PRIVATE TC_ENABLE_RSA=1 TC_RSA_ENABLE_1024=1
          TC_RSA_SMALL=$<STREQUAL:${rsa_profile},small>)
        target_link_libraries(${rsa_test} PRIVATE OpenSSL::Crypto)
        set_property(TARGET ${rsa_test} PROPERTY NO_SYSTEM_FROM_IMPORTED TRUE)
      endforeach()
    endforeach()
    foreach(small 0 1)
      tc_add_c_test_executable(test_rsa_oaep_decrypt_${small} tiny_crypto_c-test
        tests/rsa/oaep_decrypt_openssl.c ${tc_rsa_sources})
      target_link_libraries(test_rsa_oaep_decrypt_${small} PRIVATE OpenSSL::Crypto)
      target_compile_definitions(test_rsa_oaep_decrypt_${small} PRIVATE TC_ENABLE_RSA=1 TC_RSA_ENABLE_1024=1 TC_RSA_SMALL=${small})
      set_property(TARGET test_rsa_oaep_decrypt_${small} PROPERTY NO_SYSTEM_FROM_IMPORTED TRUE)
      # The OpenSSL OAEP matrix runs as one CTest shard per modulus size.
      foreach(bits 1024 2048 3072 4096)
        add_test(NAME test_rsa_oaep_decrypt_${small}_${bits}
          COMMAND test_rsa_oaep_decrypt_${small} --param bits ${bits})
        list(APPEND tc_rsa_oaep_decrypt_shards test_rsa_oaep_decrypt_${small}_${bits})
      endforeach()
      tc_add_c_test(test_rsa_private_openssl_${small} tiny_crypto_c-test-rsa-${small}
        tests/rsa/private_openssl.c examples/rsa_validate.c examples/rsa_sign.c)
      target_link_libraries(test_rsa_private_openssl_${small} PRIVATE OpenSSL::Crypto)
      set_property(TARGET test_rsa_private_openssl_${small} PROPERTY NO_SYSTEM_FROM_IMPORTED TRUE)
      tc_add_c_test(test_rsa_raw_openssl_${small} tiny_crypto_c-test-rsa-${small}
        tests/rsa/raw_openssl.c)
      target_link_libraries(test_rsa_raw_openssl_${small} PRIVATE OpenSSL::Crypto)
      set_property(TARGET test_rsa_raw_openssl_${small} PROPERTY NO_SYSTEM_FROM_IMPORTED TRUE)
      tc_add_c_test(test_rsa_inverse_openssl_${small} tiny_crypto_c-test-rsa-${small} tests/rsa/inverse_openssl.c)
      tc_add_c_test(test_rsa_number_openssl_${small} tiny_crypto_c-test-rsa-${small} tests/rsa/number_openssl.c)
      target_link_libraries(test_rsa_number_openssl_${small} PRIVATE OpenSSL::Crypto)
      set_property(TARGET test_rsa_number_openssl_${small} PROPERTY NO_SYSTEM_FROM_IMPORTED TRUE)
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
    tc_add_c_test(test_x509_openssl tiny_crypto_c-test-pki tests/x509/openssl.c
      tests/support/x509_crl_harness.c)
    target_sources(test_x509_openssl PRIVATE examples/x509_client.c)
    target_link_libraries(test_x509_openssl PRIVATE OpenSSL::Crypto)
    set_property(TARGET test_x509_openssl PROPERTY NO_SYSTEM_FROM_IMPORTED TRUE)
    tc_add_c_test(test_x509_ocsp_openssl tiny_crypto_c-test-pki-native tests/x509/ocsp_openssl.c)
    target_link_libraries(test_x509_ocsp_openssl PRIVATE OpenSSL::Crypto)
    set_property(TARGET test_x509_ocsp_openssl PROPERTY NO_SYSTEM_FROM_IMPORTED TRUE)
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
  target_include_directories(test_x509_reader PRIVATE tests/support)
  if(Python3_Interpreter_FOUND)
    add_test(NAME test_x509_schema COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/x509/test.py --reader $<TARGET_FILE:test_x509_reader>)
  endif()
  tc_add_test_executable(test_piv_object_reader tests/piv/object_reader.c)
  target_link_libraries(test_piv_object_reader PRIVATE tiny_crypto_c-test-pki)
  if(TINY_CRYPTO_TEST_TLV_CORPUS AND Python3_Interpreter_FOUND)
    add_test(NAME test_gzip_corpus COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/gzip/corpus.py
      --reader $<TARGET_FILE:test_gzip_reader> --corpus ${TINY_CRYPTO_TEST_TLV_CORPUS}/piv)
    add_test(NAME test_piv_certificate_corpus COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/piv/certificate_corpus.py
      --reader $<TARGET_FILE:test_piv_certificate_corpus_reader> --corpus ${TINY_CRYPTO_TEST_TLV_CORPUS}/piv)
    if(EXISTS "${TINY_CRYPTO_TEST_TLV_CORPUS}/piv/vci_trust_anchors")
      add_test(NAME test_piv_cvc_corpus COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/piv/cvc_corpus.py
        --reader $<TARGET_FILE:test_piv_cvc_corpus_reader> --corpus ${TINY_CRYPTO_TEST_TLV_CORPUS}/piv)
    endif()
    if(EXISTS "${TINY_CRYPTO_TEST_TLV_CORPUS}/eac/cvc")
      add_test(NAME test_eac_corpus COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/eac/corpus.py
        --reader $<TARGET_FILE:test_eac_reader> --corpus ${TINY_CRYPTO_TEST_TLV_CORPUS}/eac/cvc)
    endif()
    add_test(NAME test_x509_corpus COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/x509/corpus.py
      --reader $<TARGET_FILE:test_x509_reader> --corpus ${TINY_CRYPTO_TEST_TLV_CORPUS}/x509)
    add_test(NAME test_x509_malformed COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/x509/synthetic.py
      --reader $<TARGET_FILE:test_x509_reader> --corpus ${TINY_CRYPTO_TEST_TLV_CORPUS}/x509/synthetic)
    add_test(NAME test_x509_crl_corpus COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/x509/crl_corpus.py
      --reader $<TARGET_FILE:test_x509_crl> --corpus ${TINY_CRYPTO_TEST_TLV_CORPUS}/x509/synthetic
      --reference-corpus ${TINY_CRYPTO_TEST_TLV_CORPUS}/x509)
    add_test(NAME test_x509_path_corpus COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/x509/path_corpus.py
      --reader $<TARGET_FILE:test_x509_path_corpus_reader> --corpus ${TINY_CRYPTO_TEST_TLV_CORPUS}/x509)
    add_test(NAME test_piv_corpus COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/piv/corpus.py
      --reader $<TARGET_FILE:test_piv_object_reader> --corpus ${TINY_CRYPTO_TEST_TLV_CORPUS}/piv)
    add_test(NAME test_cms_corpus COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/cms/corpus.py
      --reader $<TARGET_FILE:test_cms_corpus_reader> --corpus ${TINY_CRYPTO_TEST_TLV_CORPUS}/piv)
  endif()
  set(TINY_CRYPTO_TEST_TLV_MBEDTLS_SUITE "" CACHE FILEPATH "Optional external ASN.1 test data file")
  if(TINY_CRYPTO_TEST_TLV_MBEDTLS_SUITE AND Python3_Interpreter_FOUND)
    add_test(NAME test_tlv_external_lengths COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/tlv/corpus.py
      --reader $<TARGET_FILE:test_tlv_corpus_reader> --mbedtls-suite ${TINY_CRYPTO_TEST_TLV_MBEDTLS_SUITE})
  endif()
  if(TINY_CRYPTO_TEST_TLV_CORPUS AND Python3_Interpreter_FOUND)
    add_test(NAME test_tlv_corpus COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/tlv/corpus.py
      --reader $<TARGET_FILE:test_tlv_corpus_reader> --corpus ${TINY_CRYPTO_TEST_TLV_CORPUS})
  endif()
  if(tc_build_cpp_tests)
    foreach(small 0 1)
      tc_add_linked_test(test_cpp_rsa_${small} tiny_crypto_c-test-rsa-${small}
        tests/cpp/rsa.cpp tests/cpp/main.cpp)
      target_include_directories(test_cpp_rsa_${small} PRIVATE tests/support)
      if(TINY_CRYPTO_TEST_OPENSSL)
        tc_add_linked_test(test_cpp_rsa_openssl_${small} tiny_crypto_c-test
          tests/cpp/rsa_openssl.cpp tests/cpp/main.cpp ${tc_rsa_sources})
        target_compile_definitions(test_cpp_rsa_openssl_${small} PRIVATE TC_ENABLE_RSA=1 TC_RSA_ENABLE_1024=1 TC_RSA_SMALL=${small})
        target_include_directories(test_cpp_rsa_openssl_${small} PRIVATE tests/support)
        target_link_libraries(test_cpp_rsa_openssl_${small} PRIVATE OpenSSL::Crypto)
        set_property(TARGET test_cpp_rsa_openssl_${small} PROPERTY NO_SYSTEM_FROM_IMPORTED TRUE)
      endif()
    endforeach()
    foreach(pair "sskdf;sskdf" "aes_dynamic;aes-dynamic" "ec;ec-rfc6979" "piv_sm;piv-sm")
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
    tc_add_linked_test(test_cpp_apdu tiny_crypto_c-test-apdu tests/cpp/apdu.cpp tests/cpp/main.cpp)
    tc_add_linked_test(test_cpp_piv_command tiny_crypto_c-test-piv-command
      tests/cpp/piv_command.cpp tests/cpp/main.cpp)
    tc_add_linked_test(test_cpp_piv_catalog tiny_crypto_c-test-piv-command
      tests/cpp/piv_catalog.cpp tests/cpp/main.cpp)
    target_include_directories(test_cpp_piv_catalog PRIVATE tests/support)
    tc_add_linked_test(test_cpp_piv_key_proof tiny_crypto_c-test-pki-native
      tests/cpp/piv_key_proof.cpp ${tc_card_simulator_sources} tests/support/cavp.c
      tests/support/munit.c tests/cpp/main.cpp)
    target_include_directories(test_cpp_piv_key_proof PRIVATE tests/support)
    target_compile_definitions(test_cpp_piv_key_proof PRIVATE
      TC_CARD_FIXTURE_DIR="${PROJECT_SOURCE_DIR}/tests/vectors/piv/sm_captures/fixtures")
    tc_add_linked_test(test_cpp_piv_card_check tiny_crypto_c-test-pki-native
      tests/cpp/piv_card_check.cpp tests/cpp/main.cpp)
    target_include_directories(test_cpp_piv_card_check PRIVATE tests/support)
    tc_add_linked_test(test_cpp_piv_sm_apdu tiny_crypto_c-test-piv-sm
      tests/cpp/piv_sm_apdu.cpp tests/support/sm_card.c tests/support/sm_card_session.c
      tests/cpp/main.cpp)
    target_include_directories(test_cpp_piv_sm_apdu PRIVATE tests/support)
    tc_sm_fixture_header(test_cpp_piv_sm_apdu)
    target_include_directories(test_cpp_piv_command PRIVATE tests/support)
    tc_add_linked_test(test_cpp_piv_vci tiny_crypto_c-test-piv-vci
      tests/cpp/piv_vci.cpp tests/support/sm_card.c tests/support/sm_card_session.c
      tests/cpp/main.cpp)
    target_include_directories(test_cpp_piv_vci PRIVATE tests/support)
    tc_sm_fixture_header(test_cpp_piv_vci)
    target_include_directories(test_cpp_apdu PRIVATE tests/support)
    target_include_directories(test_cpp_tlv PRIVATE tests/support)
  endif()
  option(TINY_CRYPTO_BUILD_FUZZERS "Build libFuzzer targets (Clang only)" OFF)
  if(TINY_CRYPTO_BUILD_FUZZERS)
    if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
      message(FATAL_ERROR "Fuzz targets require Clang with libFuzzer")
    endif()
    function(tc_add_fuzzer target)
      add_executable(${target} ${ARGN})
      target_include_directories(${target} PRIVATE src)
      target_compile_options(${target} PRIVATE -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer)
      target_link_options(${target} PRIVATE -fsanitize=fuzzer,address,undefined)
      tc_warnings(${target})
    endfunction()
    # Replay tests/fuzz/<name> and any listed fixture directories once without
    # mutation. Add each crash reproducer to tests/fuzz/<name> after the fix.
    function(tc_add_fuzz_regression name)
      add_test(NAME test_fuzz_${name}_regression COMMAND fuzz_${name} -runs=0
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/fuzz/${name} ${ARGN})
    endfunction()
    set(tc_vectors ${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors)

    get_target_property(gzip_fuzz_sources tiny_crypto_c-test-gzip SOURCES)
    get_target_property(gzip_fuzz_definitions tiny_crypto_c-test-gzip COMPILE_DEFINITIONS)
    tc_add_fuzzer(fuzz_gzip tests/gzip/fuzz.c ${gzip_fuzz_sources})
    target_compile_definitions(fuzz_gzip PRIVATE ${gzip_fuzz_definitions})
    tc_add_fuzz_regression(gzip)

    tc_add_fuzzer(fuzz_tlv tests/tlv/fuzz.c src/tlv.c src/tlv_walk.c src/der.c)
    target_compile_definitions(fuzz_tlv PRIVATE TC_ENABLE_TLV=1 TC_ENABLE_DER=1
      TC_TLV_ENABLE_BER=1 TC_TLV_ENABLE_STREAM=1)
    tc_add_fuzz_regression(tlv ${tc_vectors}/x509/rfc)

    get_target_property(pki_fuzz_sources tiny_crypto_c-test-pki SOURCES)
    get_target_property(pki_fuzz_definitions tiny_crypto_c-test-pki COMPILE_DEFINITIONS)
    list(REMOVE_ITEM pki_fuzz_definitions TC_ENABLE_SHA256=0)
    tc_add_fuzzer(fuzz_pki tests/x509/fuzz.c tests/support/x509_crl_harness.c ${tc_hash_sources}
      ${pki_fuzz_sources} src/x509_trust_anchor.c)
    target_compile_definitions(fuzz_pki PRIVATE ${pki_fuzz_definitions}
      TC_ENABLE_SHA256=1 TC_ENABLE_TRUST_ANCHOR_FORMAT=1)
    tc_add_fuzz_regression(pki ${tc_vectors}/x509/rfc ${tc_vectors}/twic/synthetic/legacy
      ${tc_vectors}/twic/synthetic/nexgen)

    get_target_property(ocsp_fuzz_sources tiny_crypto_c-test-pki-native SOURCES)
    get_target_property(ocsp_fuzz_definitions tiny_crypto_c-test-pki-native COMPILE_DEFINITIONS)
    tc_add_fuzzer(fuzz_ocsp tests/x509/fuzz_ocsp.c ${ocsp_fuzz_sources})
    target_include_directories(fuzz_ocsp PRIVATE tests/support)
    target_compile_definitions(fuzz_ocsp PRIVATE ${ocsp_fuzz_definitions}
      TC_OCSP_FUZZ_ROOT="${tc_vectors}/x509/ocsp/icam")
    tc_add_fuzz_regression(ocsp ${tc_vectors}/x509/ocsp/icam ${tc_vectors}/x509/ocsp/local
      ${tc_vectors}/x509/ocsp/sd33)

    # The PIV card stack on arbitrary card answers. Secured links use the
    # tools/sm_fixtures.py handshakes and the card model in tests/support.
    get_target_property(piv_apdu_fuzz_sources tiny_crypto_c-test-pki-native SOURCES)
    get_target_property(piv_apdu_fuzz_definitions tiny_crypto_c-test-pki-native
      COMPILE_DEFINITIONS)
    tc_add_fuzzer(fuzz_piv_apdu tests/piv/apdu_fuzz.c tests/support/sm_card.c
      tests/support/sm_card_session.c ${piv_apdu_fuzz_sources})
    target_include_directories(fuzz_piv_apdu PRIVATE tests/support)
    target_compile_definitions(fuzz_piv_apdu PRIVATE ${piv_apdu_fuzz_definitions})
    tc_sm_fixture_header(fuzz_piv_apdu)
    # Coverage feedback from wiping loops and the cipher, hash and
    # big-number rounds finds no new paths and dominates the run time, so
    # those sources stay without it.
    set(piv_apdu_ignorelist ${CMAKE_CURRENT_BINARY_DIR}/fuzz_piv_apdu_ignorelist.txt)
    file(WRITE ${piv_apdu_ignorelist} "src:*/src/common.c\nsrc:*/src/aes*.c\n"
      "src:*/src/mac_core.c\nsrc:*/src/block_modes.c\nsrc:*/src/hash*.c\nsrc:*/src/sha512.c\n"
      "src:*/src/ec.c\nsrc:*/src/rsa_*.c\n")
    target_compile_options(fuzz_piv_apdu PRIVATE
      -fsanitize-coverage-ignorelist=${piv_apdu_ignorelist})
    tc_add_fuzz_regression(piv_apdu ${tc_vectors}/piv/sd33/card01)

    tc_add_fuzzer(fuzz_twic tests/twic/fuzz.c src/common.c src/tlv.c src/tlv_walk.c src/der.c
      src/credential_text_internal.c src/twic_ccl.c src/snapshot.c src/aamva.c src/twic_tpk.c)
    target_compile_definitions(fuzz_twic PRIVATE TC_ENABLE_TLV=1 TC_ENABLE_DER=1
      TC_ENABLE_TWIC_CCL=1 TC_ENABLE_AAMVA=1 TC_ENABLE_TWIC_TPK=1
      TC_ENABLE_AES=0 TC_ENABLE_SHA256=0)
    tc_add_fuzz_regression(twic ${tc_vectors}/twic/synthetic/legacy)
  endif()

  tc_add_c_test(test_kmac tiny_crypto_c-test tests/kmac/test.c)
  tc_add_c_test(test_kmac_acvp tiny_crypto_c-test tests/kmac/acvp.c)
  target_compile_definitions(test_kmac_acvp PRIVATE
    KMAC_ACVP_FILE="${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/kmac/acvp_kmac256_aft.tsv")
  # KMAC alone, without the SHA and AES cores of the full test library.
  tc_add_test_library(tiny_crypto_c-test-kmac-only src/common.c src/kmac.c)
  target_compile_definitions(tiny_crypto_c-test-kmac-only PUBLIC
    TC_ENABLE_KMAC256=1 TC_ENABLE_AES=0 TC_ENABLE_SHA256=0)
  tc_add_c_test(test_kmac_only tiny_crypto_c-test-kmac-only tests/kmac/test.c)
  if(TINY_CRYPTO_TEST_WYCHEPROOF_DIR)
    add_test(NAME test_wycheproof_kmac
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/wycheproof.py
        --vectors ${TINY_CRYPTO_TEST_WYCHEPROOF_DIR}
        --kmac-reader $<TARGET_FILE:test_kmac> --kmac-reader $<TARGET_FILE:test_kmac_only>)
    add_test(NAME test_wycheproof_dynamic_cmac
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/wycheproof.py
        --vectors ${TINY_CRYPTO_TEST_WYCHEPROOF_DIR}
        --cmac-reader $<TARGET_FILE:test_aes_dynamic>)
    add_test(NAME test_wycheproof_hmac
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/wycheproof.py
        --vectors ${TINY_CRYPTO_TEST_WYCHEPROOF_DIR}
        --hmac-reader $<TARGET_FILE:test_hash>)
  endif()

  tc_add_c_test(test_hash tiny_crypto_c-test
    tests/hash/test.c tests/hash/hmac_test.c tests/hash/cavp.c)
  target_include_directories(test_hash PRIVATE tests/hash)
  target_compile_definitions(test_hash PRIVATE
    CAVP_VECTOR_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/hash/cavp"
    HMAC_WYCHEPROOF_DIR="${tc_wycheproof_vectors}")

  function(tc_add_aes_test target library)
    tc_add_c_test(${target} ${library} tests/aes/test.c tests/aes/cavp.c
      tests/aes/eax_test.c tests/aes/siv_test.c tests/aes/cmac_test.c tests/aes/kw_test.c)
    target_include_directories(${target} PRIVATE tests/aes)
    target_compile_definitions(${target} PRIVATE
      CAVP_VECTOR_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/aes/cavp"
      EAX_VECTOR_FILE="${tc_wycheproof_vectors}/aes_eax_test.json"
      SIV_VECTOR_FILE="${tc_wycheproof_vectors}/aead_aes_siv_cmac_test.json"
      CMAC_WYCHEPROOF_FILE="${tc_wycheproof_vectors}/aes_cmac_test.json"
      CMAC_CAVP_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/aes/cmac"
      KW_CAVP_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/aes/kw")
  endfunction()

  tc_add_aes_test(test_aes tiny_crypto_c-test)
  tc_add_c_test(test_mac_core tiny_crypto_c-test tests/aes/mac_core.c)
  tc_add_aes_test(test_aes_192 tiny_crypto_c-test-aes192)
  tc_add_aes_test(test_aes_256 tiny_crypto_c-test-aes256)

  tc_add_c_test(test_des tiny_crypto_c-test
    tests/des/test.c tests/des/test_edge_vectors.c)
  target_include_directories(test_des PRIVATE tests/des)
  target_compile_definitions(test_des PRIVATE
    CAVP_VECTOR_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/des/cavp")

  if(TINY_CRYPTO_TEST_FULL)
    tc_add_test_library(tiny_crypto_c-test-des-cmac-cavp
      src/common.c ${tc_des_sources})
    target_compile_definitions(tiny_crypto_c-test-des-cmac-cavp PUBLIC
      TC_ENABLE_AES=0 TC_ENABLE_SHA256=0 TC_ENABLE_DES=1
      TC_DES_ENABLE_TDES=1 TC_DES_ENABLE_CMAC=1 TC_DES_REJECT_WEAK_KEYS=0)
    tc_add_c_test(test_des_cmac_cavp tiny_crypto_c-test-des-cmac-cavp
      tests/des/cmac_cavp.c)
    target_compile_definitions(test_des_cmac_cavp PRIVATE
      CMAC_TDES_CAVP_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/des/cmac")
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
  tc_add_c_test(test_hkdf tiny_crypto_c-test tests/kdf/hkdf_test.c)
  target_include_directories(test_hkdf PRIVATE tests/kdf)
  tc_add_linked_test(test_hkdf_example tiny_crypto_c-test examples/hkdf.c)
  tc_add_linked_test(test_aes_kw_example tiny_crypto_c-test examples/aes_kw.c)
  if(Python3_Interpreter_FOUND)
    tc_add_test_executable(test_hkdf_reader tests/kdf/hkdf_reader.c
      tests/support/cavp.c tests/support/munit.c)
    target_link_libraries(test_hkdf_reader PRIVATE tiny_crypto_c-test)
    target_include_directories(test_hkdf_reader PRIVATE tests/support)
    add_test(NAME test_hkdf_wycheproof
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/kdf/hkdf_wycheproof.py
        $<TARGET_FILE:test_hkdf_reader>
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/wycheproof/testvectors_v1)
    set_tests_properties(test_hkdf_wycheproof PROPERTIES LABELS extended)
    add_test(NAME test_hkdf_acvp
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/kdf/hkdf_acvp.py
        $<TARGET_FILE:test_hkdf_reader>
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors/kdf/acvp_hkdf)
    set_tests_properties(test_hkdf_acvp PROPERTIES LABELS extended)
  endif()

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
    tc_add_linked_test(test_cpp_hkdf tiny_crypto_c-test
      tests/cpp/hkdf.cpp tests/cpp/main.cpp)
    target_include_directories(test_cpp_hkdf PRIVATE tests/support)
    tc_add_linked_test(test_cpp_aes_kw tiny_crypto_c-test
      tests/cpp/aes_kw.cpp tests/cpp/main.cpp)
    target_include_directories(test_cpp_aes_kw PRIVATE tests/support)
    tc_add_linked_test(test_cpp_drbg tiny_crypto_c-test-drbg
      tests/cpp/drbg.cpp tests/cpp/main.cpp)
    target_include_directories(test_cpp_drbg PRIVATE tests/support)

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
  foreach(header_profile rsa tlv apdu piv_command aamva fascn twic_uuid twic_tpk twic_object hkdf aes_kw
      piv_oids x509 key_challenge x509_path x509_revocation x509_ocsp cms cms_validation piv_objects credential piv_cvc piv_chuid
      piv_sm piv_sm_apdu piv_vci piv_catalog piv_key_proof piv_card_check twic_ccl)
    set(header_profile_definitions TC_ENABLE_AES=0 TC_ENABLE_SHA256=0)
    if(header_profile STREQUAL "hkdf")
      list(REMOVE_ITEM header_profile_definitions TC_ENABLE_SHA256=0)
      list(APPEND header_profile_definitions
        TC_ENABLE_SHA256=1 TC_ENABLE_HMAC=1 TC_ENABLE_HKDF=1 TC_ENABLE_KDF=0
        TC_TEST_HEADER_HKDF=1)
    elseif(header_profile STREQUAL "aes_kw")
      list(REMOVE_ITEM header_profile_definitions TC_ENABLE_AES=0)
      list(APPEND header_profile_definitions
        TC_ENABLE_AES=1 TC_AES_ENABLE_CTR=0 TC_AES_ENABLE_KW=1 TC_TEST_HEADER_AES_KW=1)
    elseif(header_profile STREQUAL "rsa")
      list(APPEND header_profile_definitions TC_ENABLE_RSA=1 TC_RSA_ENABLE_1024=1 TC_TEST_HEADER_RSA=1)
    elseif(header_profile STREQUAL "tlv")
      list(APPEND header_profile_definitions TC_ENABLE_TLV=1 TC_TEST_HEADER_TLV=1)
    elseif(header_profile STREQUAL "apdu")
      list(APPEND header_profile_definitions TC_ENABLE_APDU=1 TC_TEST_HEADER_APDU=1)
    elseif(header_profile STREQUAL "piv_command")
      list(APPEND header_profile_definitions TC_ENABLE_APDU=1 TC_ENABLE_TLV=1
        TC_ENABLE_PIV_COMMAND=1 TC_TEST_HEADER_PIV_COMMAND=1)
    elseif(header_profile STREQUAL "piv_catalog")
      list(APPEND header_profile_definitions TC_ENABLE_APDU=1 TC_ENABLE_TLV=1
        TC_ENABLE_PIV_COMMAND=1 TC_ENABLE_PIV_CATALOG=1 TC_TEST_HEADER_PIV_CATALOG=1)
    elseif(header_profile STREQUAL "piv_key_proof")
      list(APPEND header_profile_definitions TC_ENABLE_APDU=1 TC_ENABLE_TLV=1 TC_ENABLE_DER=1
        TC_ENABLE_X509=1 TC_ENABLE_KEY_CHALLENGE=1 TC_ENABLE_PIV_COMMAND=1
        TC_ENABLE_PIV_KEY_PROOF=1 TC_TEST_HEADER_PIV_KEY_PROOF=1)
    elseif(header_profile STREQUAL "piv_card_check")
      list(APPEND header_profile_definitions TC_ENABLE_APDU=1 TC_ENABLE_TLV=1 TC_ENABLE_DER=1
        TC_TLV_ENABLE_BER=1 TC_ENABLE_X509=1 TC_ENABLE_PIV_OIDS=1 TC_ENABLE_X509_PATH=1
        TC_ENABLE_X509_REVOCATION=1 TC_ENABLE_CMS=1 TC_ENABLE_CMS_VALIDATION=1 TC_ENABLE_FASCN=1
        TC_ENABLE_TWIC_UUID=1 TC_ENABLE_PIV_OBJECTS=1 TC_ENABLE_PIV_CHUID=1 TC_ENABLE_CREDENTIAL=1
        TC_ENABLE_PIV_COMMAND=1 TC_ENABLE_PIV_CATALOG=1 TC_ENABLE_GZIP=1
        TC_ENABLE_PIV_CARD_CHECK=1 TC_TEST_HEADER_PIV_CARD_CHECK=1)
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
    elseif(header_profile STREQUAL "x509_ocsp")
      list(REMOVE_ITEM header_profile_definitions TC_ENABLE_SHA256=0)
      list(APPEND header_profile_definitions
        TC_ENABLE_TLV=1 TC_ENABLE_DER=1 TC_ENABLE_X509=1 TC_ENABLE_X509_PATH=1
        TC_ENABLE_SHA1=1 TC_ENABLE_SHA256=1 TC_ENABLE_X509_OCSP=1 TC_TEST_HEADER_X509_OCSP=1)
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
    elseif(header_profile STREQUAL "piv_sm_apdu")
      list(REMOVE_ITEM header_profile_definitions TC_ENABLE_AES=0 TC_ENABLE_SHA256=0)
      list(APPEND header_profile_definitions
        TC_ENABLE_AES=1 TC_AES_ENABLE_DYNAMIC=1 TC_ENABLE_SHA256=1
        TC_ENABLE_SSKDF=1 TC_ENABLE_EC=1 TC_EC_ENABLE_P256=1
        TC_ENABLE_PIV_SM=1 TC_PIV_SM_ENABLE_CS2=1 TC_PIV_SM_ENABLE_CS7=0
        TC_ENABLE_TLV=1 TC_ENABLE_DER=1 TC_ENABLE_PIV_CVC=1 TC_ENABLE_APDU=1
        TC_ENABLE_PIV_COMMAND=1 TC_ENABLE_PIV_SM_APDU=1 TC_TEST_HEADER_PIV_SM_APDU=1)
    elseif(header_profile STREQUAL "piv_vci")
      list(REMOVE_ITEM header_profile_definitions TC_ENABLE_AES=0 TC_ENABLE_SHA256=0)
      list(APPEND header_profile_definitions
        TC_ENABLE_AES=1 TC_AES_ENABLE_DYNAMIC=1 TC_ENABLE_SHA256=1
        TC_ENABLE_SSKDF=1 TC_ENABLE_EC=1 TC_EC_ENABLE_P256=1
        TC_ENABLE_PIV_SM=1 TC_PIV_SM_ENABLE_CS2=1 TC_PIV_SM_ENABLE_CS7=0
        TC_ENABLE_TLV=1 TC_ENABLE_DER=1 TC_ENABLE_PIV_CVC=1 TC_ENABLE_APDU=1
        TC_ENABLE_PIV_COMMAND=1 TC_ENABLE_PIV_SM_APDU=1 TC_TLV_ENABLE_BER=1 TC_ENABLE_X509=1
        TC_ENABLE_PIV_OIDS=1 TC_ENABLE_CMS=1 TC_ENABLE_FASCN=1 TC_ENABLE_TWIC_UUID=1
        TC_ENABLE_PIV_OBJECTS=1 TC_ENABLE_PIV_VCI=1 TC_TEST_HEADER_PIV_VCI=1)
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

  # Include feature headers directly with their features off. The probes fail
  # to compile when a header declares an API that the profile does not build.
  set(tc_feature_off_kdf
    TC_ENABLE_SHA256=1 TC_ENABLE_SHA384=1 TC_ENABLE_HMAC=1 TC_ENABLE_AES=1
    TC_AES_ENABLE_CMAC=1 TC_ENABLE_KDF=0 TC_ENABLE_SSKDF=0 TC_ENABLE_MD5=0)
  set(tc_feature_off_no_sha
    TC_ENABLE_SHA1=0 TC_ENABLE_SHA224=0 TC_ENABLE_SHA256=0 TC_ENABLE_SHA384=0
    TC_ENABLE_SHA512=0 TC_ENABLE_HMAC=0 TC_ENABLE_KDF=0 TC_ENABLE_SSKDF=0
    TC_ENABLE_MD5=0 TC_TEST_HEADER_NO_SHA=1)
  foreach(feature_profile kdf no_sha)
    foreach(language c cpp)
      set(feature_target test_${language}_feature_off_${feature_profile})
      add_library(${feature_target} OBJECT tests/headers/feature_off.${language})
      target_include_directories(${feature_target} PRIVATE src)
      target_compile_definitions(${feature_target} PRIVATE
        ${tc_feature_off_${feature_profile}})
      tc_warnings(${feature_target})
    endforeach()
    set_property(TARGET test_cpp_feature_off_${feature_profile} PROPERTY CXX_STANDARD 11)
  endforeach()

  # header_compile.cpp instantiates every C++ wrapper. Both the C++17 object,
  # where TC_CPP_NODISCARD is [[nodiscard]], and the AVR object enable every
  # wrapper family and treat warnings as errors.
  set(tc_cpp_header_definitions ${tc_full_definitions} TC_AES_KEY_BITS=128
    TC_AES_ENABLE_EAX_PRIME=1 TC_AES_ENABLE_DYNAMIC=1 TC_ENABLE_MD5=1 TC_ENABLE_GZIP=1
    TC_ENABLE_DRBG=1 TC_DRBG_ENABLE_HMAC=1 TC_ENABLE_RSA=1 TC_RSA_ENABLE_1024=1 TC_ENABLE_TLV=1 TC_ENABLE_DER=1 TC_ENABLE_X509=1
    TC_ENABLE_PIV_CHUID=1 TC_ENABLE_PIV_CVC=1 TC_ENABLE_EAC_CVC=1 TC_ENABLE_PIV_SM=1
    TC_ENABLE_EC=1 TC_ENABLE_SSKDF=1 TC_ENABLE_APDU=1 TC_ENABLE_PIV_COMMAND=1
    TC_ENABLE_PIV_SM_APDU=1 TC_TLV_ENABLE_BER=1 TC_ENABLE_PIV_OIDS=1 TC_ENABLE_CMS=1
    TC_ENABLE_FASCN=1 TC_ENABLE_TWIC_UUID=1 TC_ENABLE_PIV_OBJECTS=1 TC_ENABLE_PIV_VCI=1
    TC_ENABLE_PIV_CATALOG=1 TC_ENABLE_KEY_CHALLENGE=1 TC_ENABLE_PIV_KEY_PROOF=1)
  add_library(test_cpp_headers_cxx17 OBJECT tests/cpp/header_compile.cpp)
  target_include_directories(test_cpp_headers_cxx17 PRIVATE src)
  set_property(TARGET test_cpp_headers_cxx17 PROPERTY CXX_STANDARD 17)
  target_compile_definitions(test_cpp_headers_cxx17 PRIVATE ${tc_cpp_header_definitions})
  tc_warnings(test_cpp_headers_cxx17)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_cpp_headers_cxx17 PRIVATE -Werror)
  endif()

  # const_descriptors.c passes const workspace descriptors to every public
  # entry that takes one. The C object needs -Werror for discarded qualifiers.
  foreach(language c cpp)
    add_library(test_${language}_const_descriptors OBJECT tests/headers/const_descriptors.${language})
    target_include_directories(test_${language}_const_descriptors PRIVATE src)
    # The probe calls the path, revocation, trust-anchor, CMS validation,
    # credential and OCSP entries, so their features are on.
    target_compile_definitions(test_${language}_const_descriptors PRIVATE ${tc_cpp_header_definitions}
      TC_ENABLE_X509_PATH=1 TC_ENABLE_X509_REVOCATION=1 TC_ENABLE_TRUST_ANCHOR_FORMAT=1
      TC_ENABLE_CMS_VALIDATION=1 TC_ENABLE_CREDENTIAL=1 TC_ENABLE_X509_OCSP=1)
    tc_warnings(test_${language}_const_descriptors)
    if(language STREQUAL "c")
      set(compiler_id ${CMAKE_C_COMPILER_ID})
    else()
      set(compiler_id ${CMAKE_CXX_COMPILER_ID})
    endif()
    if(compiler_id MATCHES "GNU|Clang")
      target_compile_options(test_${language}_const_descriptors PRIVATE -Werror)
    elseif(MSVC)
      target_compile_options(test_${language}_const_descriptors PRIVATE /WX)
    endif()
  endforeach()
  set_property(TARGET test_cpp_const_descriptors PROPERTY CXX_STANDARD 11)

  # copy_contract.cpp asserts the documented copy and move rules of every
  # wrapper class in C++11. avr-g++ has no <type_traits>, so it runs on the host.
  add_library(test_cpp_copy_contract OBJECT tests/cpp/copy_contract.cpp)
  target_include_directories(test_cpp_copy_contract PRIVATE src)
  set_property(TARGET test_cpp_copy_contract PROPERTY CXX_STANDARD 11)
  target_compile_definitions(test_cpp_copy_contract PRIVATE ${tc_cpp_header_definitions})
  tc_warnings(test_cpp_copy_contract)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_cpp_copy_contract PRIVATE -Werror)
  endif()

  # Discarding a wrapper status must draw a compiler warning.
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_test(NAME test_cpp_nodiscard COMMAND ${CMAKE_COMMAND}
      -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR} -DCXX_COMPILER=${CMAKE_CXX_COMPILER}
      -DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/cpp_nodiscard.cmake)
  endif()
  # A C build that includes a C++ header must stop at the header's guard.
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_test(NAME test_cpp_headers_reject_c COMMAND ${CMAKE_COMMAND}
      -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR} -DC_COMPILER=${CMAKE_C_COMPILER}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/cpp_headers_reject_c.cmake)
  endif()

  # RSA, EC and key-challenge work budgets keep exact 32-bit types.
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_test(NAME test_work_budget_types COMMAND ${CMAKE_COMMAND}
      -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR} -DC_COMPILER=${CMAKE_C_COMPILER}
      -DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/work_budget
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/work_budget_width.cmake)
  endif()

  find_program(TC_AVR_CXX NAMES avr-g++)
  find_program(TC_AVR_CC NAMES avr-gcc)
  if(TC_AVR_CC)
    # size_t is 16 bits on AVR. Width-changing conversions are errors in the
    # RSA, EC and key-challenge compile checks, so copying a 32-bit budget
    # into a size_t fails. Sign conversions keep the width and stay allowed.
    set(tc_avr_work_width_flags -Wconversion -Wno-sign-conversion)
    add_test(NAME test_work_budget_types_avr COMMAND ${CMAKE_COMMAND}
      -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR} -DC_COMPILER=${TC_AVR_CC}
      -DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/work_budget_avr
      "-DFLAGS=-Os;-mmcu=atmega2560;${tc_avr_work_width_flags}" -DNARROWING=ON
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/work_budget_width.cmake)
    foreach(rsa_source ${tc_rsa_sources} examples/rsa_validate.c examples/rsa_sign.c examples/rsa_encrypt.c)
      get_filename_component(rsa_name ${rsa_source} NAME_WE)
      add_test(NAME test_${rsa_name}_compile_avr
        COMMAND ${TC_AVR_CC} -std=c99 -Wall -Wextra -Werror ${tc_avr_work_width_flags} -Os
          -mmcu=atmega2560 -DTC_ENABLE_RSA=1 -DTC_RSA_ENABLE_1024=1
          -I${CMAKE_CURRENT_SOURCE_DIR}/src
          -c ${CMAKE_CURRENT_SOURCE_DIR}/${rsa_source}
          -o ${CMAKE_CURRENT_BINARY_DIR}/tiny_crypto_c-${rsa_name}-compile.o)
    endforeach()
    # The secure messaging framing sources build with the session and the PIV
    # card commands for a 16-bit size_t.
    foreach(sm_source piv_sm piv_sm_message piv_sm_apdu piv_sm_key_request)
      add_test(NAME test_${sm_source}_compile_avr
        COMMAND ${TC_AVR_CC} -std=c99 -Wall -Wextra -Werror -Os -mmcu=atmega328p
          -DTC_RESOURCE_PROFILE=1 -DTC_ENABLE_PIV_SM=1 -DTC_ENABLE_EC=1
          -DTC_ENABLE_SSKDF=1 -DTC_ENABLE_SHA384=1 -DTC_AES_ENABLE_DYNAMIC=1
          -DTC_ENABLE_TLV=1 -DTC_ENABLE_DER=1 -DTC_ENABLE_PIV_CVC=1
          -DTC_ENABLE_APDU=1 -DTC_ENABLE_PIV_COMMAND=1 -DTC_ENABLE_PIV_SM_APDU=1
          -I${CMAKE_CURRENT_SOURCE_DIR}/src
          -c ${CMAKE_CURRENT_SOURCE_DIR}/src/${sm_source}.c
          -o ${CMAKE_CURRENT_BINARY_DIR}/tiny_crypto_c-${sm_source}-compile.o)
    endforeach()
    foreach(vci_source piv_discovery_get piv_vci)
      add_test(NAME test_${vci_source}_compile_avr
        COMMAND ${TC_AVR_CC} -std=c99 -Wall -Wextra -Werror -Os -mmcu=atmega328p
          -DTC_RESOURCE_PROFILE=1 -DTC_ENABLE_PIV_SM=1 -DTC_ENABLE_EC=1
          -DTC_ENABLE_SSKDF=1 -DTC_ENABLE_SHA384=1 -DTC_AES_ENABLE_DYNAMIC=1
          -DTC_ENABLE_TLV=1 -DTC_ENABLE_DER=1 -DTC_ENABLE_PIV_CVC=1
          -DTC_ENABLE_APDU=1 -DTC_ENABLE_PIV_COMMAND=1 -DTC_ENABLE_PIV_SM_APDU=1
          -DTC_TLV_ENABLE_BER=1 -DTC_ENABLE_X509=1 -DTC_ENABLE_PIV_OIDS=1 -DTC_ENABLE_CMS=1
          -DTC_ENABLE_FASCN=1 -DTC_ENABLE_TWIC_UUID=1 -DTC_ENABLE_PIV_OBJECTS=1
          -DTC_ENABLE_PIV_VCI=1 -I${CMAKE_CURRENT_SOURCE_DIR}/src
          -c ${CMAKE_CURRENT_SOURCE_DIR}/src/${vci_source}.c
          -o ${CMAKE_CURRENT_BINARY_DIR}/tiny_crypto_c-${vci_source}-compile.o)
    endforeach()
    # Hash descriptors live in flash on AVR; build every hash source with all
    # digests and HMAC enabled so program-memory access stays covered.
    foreach(hash_source hash_core hash sha512 md5)
      add_test(NAME test_${hash_source}_compile_avr
        COMMAND ${TC_AVR_CC} -std=c99 -Wall -Wextra -Werror -Os -mmcu=atmega2560
          -DTC_ENABLE_HMAC=1 -DTC_ENABLE_MD5=1 -DTC_ENABLE_SHA1=1 -DTC_ENABLE_SHA224=1
          -DTC_ENABLE_SHA384=1 -DTC_ENABLE_SHA512=1 -I${CMAKE_CURRENT_SOURCE_DIR}/src
          -c ${CMAKE_CURRENT_SOURCE_DIR}/src/${hash_source}.c
          -o ${CMAKE_CURRENT_BINARY_DIR}/tiny_crypto_c-${hash_source}-compile.o)
    endforeach()
    add_test(NAME test_ec_compile_avr
      COMMAND ${TC_AVR_CC} -std=c99 -Wall -Wextra -Werror ${tc_avr_work_width_flags} -Os
        -mmcu=atmega328p -DTC_ENABLE_EC=1 -I${CMAKE_CURRENT_SOURCE_DIR}/src
        -c ${CMAKE_CURRENT_SOURCE_DIR}/src/ec.c
        -o ${CMAKE_CURRENT_BINARY_DIR}/tiny_crypto_c-ec-compile.o)
    # CTR_DRBG needs dynamic AES; every hash is on so HMAC and Hash_DRBG
    # compile with both seed lengths.
    foreach(drbg_source drbg drbg_hash drbg_hmac drbg_ctr drbg_random)
      add_test(NAME test_${drbg_source}_compile_avr
        COMMAND ${TC_AVR_CC} -std=c99 -Wall -Wextra -Werror -Os -mmcu=atmega2560
          -DTC_ENABLE_DRBG=1 -DTC_DRBG_ENABLE_HASH=1 -DTC_DRBG_ENABLE_HMAC=1
          -DTC_DRBG_ENABLE_CTR=1 -DTC_ENABLE_HMAC=1 -DTC_AES_ENABLE_DYNAMIC=1
          -DTC_ENABLE_SHA1=1 -DTC_ENABLE_SHA384=1 -DTC_ENABLE_SHA512=1
          -I${CMAKE_CURRENT_SOURCE_DIR}/src
          -c ${CMAKE_CURRENT_SOURCE_DIR}/src/${drbg_source}.c
          -o ${CMAKE_CURRENT_BINARY_DIR}/tiny_crypto_c-${drbg_source}-compile.o)
    endforeach()
    # Run AES on an emulated ATmega328P in each S-box mode. Flash-resident
    # tables and the runtime SRAM table use different read paths on AVR.
    find_program(TC_QEMU_AVR NAMES qemu-system-avr)
    if(TC_QEMU_AVR AND Python3_Interpreter_FOUND)
      foreach(sbox_mode 1 2 3)
        add_test(NAME test_aes_sbox_${sbox_mode}_qemu_avr
          COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/avr/run_qemu.py
            --cc ${TC_AVR_CC} --qemu ${TC_QEMU_AVR} --expect AES-OK
            --include ${CMAKE_CURRENT_SOURCE_DIR}/src
            --define TC_AES_SBOX_MODE=${sbox_mode} --define TC_AES_ENABLE_ECB=1
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/avr/aes_known_answer.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/aes.c ${CMAKE_CURRENT_SOURCE_DIR}/src/aes_modes.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/block_modes.c ${CMAKE_CURRENT_SOURCE_DIR}/src/common.c)
        add_test(NAME test_aes_kw_sbox_${sbox_mode}_qemu_avr
          COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/avr/run_qemu.py
            --cc ${TC_AVR_CC} --qemu ${TC_QEMU_AVR} --expect KW-OK --timeout 120
            --include ${CMAKE_CURRENT_SOURCE_DIR}/src
            --define TC_AES_SBOX_MODE=${sbox_mode} --define TC_AES_ENABLE_KW=1
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/avr/aes_kw_known_answer.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/aes.c ${CMAKE_CURRENT_SOURCE_DIR}/src/aes_kw.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/common.c)
      endforeach()
      # APDU lengths with a 16-bit size_t and a scripted plain PIV read, built
      # from the apdu_piv_read profile sources.
      set(tc_avr_apdu_piv_read_sources
        apdu_encode apdu_response apdu_channel piv_aid piv_link piv_select piv_get_data
        piv_verify piv_status piv_template_internal piv_container_internal tlv tlv_walk
        tlv_write common)
      list(TRANSFORM tc_avr_apdu_piv_read_sources
        REPLACE "(.+)" "${CMAKE_CURRENT_SOURCE_DIR}/src/\\1.c")
      add_test(NAME test_apdu_piv_read_qemu_avr
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/avr/run_qemu.py
          --cc ${TC_AVR_CC} --qemu ${TC_QEMU_AVR} --expect APDU-OK
          --include ${CMAKE_CURRENT_SOURCE_DIR}/src
          --define TC_ENABLE_APDU=1 --define TC_ENABLE_TLV=1 --define TC_ENABLE_PIV_COMMAND=1
          ${CMAKE_CURRENT_SOURCE_DIR}/tests/avr/apdu_piv_read.c ${tc_avr_apdu_piv_read_sources})
    endif()
    # The key challenge carries a 32-bit work budget across size_t PKI code.
    add_test(NAME test_key_challenge_compile_avr
      COMMAND ${TC_AVR_CC} -std=c99 -Wall -Wextra -Werror ${tc_avr_work_width_flags} -Os
        -mmcu=atmega2560 -DTC_ENABLE_KEY_CHALLENGE=1 -DTC_ENABLE_X509=1 -DTC_ENABLE_TLV=1
        -DTC_ENABLE_DER=1 -DTC_ENABLE_RSA=1 -DTC_RSA_ENABLE_1024=1 -DTC_ENABLE_EC=1
        -I${CMAKE_CURRENT_SOURCE_DIR}/src
        -c ${CMAKE_CURRENT_SOURCE_DIR}/src/key_challenge.c
        -o ${CMAKE_CURRENT_BINARY_DIR}/tiny_crypto_c-key_challenge-compile.o)
    # TC_APDU_command.ne holds 65536 with a 16-bit size_t.
    add_test(NAME test_apdu_compile_avr
      COMMAND ${TC_AVR_CC} -std=c99 -Wall -Wextra -Werror -Os -mmcu=atmega2560
        -DTC_ENABLE_APDU=1 -I${CMAKE_CURRENT_SOURCE_DIR}/src
        -c ${CMAKE_CURRENT_SOURCE_DIR}/src/apdu_encode.c
        -o ${CMAKE_CURRENT_BINARY_DIR}/tiny_crypto_c-apdu_encode-compile.o)
    add_test(NAME test_apdu_channel_compile_avr
      COMMAND ${TC_AVR_CC} -std=c99 -Wall -Wextra -Werror -Os -mmcu=atmega2560
        -DTC_ENABLE_APDU=1 -I${CMAKE_CURRENT_SOURCE_DIR}/src
        -c ${CMAKE_CURRENT_SOURCE_DIR}/src/apdu_channel.c
        -o ${CMAKE_CURRENT_BINARY_DIR}/tiny_crypto_c-apdu_channel-compile.o)
    # The PIV card commands compile for AVR with a 16-bit size_t.
    set(tc_avr_piv_command_dir ${CMAKE_CURRENT_BINARY_DIR}/avr-piv-command)
    file(MAKE_DIRECTORY ${tc_avr_piv_command_dir})
    add_test(NAME test_piv_command_compile_avr
      COMMAND ${TC_AVR_CC} -std=c99 -Wall -Wextra -Werror -Os -mmcu=atmega2560
        -DTC_ENABLE_APDU=1 -DTC_ENABLE_TLV=1 -DTC_ENABLE_PIV_COMMAND=1
        -I${CMAKE_CURRENT_SOURCE_DIR}/src
        -c ${CMAKE_CURRENT_SOURCE_DIR}/src/piv_aid.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/piv_link.c ${CMAKE_CURRENT_SOURCE_DIR}/src/piv_select.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/piv_get_data.c ${CMAKE_CURRENT_SOURCE_DIR}/src/piv_verify.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/piv_status.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/piv_template_internal.c
      WORKING_DIRECTORY ${tc_avr_piv_command_dir})
    # The catalog tables and the inventory need only the card commands.
    add_test(NAME test_piv_catalog_compile_avr
      COMMAND ${TC_AVR_CC} -std=c99 -Wall -Wextra -Werror -Os -mmcu=atmega2560
        -DTC_ENABLE_APDU=1 -DTC_ENABLE_TLV=1 -DTC_ENABLE_PIV_COMMAND=1
        -DTC_ENABLE_PIV_CATALOG=1 -I${CMAKE_CURRENT_SOURCE_DIR}/src
        -c ${CMAKE_CURRENT_SOURCE_DIR}/src/piv_catalog.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/piv_inventory.c
      WORKING_DIRECTORY ${tc_avr_piv_command_dir})
    # The key policy and the key proof need the card commands, key
    # challenges and the X.509 reader.
    add_test(NAME test_piv_key_proof_compile_avr
      COMMAND ${TC_AVR_CC} -std=c99 -Wall -Wextra -Werror -Os -mmcu=atmega2560
        -DTC_ENABLE_APDU=1 -DTC_ENABLE_TLV=1 -DTC_ENABLE_DER=1 -DTC_ENABLE_X509=1
        -DTC_ENABLE_KEY_CHALLENGE=1 -DTC_ENABLE_PIV_COMMAND=1 -DTC_ENABLE_PIV_KEY_PROOF=1
        -I${CMAKE_CURRENT_SOURCE_DIR}/src
        -c ${CMAKE_CURRENT_SOURCE_DIR}/src/piv_key_policy.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/piv_key_proof.c
      WORKING_DIRECTORY ${tc_avr_piv_command_dir})
    add_test(NAME test_sskdf_compile_avr
      COMMAND ${TC_AVR_CC} -std=c99 -Wall -Wextra -Werror -mmcu=atmega328p
        -DTC_ENABLE_SSKDF=1 -DTC_ENABLE_SHA384=1 -I${CMAKE_CURRENT_SOURCE_DIR}/src
        -c ${CMAKE_CURRENT_SOURCE_DIR}/src/sskdf.c
        -o ${CMAKE_CURRENT_BINARY_DIR}/tiny_crypto_c-sskdf-compile.o)
  endif()
  if(TC_AVR_CXX)
    list(TRANSFORM tc_cpp_header_definitions PREPEND "-D" OUTPUT_VARIABLE tc_avr_cpp_flags)
    add_test(NAME test_cpp_headers_avr
      COMMAND ${TC_AVR_CXX} -std=gnu++11 -fno-exceptions -fno-rtti -Wall -Wextra -Werror -Os
        ${tc_avr_cpp_flags} -mmcu=atmega2560 -I${CMAKE_CURRENT_SOURCE_DIR}/src
        -c ${CMAKE_CURRENT_SOURCE_DIR}/tests/cpp/header_compile.cpp
        -o ${CMAKE_CURRENT_BINARY_DIR}/tiny_crypto_c-header-compile.o)
    # avr-g++ must also diagnose every discarded wrapper result.
    add_test(NAME test_cpp_nodiscard_avr COMMAND ${CMAKE_COMMAND}
      -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR} -DCXX_COMPILER=${TC_AVR_CXX}
      -DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/avr
      "-DEXTRA_FLAGS=-mmcu=atmega2560;-fno-exceptions;-fno-rtti"
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/cpp_nodiscard.cmake)
  endif()

  # Tests that take more than a few seconds in a Release build. Faster vector
  # suites stay in `make test` and the push sanitizer runs.
  set(tc_extended_tests
    test_rsa_validation_1
    test_nist_dss_ecdsa_signatures_1
    test_nist_dss_rsa_generation
    test_nist_dss_rsa_keygen_validation
    test_wycheproof_ec
    test_wycheproof_rsa_signatures
    test_wycheproof_rsa_generation
    test_wycheproof_primality
    ${tc_wycheproof_oaep_shards}
    test_wycheproof_ecdsa
    test_ec_cavp
    test_trust_anchor_options
    test_config_rules
    test_installed_consumer
    test_installed_consumer_debug_environment
    test_resource_profiles
    test_piv_targets
    test_benchmark_fast
    test_benchmark_runtime
    test_drbg_cavp
    test_rsa_key_openssl
    test_rsa_oaep_openssl
    test_rsa_oaep_openssl_small
    test_rsa_pss_openssl
    test_rsa_pss_openssl_small
    ${tc_rsa_oaep_decrypt_shards}
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
