# SPDX-FileCopyrightText: kokke
# SPDX-FileCopyrightText: Mistial Dev
# SPDX-License-Identifier: Unlicense

cmake_minimum_required(VERSION 3.12)
project(TinyAesPackageTest C CXX)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

add_executable(example ../test.c ../munit.c)
add_executable(example_cpp ../test.cpp ../munit.c)

# CBC/CTR/ECB are public configuration macros but collide with Apple system
# headers included by munit.c. Keep them out of the standalone test harness.
set_source_files_properties(../munit.c PROPERTIES COMPILE_OPTIONS "-UCBC;-UECB;-UCTR")

target_link_libraries(example ${CONAN_LIBS})
target_link_libraries(example_cpp ${CONAN_LIBS})
