#include "cmac.h"

#include <string.h>

#include <mbedtls/aes.h>
#include <mbedtls/des.h>

/* mbedTLS ships a CMAC implementation, but the Flipper's build of it does not:
 * cmac.c is not compiled and its header is not shipped with the SDK. Neither
 * does furi_hal_crypto, which offers AES-CTR and AES-GCM but no MAC over a
 * caller-supplied key and no 3DES at all. So the algorithm lives here. */

#define CMAC_MAX_BLOCK 16
#define CMAC_KEY_LEN   16

/* Encrypts whole blocks in CBC mode from a zero IV. */
typedef bool (*CmacCipher)(uint8_t* key, const uint8_t* plain, size_t len, uint8_t* enc);

static bool cmac_aes_cbc(uint8_t* key, const uint8_t* plain, size_t len, uint8_t* enc) {
    uint8_t iv[16];
    memset(iv, 0, sizeof(iv));

    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);
    int rtn = mbedtls_aes_setkey_enc(&ctx, key, CMAC_KEY_LEN * 8);
    if(rtn == 0) rtn = mbedtls_aes_crypt_cbc(&ctx, MBEDTLS_AES_ENCRYPT, len, iv, plain, enc);
    mbedtls_aes_free(&ctx);
    return rtn == 0;
}

static bool cmac_des_cbc(uint8_t* key, const uint8_t* plain, size_t len, uint8_t* enc) {
    uint8_t iv[8];
    memset(iv, 0, sizeof(iv));

    mbedtls_des3_context ctx;
    mbedtls_des3_init(&ctx);
    int rtn = mbedtls_des3_set2key_enc(&ctx, key);
    if(rtn == 0) rtn = mbedtls_des3_crypt_cbc(&ctx, MBEDTLS_DES_ENCRYPT, len, iv, plain, enc);
    mbedtls_des3_free(&ctx);
    return rtn == 0;
}

/* Shifts a block left by one bit, big end first. */
static void shift_left(const uint8_t* input, uint8_t* output, size_t len) {
    for(size_t i = 0; i + 1 < len; i++) {
        output[i] = (uint8_t)(input[i] << 1);
        if(input[i + 1] & 0x80) output[i] |= 0x01;
    }
    output[len - 1] = (uint8_t)(input[len - 1] << 1);
}

static void block_xor(const uint8_t* a, const uint8_t* b, uint8_t* out, size_t len) {
    for(size_t i = 0; i < len; i++) {
        out[i] = a[i] ^ b[i];
    }
}

/* Derives the two subkeys from the cipher of an all-zero block. `rb` is the
 * constant for the block size: the low byte of the field polynomial. */
static bool subkeys(
    CmacCipher cipher,
    uint8_t* key,
    size_t block_size,
    uint8_t rb,
    uint8_t* k1,
    uint8_t* k2) {
    uint8_t zeroes[CMAC_MAX_BLOCK];
    uint8_t l[CMAC_MAX_BLOCK];
    uint8_t constant[CMAC_MAX_BLOCK];

    memset(zeroes, 0, block_size);
    memset(constant, 0, block_size);
    constant[block_size - 1] = rb;

    if(!cipher(key, zeroes, block_size, l)) return false;

    shift_left(l, k1, block_size);
    if(l[0] & 0x80) block_xor(k1, constant, k1, block_size);

    shift_left(k1, k2, block_size);
    if(k1[0] & 0x80) block_xor(k2, constant, k2, block_size);

    return true;
}

static bool cmac(
    CmacCipher cipher,
    size_t block_size,
    uint8_t rb,
    uint8_t* key,
    size_t key_len,
    const uint8_t* message,
    size_t message_len,
    uint8_t* out) {
    if(key_len != CMAC_KEY_LEN) return false;

    uint8_t k1[CMAC_MAX_BLOCK];
    uint8_t k2[CMAC_MAX_BLOCK];
    if(!subkeys(cipher, key, block_size, rb, k1, k2)) return false;

    /* An empty message still has one block, which takes the padded path. */
    size_t block_count = (message_len + block_size - 1) / block_size;
    bool last_block_whole = block_count > 0 && (message_len % block_size) == 0;
    if(block_count == 0) block_count = 1;
    size_t last_index = block_count - 1;

    uint8_t last[CMAC_MAX_BLOCK];
    memset(last, 0, block_size);
    if(last_block_whole) {
        memcpy(last, message + (last_index * block_size), block_size);
        block_xor(last, k1, last, block_size);
    } else {
        size_t tail = message_len % block_size;
        memcpy(last, message + (last_index * block_size), tail);
        last[tail] = 0x80;
        block_xor(last, k2, last, block_size);
    }

    uint8_t x[CMAC_MAX_BLOCK];
    uint8_t y[CMAC_MAX_BLOCK];
    memset(x, 0, block_size);

    for(size_t i = 0; i < last_index; i++) {
        block_xor(x, message + (i * block_size), y, block_size);
        if(!cipher(key, y, block_size, x)) return false;
    }

    block_xor(x, last, y, block_size);
    return cipher(key, y, block_size, out);
}

bool aes_cmac(uint8_t* key, size_t key_len, uint8_t* message, size_t message_len, uint8_t* out) {
    return cmac(cmac_aes_cbc, 16, 0x87, key, key_len, message, message_len, out);
}

bool des_cmac(uint8_t* key, size_t key_len, uint8_t* message, size_t message_len, uint8_t* out) {
    return cmac(cmac_des_cbc, 8, 0x1b, key, key_len, message, message_len, out);
}
