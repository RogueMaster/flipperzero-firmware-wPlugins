/*
 * (c) 2015-2017 Marcos Del Sol Vives
 *
 * SPDX-License-Identifier: MIT
 */

#include "drbg.h"
#include "keygen.h"
#include "util.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void nfc3d_keygen_prepare_seed(const nfc3d_keygen_masterkeys * baseKeys, const uint8_t * baseSeed, uint8_t * output, size_t * outputSize) {
	assert(baseKeys != NULL);
	assert(baseSeed != NULL);
	assert(output != NULL);
	assert(outputSize != NULL);

	uint8_t * start = output;

	// 1: Copy whole type string
	// output = memccpy(output, baseKeys->typeString, '\0', sizeof(baseKeys->typeString));
  size_t strLen = strlen((char*)baseKeys->typeString);
  size_t typeStringSize = sizeof(baseKeys->typeString);
  output = (uint8_t*)strncpy((char*)output, baseKeys->typeString, typeStringSize);
  output += strLen + 1;

	// 2: Append (16 - magicBytesSize) from the input seed
	size_t leadingSeedBytes = 16 - baseKeys->magicBytesSize;
	memcpy(output, baseSeed, leadingSeedBytes);
	output += leadingSeedBytes;

	// 3: Append all bytes from magicBytes
	memcpy(output, baseKeys->magicBytes, baseKeys->magicBytesSize);
	output += baseKeys->magicBytesSize;

	// 4: Append bytes 0x10-0x1F from input seed
	memcpy(output, baseSeed + 0x10, 16);
	output += 16;

	// 5: Xor last bytes 0x20-0x3F of input seed with AES XOR pad and append them
	unsigned int i;
	for (i = 0; i < 32; i++) {
		output[i] = baseSeed[i + 32] ^ baseKeys->xorPad[i];
	}
	output += 32;

	*outputSize = output - start;
}

void nfc3d_keygen(const nfc3d_keygen_masterkeys * baseKeys, const uint8_t * baseSeed, nfc3d_keygen_derivedkeys * derivedKeys) {
	uint8_t preparedSeed[NFC3D_DRBG_MAX_SEED_SIZE];
	size_t preparedSeedSize;

	nfc3d_keygen_prepare_seed(baseKeys, baseSeed, preparedSeed, &preparedSeedSize);
	nfc3d_drbg_generate_bytes(baseKeys->hmacKey, sizeof(baseKeys->hmacKey), preparedSeed, preparedSeedSize, (uint8_t *) derivedKeys, sizeof(*derivedKeys));
}
