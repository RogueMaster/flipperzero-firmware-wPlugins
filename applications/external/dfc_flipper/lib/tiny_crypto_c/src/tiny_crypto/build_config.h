/*
 * SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
/* Configuration recorded by an installed library.
 *
 * The source tree ships this header empty. Source builds configure through -D
 * definitions or the config.h defaults. `cmake --install` installs a generated
 * header in its place. That header defines every public configuration macro
 * with the value the installed archive was built with. config.h includes it
 * again at its end with TC_BUILD_CONFIG_VERIFY defined, and a -D definition
 * with a different value fails with "<macro> differs from the installed library
 * configuration". Options: README.md. Contracts: docs/api.md. */
#ifndef TINY_CRYPTO_BUILD_CONFIG_H_
#define TINY_CRYPTO_BUILD_CONFIG_H_
#endif
