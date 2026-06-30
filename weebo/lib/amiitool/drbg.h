/*
 * (c) 2015-2017 Marcos Del Sol Vives
 * (c) 2016      javiMaD
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef HAVE_NFC3D_DRBG_H
#define HAVE_NFC3D_DRBG_H

#include <stdbool.h>
#include <stdint.h>
#include "mbedtls/md.h"

#define NFC3D_DRBG_MAX_SEED_SIZE	480	/* Hardcoded max size in 3DS NFC module */
#define NFC3D_DRBG_OUTPUT_SIZE		32	/* Every iteration generates 32 bytes */

typedef struct {
	mbedtls_md_context_t hmacCtx;
	bool used;
	uint16_t iteration;

	uint8_t buffer[sizeof(uint16_t) + NFC3D_DRBG_MAX_SEED_SIZE];
	size_t bufferSize;
} nfc3d_drbg_ctx;

void nfc3d_drbg_init(nfc3d_drbg_ctx * ctx, const uint8_t * hmacKey, size_t hmacKeySize, const uint8_t * seed, size_t seedSize);
void nfc3d_drbg_step(nfc3d_drbg_ctx * ctx, uint8_t * output);
void nfc3d_drbg_cleanup(nfc3d_drbg_ctx * ctx);
void nfc3d_drbg_generate_bytes(const uint8_t * hmacKey, size_t hmacKeySize, const uint8_t * seed, size_t seedSize, uint8_t * output, size_t outputSize);

#endif

