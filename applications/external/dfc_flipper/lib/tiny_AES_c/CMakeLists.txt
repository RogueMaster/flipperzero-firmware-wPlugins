# SPDX-FileCopyrightText: kokke
# SPDX-FileCopyrightText: Mistial Dev
# SPDX-License-Identifier: Unlicense

cmake_minimum_required(VERSION 3.12)

project(tiny-aes C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(TINY_AES_BUILD_TESTS "Build unit tests" ON)
option(TINY_AES_ENABLE_CBC "Enable CBC mode in the main library" OFF)
option(TINY_AES_ENABLE_ECB "Enable ECB mode in the main library" OFF)
option(TINY_AES_ENABLE_CTR "Enable CTR mode in the main library" ON)
option(TINY_AES_ENABLE_OFB "Enable OFB mode in the main library" OFF)
option(TINY_AES_ENABLE_GCM "Enable GCM mode in the main library" OFF)
option(TINY_AES_ENABLE_CCM "Enable CCM mode in the main library" OFF)
option(TINY_AES_ENABLE_EAX "Enable EAX mode in the main library" OFF)
option(TINY_AES_ENABLE_EAX_PRIME "Enable ANSI C12.22 EAX' mode in the main library" OFF)
option(TINY_AES_ENABLE_SIV "Enable AES-SIV (RFC 5297) in the main library" OFF)
option(TINY_AES_ENABLE_CMAC "Enable AES-CMAC (SP 800-38B) in the main library" OFF)
option(TINY_AES_CAVP "Run the complete AES CAVP response corpora" OFF)
option(TINY_AES_WIDE_OPS "Enable portable native-width block helpers" OFF)
option(TINY_AES_ZEROIZE "Wipe stack secrets in one-shot paths" ON)
option(TINY_AES_STRICT "Enable NULL checks on classical buffer APIs" OFF)
option(TINY_AES_TINY "Reject large GHASH table profiles at compile time" OFF)
set(TINY_AES_SBOX_MODE "secure" CACHE STRING "S-box mode: secure, runtime, or fast")
set_property(CACHE TINY_AES_SBOX_MODE PROPERTY STRINGS secure runtime fast)
set(TINY_AES_GCM_GHASH_MODE "auto" CACHE STRING "GHASH mode: auto, bitwise, wide, table4, fast-table, or hardware")
set_property(CACHE TINY_AES_GCM_GHASH_MODE PROPERTY STRINGS auto bitwise wide table4 fast-table hardware)

set(_ghash_modes auto bitwise wide table4 fast-table hardware)
list(FIND _ghash_modes "${TINY_AES_GCM_GHASH_MODE}" _ghash_index)
if(_ghash_index EQUAL -1)
    message(FATAL_ERROR "TINY_AES_GCM_GHASH_MODE must be auto, bitwise, wide, table4, fast-table, or hardware")
endif()
set(TINY_AES_GCM_GHASH_DEFINITION AES_GCM_GHASH_MODE=${_ghash_index})

if(TINY_AES_SBOX_MODE STREQUAL "secure")
    set(TINY_AES_SBOX_DEFINITION AES_SBOX_MODE=1)
elseif(TINY_AES_SBOX_MODE STREQUAL "runtime")
    set(TINY_AES_SBOX_DEFINITION AES_SBOX_MODE=2)
elseif(TINY_AES_SBOX_MODE STREQUAL "fast")
    set(TINY_AES_SBOX_DEFINITION AES_SBOX_MODE=3)
else()
    message(FATAL_ERROR "TINY_AES_SBOX_MODE must be secure, runtime, or fast")
endif()

if(TINY_AES_WIDE_OPS)
    set(TINY_AES_WIDE_DEFINITION AES_WIDE_OPS=1)
else()
    set(TINY_AES_WIDE_DEFINITION AES_WIDE_OPS=0)
endif()

function(tiny_aes_mode_definitions output cbc ecb ctr ofb eax)
    set(${output}
        AES_ENABLE_CBC=${cbc}
        AES_ENABLE_ECB=${ecb}
        AES_ENABLE_CTR=${ctr}
        AES_ENABLE_OFB=${ofb}
        AES_ENABLE_EAX=${eax}
        ${TINY_AES_SBOX_DEFINITION}
        ${TINY_AES_WIDE_DEFINITION}
        PARENT_SCOPE
    )
endfunction()

if(TINY_AES_ENABLE_CBC)
    set(TINY_AES_CBC 1)
else()
    set(TINY_AES_CBC 0)
endif()
if(TINY_AES_ENABLE_ECB)
    set(TINY_AES_ECB 1)
else()
    set(TINY_AES_ECB 0)
endif()
if(TINY_AES_ENABLE_CTR)
    set(TINY_AES_CTR 1)
else()
    set(TINY_AES_CTR 0)
endif()
if(TINY_AES_ENABLE_OFB)
    set(TINY_AES_OFB 1)
else()
    set(TINY_AES_OFB 0)
endif()
if(TINY_AES_ENABLE_GCM)
    set(TINY_AES_GCM 1)
else()
    set(TINY_AES_GCM 0)
endif()
if(TINY_AES_ENABLE_CCM)
    set(TINY_AES_CCM 1)
else()
    set(TINY_AES_CCM 0)
endif()
if(TINY_AES_ENABLE_EAX)
    set(TINY_AES_EAX 1)
else()
    set(TINY_AES_EAX 0)
endif()
if(TINY_AES_ENABLE_EAX_PRIME)
    set(TINY_AES_EAX_PRIME 1)
else()
    set(TINY_AES_EAX_PRIME 0)
endif()
if(TINY_AES_ENABLE_SIV)
    set(TINY_AES_SIV 1)
else()
    set(TINY_AES_SIV 0)
endif()
if(TINY_AES_ENABLE_CMAC)
    set(TINY_AES_CMAC 1)
else()
    set(TINY_AES_CMAC 0)
endif()
if(TINY_AES_CAVP)
    set(TINY_AES_CAVP_DEFINITION 1)
else()
    set(TINY_AES_CAVP_DEFINITION 0)
endif()

tiny_aes_mode_definitions(tiny_aes_library_definitions
    ${TINY_AES_CBC} ${TINY_AES_ECB} ${TINY_AES_CTR} ${TINY_AES_OFB}
    ${TINY_AES_EAX})
if(TINY_AES_ZEROIZE)
    set(TINY_AES_ZEROIZE_VALUE 1)
else()
    set(TINY_AES_ZEROIZE_VALUE 0)
endif()
if(TINY_AES_STRICT)
    set(TINY_AES_STRICT_VALUE 1)
else()
    set(TINY_AES_STRICT_VALUE 0)
endif()
if(TINY_AES_TINY)
    set(TINY_AES_TINY_VALUE 1)
else()
    set(TINY_AES_TINY_VALUE 0)
endif()
list(APPEND tiny_aes_library_definitions AES_ENABLE_GCM=${TINY_AES_GCM}
    AES_ENABLE_CCM=${TINY_AES_CCM}
    AES_ENABLE_EAX_PRIME=${TINY_AES_EAX_PRIME}
    AES_ENABLE_SIV=${TINY_AES_SIV}
    AES_ENABLE_CMAC=${TINY_AES_CMAC}
    AES_ZEROIZE=${TINY_AES_ZEROIZE_VALUE}
    AES_STRICT=${TINY_AES_STRICT_VALUE}
    AES_TINY=${TINY_AES_TINY_VALUE}
    ${TINY_AES_GCM_GHASH_DEFINITION})

add_library(tiny-aes STATIC aes.c)
target_include_directories(tiny-aes PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_compile_definitions(tiny-aes PUBLIC ${tiny_aes_library_definitions})

if(TINY_AES_ENABLE_GCM)
    add_executable(tiny-aes-benchmark benchmark.c)
    target_link_libraries(tiny-aes-benchmark PRIVATE tiny-aes)
    target_compile_definitions(tiny-aes-benchmark PRIVATE AES_ENABLE_GCM=1 AES128=1)
endif()

if(TINY_AES_BUILD_TESTS)
    enable_testing()

    add_executable(test_aes_cpp test_cpp.cpp aes.c)
    target_include_directories(test_aes_cpp PRIVATE ${CMAKE_CURRENT_LIST_DIR})
    target_compile_definitions(test_aes_cpp PRIVATE AES_ENABLE_CBC=1 AES_ENABLE_ECB=1 AES_ENABLE_CTR=1 AES_ENABLE_OFB=1 AES_ENABLE_GCM=1 AES_ENABLE_CMAC=1 AES128=1)
    add_test(NAME test_aes_cpp COMMAND test_aes_cpp)

    add_library(tiny-aes-munit OBJECT munit.c)

    function(add_tiny_aes_test key mode gcm ccm eax ghash_mode sbox_mode wide cavp)
        set(cbc 0)
        set(ecb 0)
        set(ctr 0)
        set(ofb 0)
        set(eax_mode ${eax})
        set(eax_prime_mode 0)
        set(siv_mode 0)
        set(cmac_mode 0)
        if(mode STREQUAL "ecb")
            set(ecb 1)
        elseif(mode STREQUAL "cbc")
            set(cbc 1)
        elseif(mode STREQUAL "ctr")
            set(ctr 1)
        elseif(mode STREQUAL "ofb")
            set(ofb 1)
        elseif(mode STREQUAL "all")
            set(cbc 1)
            set(ecb 1)
            set(ctr 1)
            set(ofb 1)
            set(eax_mode 1)
            set(siv_mode 1)
            set(cmac_mode 1)
        elseif(mode STREQUAL "eax")
            set(eax_mode 1)
        elseif(mode STREQUAL "eax-prime")
            set(eax_prime_mode 1)
        elseif(mode STREQUAL "siv")
            set(siv_mode 1)
        elseif(mode STREQUAL "cmac")
            set(cmac_mode 1)
        elseif(NOT mode STREQUAL "ccm" AND NOT mode STREQUAL "gcm")
            message(FATAL_ERROR "Unknown AES test mode: ${mode}")
        endif()

        if(key STREQUAL "128")
            set(key_definition AES128=1)
        else()
            set(key_definition AES${key}=1)
        endif()

        if(sbox_mode STREQUAL "secure")
            set(sbox_definition AES_SBOX_MODE=1)
        elseif(sbox_mode STREQUAL "runtime")
            set(sbox_definition AES_SBOX_MODE=2)
        else()
            set(sbox_definition AES_SBOX_MODE=3)
        endif()

        list(FIND _ghash_modes "${ghash_mode}" ghash_index)
        if(ghash_mode STREQUAL "hardware")
            set(hardware_definition AES_GCM_GHASH_HARDWARE_MULTIPLY=AES_CAVP_GHASH_HARDWARE_MULTIPLY)
        else()
            set(hardware_definition)
        endif()

        set(test_name "${key}-${mode}-gcm${gcm}-ccm${ccm}-eax${eax_mode}-eaxprime${eax_prime_mode}-siv${siv_mode}-cmac${cmac_mode}-ghash${ghash_index}-${sbox_mode}-wide${wide}-cavp${cavp}")
        add_library(test_core_${test_name} OBJECT aes.c test.c cavp.c eax_test.c siv_test.c cmac_test.c)
        target_include_directories(test_core_${test_name} PRIVATE ${CMAKE_CURRENT_LIST_DIR})
        # CAVP includes Tlen=4/5; lower floor only in CMAC-enabled test objects.
        if(cmac_mode)
            set(cmac_min_tag AES_CMAC_MIN_TAG_LEN=4)
        else()
            set(cmac_min_tag)
        endif()
        target_compile_definitions(test_core_${test_name} PRIVATE
            AES_ENABLE_CBC=${cbc} AES_ENABLE_ECB=${ecb} AES_ENABLE_CTR=${ctr} AES_ENABLE_OFB=${ofb} AES_ENABLE_GCM=${gcm} AES_ENABLE_CCM=${ccm} AES_ENABLE_EAX=${eax_mode} AES_ENABLE_EAX_PRIME=${eax_prime_mode} AES_ENABLE_SIV=${siv_mode} AES_ENABLE_CMAC=${cmac_mode}
            AES_CAVP=${cavp}
            AES_GCM_GHASH_MODE=${ghash_index}
            ${key_definition} ${sbox_definition} AES_WIDE_OPS=${wide}
            ${hardware_definition}
            ${cmac_min_tag}
            CAVP_VECTOR_DIR=\"${CMAKE_CURRENT_LIST_DIR}/test_vectors/cavp\"
            EAX_VECTOR_FILE=\"${CMAKE_CURRENT_LIST_DIR}/test_vectors/eax/aes_eax_test.json\"
            SIV_VECTOR_FILE=\"${CMAKE_CURRENT_LIST_DIR}/test_vectors/siv/aead_aes_siv_cmac_test.json\"
            CMAC_WYCHEPROOF_FILE=\"${CMAKE_CURRENT_LIST_DIR}/test_vectors/cmac/aes_cmac_test.json\"
            CMAC_CAVP_DIR=\"${CMAKE_CURRENT_LIST_DIR}/test_vectors/cmac\"
        )

        add_executable(test_aes_${test_name}
            $<TARGET_OBJECTS:test_core_${test_name}>
            $<TARGET_OBJECTS:tiny-aes-munit>
        )
        add_test(NAME test_aes_${test_name} COMMAND test_aes_${test_name})
    endfunction()

    # Each profile is paired with the minimum configuration that exercises it.
    # Mode subsets are deliberately not multiplied together.
    set(cipher_profiles secure:0 runtime:0 fast:0 secure:1)
    set(gcm_profiles
        secure:0:auto secure:1:auto secure:0:bitwise secure:1:wide
        secure:0:table4 secure:0:fast-table secure:0:hardware
        runtime:0:auto fast:0:auto)
    foreach(key IN ITEMS 128 192 256)
        if(TINY_AES_CAVP)
            set(cavp 1)
        else()
            set(cavp 0)
        endif()

        foreach(mode IN ITEMS ecb cbc ctr ofb)
            foreach(profile IN LISTS cipher_profiles)
                string(REPLACE ":" ";" profile_values "${profile}")
                list(GET profile_values 0 sbox_mode)
                list(GET profile_values 1 wide)
                add_tiny_aes_test(${key} ${mode} 0 0 0 auto ${sbox_mode} ${wide} ${cavp})
            endforeach()
        endforeach()

        foreach(profile IN LISTS cipher_profiles)
            string(REPLACE ":" ";" profile_values "${profile}")
            list(GET profile_values 0 sbox_mode)
            list(GET profile_values 1 wide)
            add_tiny_aes_test(${key} ccm 0 1 0 auto ${sbox_mode} ${wide} ${cavp})
        endforeach()

        foreach(profile IN LISTS gcm_profiles)
            string(REPLACE ":" ";" profile_values "${profile}")
            list(GET profile_values 0 sbox_mode)
            list(GET profile_values 1 wide)
            list(GET profile_values 2 ghash_mode)
            add_tiny_aes_test(${key} gcm 1 0 0 ${ghash_mode} ${sbox_mode} ${wide} ${cavp})
        endforeach()

        foreach(sbox_mode IN ITEMS secure runtime fast)
            add_tiny_aes_test(${key} eax 0 0 1 auto ${sbox_mode} 0 0)
        endforeach()

        if(key STREQUAL "128")
            foreach(sbox_mode IN ITEMS secure runtime fast)
                add_tiny_aes_test(${key} eax-prime 0 0 0 auto ${sbox_mode} 0 0)
            endforeach()
        endif()

        foreach(sbox_mode IN ITEMS secure runtime fast)
            add_tiny_aes_test(${key} siv 0 0 0 auto ${sbox_mode} 0 0)
        endforeach()

        foreach(sbox_mode IN ITEMS secure runtime fast)
            add_tiny_aes_test(${key} cmac 0 0 0 auto ${sbox_mode} 0 0)
        endforeach()

        if(NOT TINY_AES_CAVP)
            add_tiny_aes_test(${key} all 1 1 1 auto secure 1 0)
        endif()
    endforeach()
endif()
