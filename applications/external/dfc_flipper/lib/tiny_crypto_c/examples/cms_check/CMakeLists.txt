# SPDX-License-Identifier: GPL-2.0-or-later
cmake_minimum_required(VERSION 3.24)
project(cms_check LANGUAGES C)
find_package(tiny_crypto_c CONFIG REQUIRED)
add_executable(cms_check ../cms_check.c ../cms_reader.c ../cms_validate.c ../pki_input.c)
target_link_libraries(cms_check PRIVATE tiny_crypto_c::tiny_crypto_c)
set_target_properties(cms_check PROPERTIES C_STANDARD 99
  C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
