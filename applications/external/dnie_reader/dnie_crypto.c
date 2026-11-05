#include "dnie_crypto.h"

#include <string.h>
#include <mbedtls/sha1.h>
#include <mbedtls/aes.h>
#include <mbedtls/des.h>

bool dnie_kdf_aes128(const uint8_t* secret, size_t secret_len, uint32_t counter, uint8_t out[16]) {
    const uint8_t c[4] = {
        (uint8_t)(counter >> 24),
        (uint8_t)(counter >> 16),
        (uint8_t)(counter >> 8),
        (uint8_t)counter};
    uint8_t digest[20];
    mbedtls_sha1_context ctx;
    bool ok = false;

    mbedtls_sha1_init(&ctx);
    if(mbedtls_sha1_starts(&ctx) == 0 && mbedtls_sha1_update(&ctx, secret, secret_len) == 0 &&
       mbedtls_sha1_update(&ctx, c, sizeof(c)) == 0 && mbedtls_sha1_finish(&ctx, digest) == 0) {
        memcpy(out, digest, 16);
        ok = true;
    }
    mbedtls_sha1_free(&ctx);
    memset(digest, 0, sizeof(digest));
    return ok;
}

bool dnie_aes128_ecb_encrypt(const uint8_t key[16], const uint8_t in[16], uint8_t out[16]) {
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    bool ok = mbedtls_aes_setkey_enc(&aes, key, 128) == 0 &&
              mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, in, out) == 0;
    mbedtls_aes_free(&aes);
    return ok;
}

bool dnie_aes128_cbc(
    const uint8_t key[16],
    const uint8_t iv[16],
    bool encrypt,
    const uint8_t* in,
    size_t len,
    uint8_t* out) {
    if(len == 0 || len % 16 != 0) return false;
    uint8_t iv_copy[16];
    memcpy(iv_copy, iv, 16); // mbedtls actualiza el IV in situ
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    bool ok =
        (encrypt ? mbedtls_aes_setkey_enc(&aes, key, 128) :
                   mbedtls_aes_setkey_dec(&aes, key, 128)) == 0 &&
        mbedtls_aes_crypt_cbc(
            &aes, encrypt ? MBEDTLS_AES_ENCRYPT : MBEDTLS_AES_DECRYPT, len, iv_copy, in, out) == 0;
    mbedtls_aes_free(&aes);
    return ok;
}

// Subclaves de CMAC: K1 = L<<1 (^0x87 si se desborda), K2 = K1<<1 (idem)
static void cmac_dbl(const uint8_t in[16], uint8_t out[16]) {
    uint8_t carry = in[0] >> 7;
    for(int i = 0; i < 15; i++)
        out[i] = (uint8_t)((in[i] << 1) | (in[i + 1] >> 7));
    out[15] = (uint8_t)(in[15] << 1);
    if(carry) out[15] ^= 0x87;
}

bool dnie_aes128_cmac(const uint8_t key[16], const uint8_t* data, size_t len, uint8_t out[16]) {
    mbedtls_aes_context aes;
    uint8_t l[16] = {0}, k1[16], k2[16], x[16] = {0}, last[16];
    bool ok = false;

    mbedtls_aes_init(&aes);
    do {
        if(mbedtls_aes_setkey_enc(&aes, key, 128) != 0) break;
        if(mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, l, l) != 0) break;
        cmac_dbl(l, k1);
        cmac_dbl(k1, k2);

        size_t n = (len + 15) / 16;
        bool complete = (len > 0) && (len % 16 == 0);
        if(n == 0) n = 1;

        // Ultimo bloque: completo ^ K1, o con relleno 80 00.. ^ K2
        size_t last_off = (n - 1) * 16;
        size_t last_len = len - last_off;
        memset(last, 0, sizeof(last));
        if(last_len > 0) memcpy(last, data + last_off, last_len);
        if(complete) {
            for(int i = 0; i < 16; i++)
                last[i] ^= k1[i];
        } else {
            last[last_len] = 0x80;
            for(int i = 0; i < 16; i++)
                last[i] ^= k2[i];
        }

        bool err = false;
        for(size_t b = 0; b + 1 < n && !err; b++) {
            for(int i = 0; i < 16; i++)
                x[i] ^= data[b * 16 + i];
            err = mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, x, x) != 0;
        }
        if(err) break;
        for(int i = 0; i < 16; i++)
            x[i] ^= last[i];
        if(mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, x, out) != 0) break;
        ok = true;
    } while(0);

    mbedtls_aes_free(&aes);
    memset(l, 0, sizeof(l));
    memset(k1, 0, sizeof(k1));
    memset(k2, 0, sizeof(k2));
    return ok;
}

