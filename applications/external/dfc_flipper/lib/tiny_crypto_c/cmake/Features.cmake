# SPDX-License-Identifier: GPL-2.0-or-later
#
# Feature registry. cmake/features.json lists every feature with its config.h
# macro, description, parent, value set and the sources it compiles. This file
# declares one cache option per feature, registers the resolved macro value
# and selects the library sources. config.h owns the defaults and dependency
# rules, and ConfigCheck.cmake applies them to the resolved values.
#
# Naming rule: the option is TINY_CRYPTO_ followed by the macro name without
# its TC_ prefix, so TC_AES_ENABLE_CBC is set with TINY_CRYPTO_AES_ENABLE_CBC.
#
# After inclusion:
#   TC_FEATURE_OPTIONS   global property, every AUTO/ON/OFF option
#   TC_VALUE_OPTIONS     global property, every option with a named value set
#   <option>             0 or 1 for an AUTO/ON/OFF option, the name for a value option
#   tc_registry_sources(<out>)                  library sources for the selected features
#   tc_registry_family_sources(<out> <macro>...) every source the named features and
#                                               their sub-features can compile

set(tc_registry_file "${CMAKE_CURRENT_LIST_DIR}/features.json")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${tc_registry_file}")
file(READ "${tc_registry_file}" tc_registry)

function(tc_feature_option_name macro output)
  if(NOT macro MATCHES "^TC_[A-Z0-9_]+$")
    message(FATAL_ERROR "Feature macro ${macro} must start with TC_")
  endif()
  string(REGEX REPLACE "^TC_" "TINY_CRYPTO_" name "${macro}")
  set(${output} ${name} PARENT_SCOPE)
endfunction()

# Read a registry member, or default when the member is absent.
function(tc_registry_member output default)
  string(JSON value ERROR_VARIABLE missing GET "${tc_registry}" ${ARGN})
  if(missing)
    set(value "${default}")
  endif()
  set(${output} "${value}" PARENT_SCOPE)
endfunction()

# Read a registry array of strings as a CMake list.
function(tc_registry_strings output)
  set(items)
  string(JSON count ERROR_VARIABLE missing LENGTH "${tc_registry}" ${ARGN})
  if(NOT missing AND count GREATER 0)
    math(EXPR last "${count} - 1")
    foreach(index RANGE ${last})
      string(JSON item GET "${tc_registry}" ${ARGN} ${index})
      list(APPEND items "${item}")
    endforeach()
  endif()
  set(${output} "${items}" PARENT_SCOPE)
endfunction()

# Resolve an AUTO/ON/OFF option to 0 or 1. AUTO follows the application
# target, then the config.h default: a resource-profile value, or the value of
# the feature that config.h names. Macros in tc_platform_features, set by a
# platform port, have a floor of 1.
function(tc_resolve_switch option macro description output)
  set(${option} AUTO CACHE STRING "${description} (AUTO follows the resource profile)")
  set_property(CACHE ${option} PROPERTY STRINGS AUTO ON OFF)
  string(TOUPPER "${${option}}" choice)
  tc_target_default(${macro} role_default)
  set(platform_floor OFF)
  if(macro IN_LIST tc_platform_features)
    set(platform_floor ON)
  endif()
  if(choice STREQUAL "AUTO")
    if(DEFINED tc_profile_default_${macro})
      set(value ${tc_profile_default_${macro}})
    elseif(DEFINED tc_profile_follows_${macro})
      get_property(value GLOBAL PROPERTY TC_FEATURE_VALUE_${tc_profile_follows_${macro}})
      if(value STREQUAL "")
        message(FATAL_ERROR "${macro} follows ${tc_profile_follows_${macro}}, which is registered later")
      endif()
    else()
      message(FATAL_ERROR "config.h has no default for ${macro}")
    endif()
    if(NOT role_default STREQUAL "")
      set(value ${role_default})
    endif()
    if(platform_floor)
      set(value 1)
    endif()
  elseif(choice MATCHES "^(ON|TRUE|YES|1)$")
    set(value 1)
  elseif(choice MATCHES "^(OFF|FALSE|NO|0)$")
    set(value 0)
  else()
    message(FATAL_ERROR "${option} must be AUTO, ON, or OFF")
  endif()
  if(NOT role_default STREQUAL "" AND NOT value EQUAL role_default)
    message(FATAL_ERROR "${option} conflicts with ${TINY_CRYPTO_TARGET}; use AUTO or an unscoped target")
  endif()
  if(platform_floor AND value EQUAL 0)
    message(FATAL_ERROR "${option} is required by the platform port; use AUTO or ON")
  endif()
  set(${output} ${value} PARENT_SCOPE)
