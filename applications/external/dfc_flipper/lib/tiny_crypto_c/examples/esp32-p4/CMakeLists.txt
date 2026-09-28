# SPDX-License-Identifier: GPL-2.0-or-later
cmake_minimum_required(VERSION 3.24)
set(IDF_TARGET esp32p4)
set(COMPONENTS main)
set(PROJECT_VER "1.0.1")
set(SDKCONFIG "${CMAKE_BINARY_DIR}/sdkconfig")
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../ports/esp-idf/tiny_crypto_c")
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../ports/esp-idf/vendor/bootloader_support")
set(TINY_CRYPTO_TARGET piv-acu CACHE STRING "PIV role")
set(TINY_CRYPTO_RESOURCE_PROFILE mini CACHE STRING "Resource profile")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(tiny_crypto_piv)

idf_build_get_property(tc_idf_components BUILD_COMPONENTS)
if("mbedtls" IN_LIST tc_idf_components)
  message(FATAL_ERROR
    "The ESP32-P4 firmware must not include mbedTLS. This IDF configuration "
    "pulls it in through framework dependencies; use an mbedTLS-free platform integration.")
endif()
