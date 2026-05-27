/*
 * ZeroFIDO
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 or later.
 */

#include "hmac_sha256.h"

#include <string.h>

static void zf_hmac_secure_zero(void *data, size_t size) {
    volatile uint8_t *ptr = data;

    if (!ptr) {
        return;
    }

    while (size-- > 0U) {
        *ptr++ = 0;
    }
}

bool zf_hmac_sha256_parts_with_scratch(ZfHmacSha256Scratch *scratch, const uint8_t *key,
                                       size_t key_len, const uint8_t *first, size_t first_size,
                                       const uint8_t *second, size_t second_size, uint8_t out[32]) {
    if (!scratch || !key || !out || (first_size > 0U && !first) || (second_size > 0U && !second)) {
        return false;
    }

    memset(scratch, 0, sizeof(*scratch));

    if (key_len > sizeof(scratch->key_block)) {
        zf_sha256_init(&scratch->sha);
        zf_sha256_update(&scratch->sha, key, key_len);
        zf_sha256_finish(&scratch->sha, scratch->key_block);
    } else if (key_len > 0U) {
        memcpy(scratch->key_block, key, key_len);
    }

    for (size_t i = 0; i < sizeof(scratch->pad); ++i) {
        scratch->pad[i] = scratch->key_block[i] ^ 0x36U;
    }
    zf_sha256_init(&scratch->sha);
    zf_sha256_update(&scratch->sha, scratch->pad, sizeof(scratch->pad));
    if (first_size > 0U) {
        zf_sha256_update(&scratch->sha, first, first_size);
    }
    if (second_size > 0U) {
        zf_sha256_update(&scratch->sha, second, second_size);
    }
    zf_sha256_finish(&scratch->sha, scratch->inner_hash);

    for (size_t i = 0; i < sizeof(scratch->pad); ++i) {
        scratch->pad[i] = scratch->key_block[i] ^ 0x5CU;
    }
    zf_sha256_init(&scratch->sha);
    zf_sha256_update(&scratch->sha, scratch->pad, sizeof(scratch->pad));
    zf_sha256_update(&scratch->sha, scratch->inner_hash, sizeof(scratch->inner_hash));
    zf_sha256_finish(&scratch->sha, out);

    zf_hmac_secure_zero(scratch, sizeof(*scratch));
    return true;
}

bool zf_hmac_sha256_parts(const uint8_t *key, size_t key_len, const uint8_t *first,
                          size_t first_size, const uint8_t *second, size_t second_size,
                          uint8_t out[32]) {
    ZfHmacSha256Scratch scratch;

    return zf_hmac_sha256_parts_with_scratch(&scratch, key, key_len, first, first_size, second,
                                             second_size, out);
}

bool zf_hmac_sha256(const uint8_t *key, size_t key_len, const uint8_t *data, size_t size,
                    uint8_t out[32]) {
    return zf_hmac_sha256_parts(key, key_len, data, size, NULL, 0, out);
}

bool zf_hkdf_sha256(const uint8_t *salt, size_t salt_len, const uint8_t *ikm, size_t ikm_len,
                    const uint8_t *info, size_t info_len, uint8_t out[32]) {
    uint8_t prk[32];
    uint8_t info_counter[64];
    bool ok = false;

    if (!salt || !ikm || !info || !out || info_len + 1U > sizeof(info_counter)) {
        return false;
    }

    memcpy(info_counter, info, info_len);
    info_counter[info_len] = 1U;

    if (!zf_hmac_sha256(salt, salt_len, ikm, ikm_len, prk)) {
        goto cleanup;
    }
    if (!zf_hmac_sha256(prk, sizeof(prk), info_counter, info_len + 1U, out)) {
        goto cleanup;
    }
    ok = true;

cleanup:
    zf_hmac_secure_zero(prk, sizeof(prk));
    zf_hmac_secure_zero(info_counter, sizeof(info_counter));
    return ok;
}