endfunction()

# Resolve a value option to the number of the selected name. A name without a
# number selects the resource-profile default from config.h.
function(tc_resolve_value option macro description index output)
  tc_registry_member(default "" features ${index} default)
  string(JSON count LENGTH "${tc_registry}" features ${index} values)
  math(EXPR last "${count} - 1")
  set(names)
  foreach(entry RANGE ${last})
    string(JSON name GET "${tc_registry}" features ${index} values ${entry} name)
    tc_registry_member(number "" features ${index} values ${entry} value)
    list(APPEND names "${name}")
    set(number_${name} "${number}")
  endforeach()
  set(${option} "${default}" CACHE STRING "${description}")
  set_property(CACHE ${option} PROPERTY STRINGS ${names})
  set(choice "${${option}}")
  if(NOT choice IN_LIST names)
    list(JOIN names ", " allowed)
    message(FATAL_ERROR "${option} must be one of: ${allowed}")
  endif()
  set(value "${number_${choice}}")
  if(value STREQUAL "")
    set(value "${tc_profile_default_${macro}}")
  endif()
  set(${output} ${value} PARENT_SCOPE)
endfunction()

# Declare the option for feature index, register its macro and record whether
# its sources compile: a feature is active when it is on and its parent is
# active.
function(tc_register_feature index)
  string(JSON macro GET "${tc_registry}" features ${index} macro)
  string(JSON description GET "${tc_registry}" features ${index} description)
  tc_registry_member(parent "" features ${index} parent)
  tc_registry_strings(sources features ${index} sources)
  tc_feature_option_name(${macro} option)
  if(NOT parent STREQUAL "")
    get_property(parent_registered GLOBAL PROPERTY TC_FEATURE_VALUE_${parent} SET)
    if(NOT parent_registered)
      message(FATAL_ERROR "${macro} names parent ${parent}, which is not registered before it")
    endif()
  endif()

  string(JSON kind ERROR_VARIABLE scalar TYPE "${tc_registry}" features ${index} values)
  if(scalar)
    set_property(GLOBAL APPEND PROPERTY TC_FEATURE_OPTIONS ${option})
    tc_resolve_switch(${option} ${macro} "${description}" value)
    set(enabled ${value})
    # A plain variable holding 0 or 1 lets later CMake code test the option.
    set(${option} ${value} PARENT_SCOPE)
  else()
    set_property(GLOBAL APPEND PROPERTY TC_VALUE_OPTIONS ${option})
    tc_resolve_value(${option} ${macro} "${description}" ${index} value)
    set(enabled 1)
  endif()
  tc_register_definition(${macro} ${value})

  set(active ${enabled})
  if(active AND NOT parent STREQUAL "")
    get_property(active GLOBAL PROPERTY TC_FEATURE_ACTIVE_${parent})
  endif()
  set_property(GLOBAL APPEND PROPERTY TC_FEATURE_MACROS ${macro})
  set_property(GLOBAL PROPERTY TC_FEATURE_VALUE_${macro} ${value})
  set_property(GLOBAL PROPERTY TC_FEATURE_ACTIVE_${macro} ${active})
  set_property(GLOBAL PROPERTY TC_FEATURE_PARENT_${macro} "${parent}")
  set_property(GLOBAL PROPERTY TC_FEATURE_SOURCES_${macro} "${sources}")
endfunction()

