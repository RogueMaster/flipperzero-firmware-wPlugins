/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 */

#include "emrtd_mac.h"

#include <string.h>

#include <mbedtls/aes.h>
#include <mbedtls/des.h>

size_t emrtd_padded_len(size_t len, size_t block) {
    return ((len / block) + 1) * block;
}

size_t emrtd_pad_iso9797_m2(uint8_t* buf, size_t len, size_t block) {
    const size_t padded = emrtd_padded_len(len, block);
    buf[len] = 0x80;
    memset(buf + len + 1, 0x00, padded - len - 1);
    return padded;
}

bool emrtd_unpad_iso9797_m2(const uint8_t* data, size_t len, size_t* out_len) {
    size_t i = len;
    while(i > 0 && data[i - 1] == 0x00) {
        i--;
    }
    if(i == 0 || data[i - 1] != 0x80) {
        return false;
    }
    *out_len = i - 1;
    return true;
}

bool emrtd_retail_mac(const uint8_t key[16], const uint8_t* data, size_t len, uint8_t mac[8]) {
    if(len % 8 != 0) {
        return false;
    }

    mbedtls_des_context enc_k1;
    mbedtls_des_context dec_k2;
    mbedtls_des_init(&enc_k1);
    mbedtls_des_init(&dec_k2);
    mbedtls_des_setkey_enc(&enc_k1, key);
    mbedtls_des_setkey_dec(&dec_k2, key + 8);

    /* CBC-MAC under K1 with a zero IV. */
    uint8_t h[8];
    uint8_t block[8];
    memset(h, 0, sizeof(h));
    for(size_t offset = 0; offset < len; offset += 8) {
        for(size_t i = 0; i < 8; i++) {
            block[i] = data[offset + i] ^ h[i];
        }
        mbedtls_des_crypt_ecb(&enc_k1, block, h);
    }

    /* Output transformation 3: e_K1( d_K2( H ) ). */
    mbedtls_des_crypt_ecb(&dec_k2, h, block);
    mbedtls_des_crypt_ecb(&enc_k1, block, mac);

    mbedtls_des_free(&enc_k1);
    mbedtls_des_free(&dec_k2);
    memset(h, 0, sizeof(h));
    memset(block, 0, sizeof(block));
    return true;
}

/* Left shift a 128 bit block by one, feeding in a zero (RFC 4493 section 2.3). */
static void emrtd_cmac_shift_left(const uint8_t in[16], uint8_t out[16]) {
    uint8_t carry = 0;
    for(size_t i = 16; i > 0; i--) {
        const uint8_t byte = in[i - 1];
        out[i - 1] = (uint8_t)((byte << 1) | carry);
        carry = (byte >> 7) & 0x01;
    }
}

bool emrtd_aes_cmac(
    const uint8_t* key,
    size_t key_len,
    const uint8_t* data,
    size_t len,
    uint8_t* mac,
    size_t mac_len) {
    if(mac_len == 0 || mac_len > 16) {
        return false;
    }

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    if(mbedtls_aes_setkey_enc(&aes, key, (unsigned int)(key_len * 8)) != 0) {
        mbedtls_aes_free(&aes);
        return false;
    }

    /* Subkey generation: L = E(K, 0), K1 = L<<1 [^Rb], K2 = K1<<1 [^Rb]. */
    uint8_t zero[16];
    uint8_t l[16];
    uint8_t k1[16];
    uint8_t k2[16];
    memset(zero, 0, sizeof(zero));
    mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, zero, l);

    const uint8_t msb_l = l[0] & 0x80;
    emrtd_cmac_shift_left(l, k1);
    if(msb_l) {
        k1[15] ^= 0x87;
    }
    const uint8_t msb_k1 = k1[0] & 0x80;
    emrtd_cmac_shift_left(k1, k2);
    if(msb_k1) {
        k2[15] ^= 0x87;
    }

    const bool complete = (len != 0) && (len % 16 == 0);
    const size_t blocks = complete ? len / 16 : (len / 16) + 1;

    uint8_t x[16];
    uint8_t block[16];
    memset(x, 0, sizeof(x));

    for(size_t n = 0; n < blocks - 1; n++) {
        for(size_t i = 0; i < 16; i++) {
            block[i] = x[i] ^ data[n * 16 + i];
        }
        mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, block, x);
    }

    /* Last block: XOR with K1 when it is complete, otherwise pad and use K2. */
    const size_t last_offset = (blocks - 1) * 16;
    uint8_t last[16];
    if(complete) {
        memcpy(last, data + last_offset, 16);
        for(size_t i = 0; i < 16; i++) {
            last[i] ^= k1[i];
        }
    } else {
        const size_t rest = len - last_offset;
        memcpy(last, data + last_offset, rest);
        last[rest] = 0x80;
        memset(last + rest + 1, 0x00, 16 - rest - 1);
        for(size_t i = 0; i < 16; i++) {
            last[i] ^= k2[i];
        }
    }
    for(size_t i = 0; i < 16; i++) {
        block[i] = x[i] ^ last[i];
    }
    mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, block, x);

    memcpy(mac, x, mac_len);

    mbedtls_aes_free(&aes);
    memset(l, 0, sizeof(l));
    memset(k1, 0, sizeof(k1));
    memset(k2, 0, sizeof(k2));
    memset(x, 0, sizeof(x));
    memset(block, 0, sizeof(block));
    memset(last, 0, sizeof(last));
    return true;
}
