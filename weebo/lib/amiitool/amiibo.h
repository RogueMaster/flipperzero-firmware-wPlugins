/*
 * (c) 2015-2017 Marcos Del Sol Vives
 * (c) 2016      javiMaD
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef HAVE_NFC3D_AMIIBO_H
#define HAVE_NFC3D_AMIIBO_H

#include <stdint.h>
#include <stdbool.h>
#include "keygen.h"

#define NFC3D_AMIIBO_SIZE 520

#pragma pack(1)
typedef struct {
	nfc3d_keygen_masterkeys data;
	nfc3d_keygen_masterkeys tag;
} nfc3d_amiibo_keys;
#pragma pack()

bool nfc3d_amiibo_unpack(const nfc3d_amiibo_keys * amiiboKeys, const uint8_t * tag, uint8_t * plain);
void nfc3d_amiibo_pack(const nfc3d_amiibo_keys * amiiboKeys, const uint8_t * plain, uint8_t * tag);
bool nfc3d_amiibo_load_keys(nfc3d_amiibo_keys * amiiboKeys, const char * path);
void nfc3d_amiibo_copy_app_data(const uint8_t * src, uint8_t * dst);

#endif
