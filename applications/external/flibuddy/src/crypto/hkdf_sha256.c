#include "hkdf_sha256.h"

#include <string.h>

bool fb_hkdf_sha256(
    const uint8_t* salt,
    size_t salt_len,
    const uint8_t* ikm,
    size_t ikm_len,
    const uint8_t* info,
    size_t info_len,
    uint8_t* out,
    size_t out_len) {
    if(out_len == 0 || out_len > FB_HKDF_SHA256_MAX_OUT) return false;

    /* HMAC zero-pads the key, so an empty salt already equals HashLen zeros. */
    uint8_t prk[FB_SHA256_DIGEST_LEN];
    fb_hmac_sha256(salt, salt_len, ikm, ikm_len, prk);

    uint8_t t[FB_SHA256_DIGEST_LEN];
    size_t t_len = 0;
    for(uint8_t counter = 1; out_len > 0; counter++) {
        FbHmacSha256 ctx;
        fb_hmac_sha256_init(&ctx, prk, sizeof(prk));
        fb_hmac_sha256_update(&ctx, t, t_len);
        if(info_len > 0) fb_hmac_sha256_update(&ctx, info, info_len);
        fb_hmac_sha256_update(&ctx, &counter, 1);
        fb_hmac_sha256_final(&ctx, t);
        t_len = sizeof(t);

        size_t take = out_len < t_len ? out_len : t_len;
        memcpy(out, t, take);
        out += take;
        out_len -= take;
    }

    memset(prk, 0, sizeof(prk));
    memset(t, 0, sizeof(t));
    return true;
}
