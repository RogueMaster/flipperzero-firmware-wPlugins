/*
 * (c) 2015-2017 Marcos Del Sol Vives
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef HAVE_NFC3D_KEYGEN_H
#define HAVE_NFC3D_KEYGEN_H

#include <stdint.h>
#include <stdbool.h>

#define NFC3D_KEYGEN_SEED_SIZE 64

#pragma pack(1)
typedef struct {
	uint8_t hmacKey[16];
	char typeString[14];
	uint8_t rfu;
	uint8_t magicBytesSize;
	uint8_t magicBytes[16];
	uint8_t xorPad[32];
} nfc3d_keygen_masterkeys;

typedef struct {
	const uint8_t aesKey[16];
	const uint8_t aesIV[16];
	const uint8_t hmacKey[16];
} nfc3d_keygen_derivedkeys;
#pragma pack()

void nfc3d_keygen(const nfc3d_keygen_masterkeys * baseKeys, const uint8_t * baseSeed, nfc3d_keygen_derivedkeys * derivedKeys);

#endif
