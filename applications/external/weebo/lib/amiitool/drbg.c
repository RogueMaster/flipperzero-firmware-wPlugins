/*
 * (c) 2015-2017 Marcos Del Sol Vives
 * (c) 2016      javiMaD
 *
 * SPDX-License-Identifier: MIT
 */

#include "drbg.h"
#include <assert.h>
#include <string.h>
#include <mbedtls/md.h>

void nfc3d_drbg_init(nfc3d_drbg_ctx * ctx, const uint8_t * hmacKey, size_t hmacKeySize, const uint8_t * seed, size_t seedSize) {
	assert(ctx != NULL);
	assert(hmacKey != NULL);
	assert(seed != NULL);
	assert(seedSize <= NFC3D_DRBG_MAX_SEED_SIZE);

	// Initialize primitives
	ctx->used = false;
	ctx->iteration = 0;
	ctx->bufferSize = sizeof(ctx->iteration) + seedSize;

	// The 16-bit counter is prepended to the seed when hashing, so we'll leave 2 bytes at the start
	memcpy(ctx->buffer + sizeof(uint16_t), seed, seedSize);

	// Initialize underlying HMAC context
	mbedtls_md_init(&ctx->hmacCtx);
	mbedtls_md_setup(&ctx->hmacCtx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
	mbedtls_md_hmac_starts(&ctx->hmacCtx, hmacKey, hmacKeySize);
}

void nfc3d_drbg_step(nfc3d_drbg_ctx * ctx, uint8_t * output) {
	assert(ctx != NULL);
	assert(output != NULL);

	if (ctx->used) {
		// If used at least once, reinitialize the HMAC
		mbedtls_md_hmac_reset(&ctx->hmacCtx);
	} else {
		ctx->used = true;
	}

	// Store counter in big endian, and increment it
	ctx->buffer[0] = ctx->iteration >> 8;
	ctx->buffer[1] = ctx->iteration >> 0;
	ctx->iteration++;

	// Do HMAC magic
	mbedtls_md_hmac_update(&ctx->hmacCtx, ctx->buffer, ctx->bufferSize);
	mbedtls_md_hmac_finish(&ctx->hmacCtx, output);
}

void nfc3d_drbg_cleanup(nfc3d_drbg_ctx * ctx) {
	assert(ctx != NULL);
	mbedtls_md_free(&ctx->hmacCtx);
}

void nfc3d_drbg_generate_bytes(const uint8_t * hmacKey, size_t hmacKeySize, const uint8_t * seed, size_t seedSize, uint8_t * output, size_t outputSize) {
	uint8_t temp[NFC3D_DRBG_OUTPUT_SIZE];

	nfc3d_drbg_ctx rngCtx;
	nfc3d_drbg_init(&rngCtx, hmacKey, hmacKeySize, seed, seedSize);

	while (outputSize > 0) {
		if (outputSize < NFC3D_DRBG_OUTPUT_SIZE) {
			nfc3d_drbg_step(&rngCtx, temp);
			memcpy(output, temp, outputSize);
			break;
		}

		nfc3d_drbg_step(&rngCtx, output);
		output += NFC3D_DRBG_OUTPUT_SIZE;
		outputSize -= NFC3D_DRBG_OUTPUT_SIZE;
	}

	nfc3d_drbg_cleanup(&rngCtx);
}