bool dnie_aes128_cbc_decrypt_iv0(
    const uint8_t key[16],
    const uint8_t* in,
    size_t len,
    uint8_t* out) {
    if(len == 0 || len % 16 != 0) return false;
    uint8_t iv[16] = {0};
    mbedtls_aes_context aes;
    bool ok = false;

    mbedtls_aes_init(&aes);
    if(mbedtls_aes_setkey_dec(&aes, key, 128) == 0 &&
       mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, len, iv, in, out) == 0) {
        ok = true;
    }
    mbedtls_aes_free(&aes);
    return ok;
}

// ---------------------------------------------------------------------------
// 3DES
// ---------------------------------------------------------------------------

bool dnie_kdf_3des(const uint8_t* secret, size_t secret_len, uint32_t counter, uint8_t out[16]) {
    const uint8_t c[4] = {
        (uint8_t)(counter >> 24),
        (uint8_t)(counter >> 16),
        (uint8_t)(counter >> 8),
        (uint8_t)counter};
    uint8_t digest[20];
    mbedtls_sha1_context ctx;
    bool ok = false;
    mbedtls_sha1_init(&ctx);
    if(mbedtls_sha1_starts(&ctx) == 0 && mbedtls_sha1_update(&ctx, secret, secret_len) == 0 &&
       mbedtls_sha1_update(&ctx, c, sizeof(c)) == 0 && mbedtls_sha1_finish(&ctx, digest) == 0) {
        memcpy(out, digest, 16);
        mbedtls_des_key_set_parity(
            out); // paridad impar en cada byte (DES la ignora, pero ICAO la fija)
        mbedtls_des_key_set_parity(out + 8);
        ok = true;
    }
    mbedtls_sha1_free(&ctx);
    memset(digest, 0, sizeof(digest));
    return ok;
}

bool dnie_3des_cbc_iv0(
    const uint8_t key[16],
    bool encrypt,
    const uint8_t* in,
    size_t len,
    uint8_t* out) {
    if(len == 0 || len % 8 != 0) return false;
    uint8_t iv[8] = {0};
    mbedtls_des3_context ctx;
    mbedtls_des3_init(&ctx);
    bool ok =
        (encrypt ? mbedtls_des3_set2key_enc(&ctx, key) : mbedtls_des3_set2key_dec(&ctx, key)) ==
            0 &&
        mbedtls_des3_crypt_cbc(
            &ctx, encrypt ? MBEDTLS_DES_ENCRYPT : MBEDTLS_DES_DECRYPT, len, iv, in, out) == 0;
    mbedtls_des3_free(&ctx);
    return ok;
}

bool dnie_retail_mac(const uint8_t key[16], const uint8_t* data, size_t len, uint8_t out[8]) {
    if(len == 0 || len % 8 != 0) return false;
    mbedtls_des_context k1_enc, k2_dec;
    mbedtls_des_init(&k1_enc);
    mbedtls_des_init(&k2_dec);
    uint8_t y[8] = {0};
    bool ok = mbedtls_des_setkey_enc(&k1_enc, key) == 0 &&
              mbedtls_des_setkey_dec(&k2_dec, key + 8) == 0;
    // CBC con K1 sobre todos los bloques
    for(size_t i = 0; ok && i < len; i += 8) {
        for(int b = 0; b < 8; b++)
            y[b] ^= data[i + b];
        ok = mbedtls_des_crypt_ecb(&k1_enc, y, y) == 0;
    }
    // Transformacion final: descifrar con K2 y volver a cifrar con K1
    ok = ok && mbedtls_des_crypt_ecb(&k2_dec, y, y) == 0 &&
         mbedtls_des_crypt_ecb(&k1_enc, y, out) == 0;
    mbedtls_des_free(&k1_enc);
    mbedtls_des_free(&k2_dec);
    return ok;
}
