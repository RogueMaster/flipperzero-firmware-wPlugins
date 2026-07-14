/*
 * (c) 2017      Marcos Del Sol Vives
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef HAVE_NFC3D_VERSION_H
#define HAVE_NFC3D_VERSION_H

#include <stdint.h>

const char * nfc3d_version_fork();
uint32_t nfc3d_version_build();
uint32_t nfc3d_version_commit();

#endif
