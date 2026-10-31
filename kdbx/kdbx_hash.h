#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef KDBX_PARSER_USE_HOST_HASH
#define KDBX_PARSER_USE_HOST_HASH 0
#endif

#if KDBX_PARSER_USE_HOST_HASH
#include "../flippass_crypto.h"
#else
#include "sha2.h"
#endif

static inline bool kdbx_hash_sha256(const uint8_t* data, size_t size, uint8_t out[32]) {
#if KDBX_PARSER_USE_HOST_HASH
    return flippass_crypto_sha256_raw(data, size, out);
#else
    sha256_Raw(data, size, out);
    return true;
#endif
}

static inline bool kdbx_hash_sha512(const uint8_t* data, size_t size, uint8_t out[64]) {
#if KDBX_PARSER_USE_HOST_HASH
    return flippass_crypto_sha512_raw(data, size, out);
#else
    sha512_Raw(data, size, out);
    return true;
#endif
}
