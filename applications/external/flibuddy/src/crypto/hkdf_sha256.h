/* HKDF-SHA256 (RFC 5869): extract with `salt`, expand with `info`. */
#pragma once

#include <stdbool.h>

#include "hmac_sha256.h"

#define FB_HKDF_SHA256_MAX_OUT (255 * FB_SHA256_DIGEST_LEN)

/* Writes exactly `out_len` bytes to `out`. Returns false, writing nothing,
 * when `out_len` is 0 or above FB_HKDF_SHA256_MAX_OUT. An empty salt
 * (NULL, 0) means HashLen zero bytes, as in the RFC. */
bool fb_hkdf_sha256(
    const uint8_t* salt,
    size_t salt_len,
    const uint8_t* ikm,
    size_t ikm_len,
    const uint8_t* info,
    size_t info_len,
    uint8_t* out,
    size_t out_len);
