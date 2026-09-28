/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TINY_CRYPTO_RESOURCE_PROFILE_H_
#define TINY_CRYPTO_RESOURCE_PROFILE_H_

#define TC_RESOURCE_DEFAULT 0
#define TC_RESOURCE_MICRO 1
#define TC_RESOURCE_MINI 2
#define TC_RESOURCE_DESKTOP 3

#ifndef TC_RESOURCE_PROFILE
#define TC_RESOURCE_PROFILE TC_RESOURCE_DEFAULT
#endif
#if TC_RESOURCE_PROFILE < TC_RESOURCE_DEFAULT || TC_RESOURCE_PROFILE > TC_RESOURCE_DESKTOP
#error "Unknown TC_RESOURCE_PROFILE"
#endif

/* Keep the defaults in config.h: CMake reads the same four-value entries. */
#if TC_RESOURCE_PROFILE == TC_RESOURCE_MICRO
#define TC_PROFILE_VALUE(original, micro, mini, desktop) micro
#elif TC_RESOURCE_PROFILE == TC_RESOURCE_MINI
#define TC_PROFILE_VALUE(original, micro, mini, desktop) mini
#elif TC_RESOURCE_PROFILE == TC_RESOURCE_DESKTOP
#define TC_PROFILE_VALUE(original, micro, mini, desktop) desktop
#else
#define TC_PROFILE_VALUE(original, micro, mini, desktop) original
#endif
#endif
