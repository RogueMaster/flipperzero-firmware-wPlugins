#include "wmbus_aes.h"
#include <string.h>

#ifndef WMBUS_AES_SOFT
#include <furi_hal_crypto.h>
#endif

void wmbus_aes_build_iv5(const WmbusCryptoIvCtx* ctx, uint8_t iv[16]) {
    /* OMS Vol.2 §9.2.4: IV = MMAAAAAAAAVVTTNN repeated to fill 16 bytes is
     * NOT exact — the actual definition is:
     *   IV[0..1]  = M field little-endian
     *   IV[2..9]  = A field (ID4 + Ver + Type) — 8 bytes
     *   IV[10..15] = AccessNo (8 times for short header; here we replicate). */
    iv[0] = (uint8_t)(ctx->m_field & 0xFF);
    iv[1] = (uint8_t)((ctx->m_field >> 8) & 0xFF);
    memcpy(&iv[2], ctx->a_field, 8);
    for(int i = 10; i < 16; i++) iv[i] = ctx->access_no;
}

bool wmbus_aes_verify_2f2f(const uint8_t* p, size_t len) {
    return len >= 2 && p[0] == 0x2F && p[1] == 0x2F;
}

#ifndef WMBUS_AES_SOFT

bool wmbus_aes_cbc_decrypt(
    const uint8_t key[16], const uint8_t iv[16],
    const uint8_t* in, size_t len, uint8_t* out) {
    if(len == 0 || (len & 0x0F)) return false;
    /* Ephemeral key+IV via furi_hal_crypto_load_key. The HW keeps it loaded
     * until the next load_key call or reset, so no explicit unload needed. */
    uint8_t iv_copy[16]; memcpy(iv_copy, iv, 16);
    if(!furi_hal_crypto_load_key((uint8_t*)key, iv_copy)) return false;
    return furi_hal_crypto_decrypt((uint8_t*)in, out, len);
}

#else /* software AES — only for host unit tests */

/* Minimal AES-128 software implementation (not used on device). */
/* (Omitted here for brevity; add when running unit tests off-target.) */
bool wmbus_aes_cbc_decrypt(
    const uint8_t key[16], const uint8_t iv[16],
    const uint8_t* in, size_t len, uint8_t* out) {
    (void)key; (void)iv; (void)in; (void)len; (void)out;
    return false;
}

#endif
