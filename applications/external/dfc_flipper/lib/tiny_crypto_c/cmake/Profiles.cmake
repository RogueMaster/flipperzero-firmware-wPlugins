# SPDX-License-Identifier: GPL-2.0-or-later
include(${CMAKE_CURRENT_LIST_DIR}/Targets.cmake)

# Every public configuration macro is registered once, as MACRO=value, in the
# TINY_CRYPTO_PUBLIC_DEFINITIONS global property. CMakeLists.txt passes the list
# to the library target, to ConfigCheck.cmake and to the installed
# build_config.h, so headers and objects always see the same values.
set_property(GLOBAL PROPERTY TINY_CRYPTO_PUBLIC_DEFINITIONS "")
set_property(GLOBAL PROPERTY TINY_CRYPTO_PUBLIC_MACROS "")

function(tc_register_definition macro value)
  get_property(macros GLOBAL PROPERTY TINY_CRYPTO_PUBLIC_MACROS)
  if(macro IN_LIST macros)
    message(FATAL_ERROR "${macro} is registered twice")
  endif()
  set_property(GLOBAL APPEND PROPERTY TINY_CRYPTO_PUBLIC_MACROS ${macro})
  set_property(GLOBAL APPEND PROPERTY TINY_CRYPTO_PUBLIC_DEFINITIONS "${macro}=${value}")
endfunction()

set(TINY_CRYPTO_RESOURCE_PROFILE "" CACHE STRING "Resource profile: micro, mini, desktop, or empty for existing defaults")
set_property(CACHE TINY_CRYPTO_RESOURCE_PROFILE PROPERTY STRINGS "" micro mini desktop)
set(tc_profile_names default micro mini desktop)
if(TINY_CRYPTO_RESOURCE_PROFILE STREQUAL "")
  set(tc_profile_index 0)
else()
  list(FIND tc_profile_names "${TINY_CRYPTO_RESOURCE_PROFILE}" tc_profile_index)
  if(tc_profile_index LESS 1)
    message(FATAL_ERROR "Unknown TINY_CRYPTO_RESOURCE_PROFILE")
  endif()
endif()
tc_register_definition(TC_RESOURCE_PROFILE ${tc_profile_index})

# Read the config.h defaults without running a target executable when
# cross-compiling. A default is a resource-profile value, or the name of the
# feature whose value it follows (TC_TAF_ENABLE_* follow the trust-anchor
# format).
file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/../src/tiny_crypto/config.h" tc_profile_entries
  REGEX "^#define TC_[A-Z0-9_]+ TC_(PROFILE_VALUE|ENABLE_[A-Z0-9_]+$)")
foreach(entry IN LISTS tc_profile_entries)
  if(entry MATCHES "^#define (TC_[A-Z0-9_]+) (TC_ENABLE_[A-Z0-9_]+)$")
    set(tc_profile_follows_${CMAKE_MATCH_1} ${CMAKE_MATCH_2})
    continue()
  endif()
  if(NOT entry MATCHES "^#define (TC_[A-Z0-9_]+) TC_PROFILE_VALUE\\(([0-9]+), ([0-9]+), ([0-9]+), ([0-9]+)\\)$")
    message(FATAL_ERROR "Malformed resource default: ${entry}")
  endif()
  set(values ${CMAKE_MATCH_2} ${CMAKE_MATCH_3} ${CMAKE_MATCH_4} ${CMAKE_MATCH_5})
  list(GET values ${tc_profile_index} tc_profile_default_${CMAKE_MATCH_1})
endforeach()
