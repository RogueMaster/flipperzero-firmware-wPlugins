# SPDX-License-Identifier: GPL-2.0-or-later
cmake_minimum_required(VERSION 3.24)
project(piv_inspect LANGUAGES C)
find_package(tiny_crypto_c CONFIG REQUIRED)
# The flow and its text output. Card commands and checks come from the
# library.
add_library(piv_inspect_flow STATIC ../piv_inspect.c ../piv_inspect_trust.c ../piv_inspect_crl.c
  ../piv_inspect_print_objects.c ../piv_inspect_print_certificates.c
  ../piv_inspect_print_report.c)
target_link_libraries(piv_inspect_flow PUBLIC tiny_crypto_c::tiny_crypto_c)
# The PC/SC transport and the host services of the command.
add_executable(piv_inspect ../piv_inspect_main.c ../credential_pcsc.c ../credential_system.c
  ../pki_input.c)
target_link_libraries(piv_inspect PRIVATE piv_inspect_flow)
if(APPLE)
  find_library(PCSC_LIBRARY PCSC REQUIRED)
  target_link_libraries(piv_inspect PRIVATE ${PCSC_LIBRARY})
elseif(UNIX)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(PCSC REQUIRED IMPORTED_TARGET libpcsclite)
  target_link_libraries(piv_inspect PRIVATE PkgConfig::PCSC)
else()
  message(FATAL_ERROR "This utility requires POSIX terminals, memory locking and core-file controls")
endif()
set_target_properties(piv_inspect_flow piv_inspect PROPERTIES C_STANDARD 99
  C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
enable_testing()
add_test(NAME piv_inspect_help COMMAND piv_inspect --help)
add_test(NAME piv_inspect_arguments COMMAND piv_inspect)
set_tests_properties(piv_inspect_arguments PROPERTIES WILL_FAIL TRUE)
add_test(NAME piv_inspect_unknown_option COMMAND piv_inspect --reader x --anchor a --bogus y)
set_tests_properties(piv_inspect_unknown_option PROPERTIES WILL_FAIL TRUE)