# Stop the configure on a TINY_CRYPTO_* cache entry that names no option. A
# retired name reports its replacement. Build, test and profile selection
# options are the names outside the registry.
function(tc_check_option_names)
  set(known)
  string(JSON count LENGTH "${tc_registry}" features)
  math(EXPR last "${count} - 1")
  foreach(index RANGE ${last})
    string(JSON macro GET "${tc_registry}" features ${index} macro)
    tc_feature_option_name(${macro} option)
    list(APPEND known ${option})
  endforeach()
  set(problems)
  get_cmake_property(cache_names CACHE_VARIABLES)
  foreach(name IN LISTS cache_names)
    if(NOT name MATCHES "^TINY_CRYPTO_" OR name IN_LIST known OR
       name MATCHES "^TINY_CRYPTO_(BUILD_[A-Z0-9_]+|TEST_[A-Z0-9_]+|SANITIZE|RESOURCE_PROFILE|TARGET)$")
      continue()
    endif()
    string(JSON replacement ERROR_VARIABLE unknown GET "${tc_registry}" retired_options ${name})
    if(unknown)
      list(APPEND problems "  ${name} is not an option (see cmake/features.json)")
    elseif(replacement STREQUAL "")
      list(APPEND problems "  ${name} was removed")
    else()
      list(APPEND problems "  ${name} was renamed to ${replacement}")
    endif()
  endforeach()
  if(problems)
    list(JOIN problems "\n" report)
    message(FATAL_ERROR "Unknown tiny_crypto_c options:\n${report}\n"
      "Remove each entry from the cache with cmake -U <name>, or use a new build directory.")
  endif()
endfunction()
tc_check_option_names()

set_property(GLOBAL PROPERTY TC_FEATURE_OPTIONS "")
set_property(GLOBAL PROPERTY TC_VALUE_OPTIONS "")
set_property(GLOBAL PROPERTY TC_FEATURE_MACROS "")
string(JSON tc_feature_count LENGTH "${tc_registry}" features)
math(EXPR tc_feature_last "${tc_feature_count} - 1")
foreach(tc_feature_index RANGE ${tc_feature_last})
  tc_register_feature(${tc_feature_index})
endforeach()

# Sources of the features in scope, and of each shared-source entry that lists
# one of them. scope_property names the per-feature property that says whether
# a feature is in scope.
function(tc_registry_collect output scope_property)
  tc_registry_strings(sources core_sources)
  get_property(macros GLOBAL PROPERTY TC_FEATURE_MACROS)
  foreach(macro IN LISTS macros)
    get_property(in_scope GLOBAL PROPERTY ${scope_property}_${macro})
    if(in_scope)
      get_property(owned GLOBAL PROPERTY TC_FEATURE_SOURCES_${macro})
      list(APPEND sources ${owned})
    endif()
  endforeach()
  string(JSON count LENGTH "${tc_registry}" shared_sources)
  math(EXPR last "${count} - 1")
  foreach(entry RANGE ${last})
    tc_registry_strings(users shared_sources ${entry} features)
    foreach(macro IN LISTS users)
      get_property(registered GLOBAL PROPERTY TC_FEATURE_VALUE_${macro} SET)
      if(NOT registered)
        message(FATAL_ERROR "Shared sources name unregistered feature ${macro}")
      endif()
      get_property(in_scope GLOBAL PROPERTY ${scope_property}_${macro})
      if(in_scope)
        tc_registry_strings(shared shared_sources ${entry} sources)
        list(APPEND sources ${shared})
        break()
      endif()
    endforeach()
  endforeach()
  list(REMOVE_DUPLICATES sources)
  set(${output} ${sources} PARENT_SCOPE)
endfunction()

function(tc_registry_sources output)
  tc_registry_collect(sources TC_FEATURE_ACTIVE)
  set(${output} ${sources} PARENT_SCOPE)
endfunction()

# Test libraries compile whole families and select modes with definitions.
function(tc_registry_family_sources output)
  get_property(macros GLOBAL PROPERTY TC_FEATURE_MACROS)
  foreach(macro IN LISTS macros)
    set(ancestor ${macro})
    set(in_family 0)
    while(NOT ancestor STREQUAL "")
      if(ancestor IN_LIST ARGN)
        set(in_family 1)
        break()
      endif()
      get_property(ancestor GLOBAL PROPERTY TC_FEATURE_PARENT_${ancestor})
    endwhile()
    set_property(GLOBAL PROPERTY TC_FEATURE_FAMILY_${macro} ${in_family})
  endforeach()
  tc_registry_collect(sources TC_FEATURE_FAMILY)
  # The core sources belong to every build. Test targets list them themselves.
  tc_registry_strings(core core_sources)
  list(REMOVE_ITEM sources ${core})
  set(${output} ${sources} PARENT_SCOPE)
endfunction()
