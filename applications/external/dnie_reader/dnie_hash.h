#pragma once

// Hash en streaming (SHA-1 / SHA-224 / SHA-256: los que trae el mbedtls del firmware)

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <mbedtls/sha1.h>
#include <mbedtls/sha256.h>

#define DNIE_HASH_MAX 32

typedef enum {
    DnieHashUnknown,
    DnieHashSha1,
    DnieHashSha224,
    DnieHashSha256,
    DnieHashSha384, // reconocido pero no disponible en el firmware
    DnieHashSha512,
} DnieHashAlg;

typedef struct {
    DnieHashAlg alg;
    union {
        mbedtls_sha1_context sha1;
        mbedtls_sha256_context sha256;
    } ctx;
} DnieHash;

size_t dnie_hash_len(DnieHashAlg alg);
bool dnie_hash_supported(DnieHashAlg alg);
const char* dnie_hash_name(DnieHashAlg alg);

bool dnie_hash_start(DnieHash* h, DnieHashAlg alg);
void dnie_hash_update(DnieHash* h, const uint8_t* data, size_t len);
bool dnie_hash_finish(DnieHash* h, uint8_t out[DNIE_HASH_MAX]);

// Hash de un buffer completo
bool dnie_hash(DnieHashAlg alg, const uint8_t* data, size_t len, uint8_t out[DNIE_HASH_MAX]);
