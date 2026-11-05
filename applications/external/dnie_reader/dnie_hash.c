#include "dnie_hash.h"

#include <string.h>

size_t dnie_hash_len(DnieHashAlg alg) {
    switch(alg) {
    case DnieHashSha1:
        return 20;
    case DnieHashSha224:
        return 28;
    case DnieHashSha256:
        return 32;
    case DnieHashSha384:
        return 48;
    case DnieHashSha512:
        return 64;
    default:
        return 0;
    }
}

bool dnie_hash_supported(DnieHashAlg alg) {
    return alg == DnieHashSha1 || alg == DnieHashSha224 || alg == DnieHashSha256;
}

const char* dnie_hash_name(DnieHashAlg alg) {
    switch(alg) {
    case DnieHashSha1:
        return "SHA-1";
    case DnieHashSha224:
        return "SHA-224";
    case DnieHashSha256:
        return "SHA-256";
    case DnieHashSha384:
        return "SHA-384";
    case DnieHashSha512:
        return "SHA-512";
    default:
        return "?";
    }
}

bool dnie_hash_start(DnieHash* h, DnieHashAlg alg) {
    memset(h, 0, sizeof(*h));
    h->alg = alg;
    if(alg == DnieHashSha1) {
        mbedtls_sha1_init(&h->ctx.sha1);
        return mbedtls_sha1_starts(&h->ctx.sha1) == 0;
    }
    if(alg == DnieHashSha224 || alg == DnieHashSha256) {
        mbedtls_sha256_init(&h->ctx.sha256);
        return mbedtls_sha256_starts(&h->ctx.sha256, alg == DnieHashSha224) == 0;
    }
    return false;
}

void dnie_hash_update(DnieHash* h, const uint8_t* data, size_t len) {
    if(h->alg == DnieHashSha1) {
        mbedtls_sha1_update(&h->ctx.sha1, data, len);
    } else if(h->alg == DnieHashSha224 || h->alg == DnieHashSha256) {
        mbedtls_sha256_update(&h->ctx.sha256, data, len);
    }
}

bool dnie_hash_finish(DnieHash* h, uint8_t out[DNIE_HASH_MAX]) {
    bool ok = false;
    if(h->alg == DnieHashSha1) {
        ok = mbedtls_sha1_finish(&h->ctx.sha1, out) == 0;
        mbedtls_sha1_free(&h->ctx.sha1);
    } else if(h->alg == DnieHashSha224 || h->alg == DnieHashSha256) {
        ok = mbedtls_sha256_finish(&h->ctx.sha256, out) == 0;
        mbedtls_sha256_free(&h->ctx.sha256);
    }
    h->alg = DnieHashUnknown;
    return ok;
}

bool dnie_hash(DnieHashAlg alg, const uint8_t* data, size_t len, uint8_t out[DNIE_HASH_MAX]) {
    DnieHash h;
    if(!dnie_hash_start(&h, alg)) return false;
    dnie_hash_update(&h, data, len);
    return dnie_hash_finish(&h, out);
}
