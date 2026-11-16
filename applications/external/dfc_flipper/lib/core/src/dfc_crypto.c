#include "dfc_crypto_internal.h"

#include <string.h>

#if defined(DFC_CRYPTO_BACKEND_MBEDTLS) && defined(DFC_CRYPTO_BACKEND_TINY)
#error "Select only one DFC crypto backend"
#elif !defined(DFC_CRYPTO_BACKEND_MBEDTLS) && !defined(DFC_CRYPTO_BACKEND_TINY)
#define DFC_CRYPTO_BACKEND_MBEDTLS 1
#endif

#if defined(DFC_CRYPTO_BACKEND_MBEDTLS)

#include <mbedtls/aes.h>
#include <mbedtls/des.h>

#if DFC_ENABLE_AES_CRYPTO
bool dfc_crypto_aes_block_init(DfcCryptoAesBlock* block, const uint8_t* key, size_t key_len) {
    if(!block || !key || key_len != 16) return false;
    memset(block, 0, sizeof(*block));
    mbedtls_aes_init(&block->context);
    if(mbedtls_aes_setkey_enc(&block->context, key, 128) != 0) {
        mbedtls_aes_free(&block->context);
        memset(block, 0, sizeof(*block));
        return false;
    }
    block->initialized = true;
    return true;
}

bool dfc_crypto_aes_block_encrypt(
    DfcCryptoAesBlock* block,
    const uint8_t input[16],
    uint8_t output[16]) {
    return block && block->initialized && input && output &&
           mbedtls_aes_crypt_ecb(&block->context, MBEDTLS_AES_ENCRYPT, input, output) == 0;
}

void dfc_crypto_aes_block_clear(DfcCryptoAesBlock* block) {
    if(!block) return;
    if(block->initialized) mbedtls_aes_free(&block->context);
    memset(block, 0, sizeof(*block));
}
#endif

#if DFC_ENABLE_DES_CRYPTO
bool dfc_crypto_des_block_init(
    DfcCryptoDesBlock* block,
    bool encrypt,
    const uint8_t* key,
    size_t key_len) {
    if(!block || !key || (key_len != 8 && key_len != 16 && key_len != 24)) return false;
    memset(block, 0, sizeof(*block));
    block->key_len = key_len;
    block->encrypt = encrypt;
    int result;
    if(key_len == 8) {
        mbedtls_des_init(&block->context.des);
        result = encrypt ? mbedtls_des_setkey_enc(&block->context.des, key) :
                           mbedtls_des_setkey_dec(&block->context.des, key);
    } else {
        mbedtls_des3_init(&block->context.des3);
        if(key_len == 24) {
            result = encrypt ? mbedtls_des3_set3key_enc(&block->context.des3, key) :
                               mbedtls_des3_set3key_dec(&block->context.des3, key);
        } else {
            result = encrypt ? mbedtls_des3_set2key_enc(&block->context.des3, key) :
                               mbedtls_des3_set2key_dec(&block->context.des3, key);
        }
    }
    if(result != 0) {
        if(key_len == 8)
            mbedtls_des_free(&block->context.des);
        else
            mbedtls_des3_free(&block->context.des3);
        memset(block, 0, sizeof(*block));
        return false;
    }
    block->initialized = true;
    return true;
}

bool dfc_crypto_des_block_crypt(
    DfcCryptoDesBlock* block,
    const uint8_t input[8],
    uint8_t output[8]) {
    if(!block || !block->initialized || !input || !output) return false;
    if(block->key_len == 8) return mbedtls_des_crypt_ecb(&block->context.des, input, output) == 0;
    return mbedtls_des3_crypt_ecb(&block->context.des3, input, output) == 0;
}

void dfc_crypto_des_block_clear(DfcCryptoDesBlock* block) {
    if(!block) return;
    if(block->initialized) {
        if(block->key_len == 8)
            mbedtls_des_free(&block->context.des);
        else
            mbedtls_des3_free(&block->context.des3);
    }
    memset(block, 0, sizeof(*block));
}
#endif

#if DFC_ENABLE_AES_CRYPTO
bool dfc_crypto_aes_cbc(
    bool encrypt,
    const uint8_t* key,
    size_t key_len,
    uint8_t iv[16],
    const uint8_t* input,
    uint8_t* output,
    size_t length) {
    if(!key || !iv || key_len != 16 || length % 16 != 0 || (length != 0 && (!input || !output)))
        return false;

    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);
    int result = encrypt ? mbedtls_aes_setkey_enc(&ctx, key, 128) :
                           mbedtls_aes_setkey_dec(&ctx, key, 128);
    if(result == 0) {
        result = mbedtls_aes_crypt_cbc(
            &ctx, encrypt ? MBEDTLS_AES_ENCRYPT : MBEDTLS_AES_DECRYPT, length, iv, input, output);
    }
    mbedtls_aes_free(&ctx);
    return result == 0;
}
#endif

#if DFC_ENABLE_DES_CRYPTO
bool dfc_crypto_des_cbc(
    bool encrypt,
    const uint8_t* key,
    size_t key_len,
    uint8_t iv[8],
    const uint8_t* input,
    uint8_t* output,
    size_t length) {
    if(!key || !iv || (key_len != 8 && key_len != 16 && key_len != 24) || length % 8 != 0 ||
       (length != 0 && (!input || !output)))
        return false;

    DfcCryptoDesBlock block;
    if(!dfc_crypto_des_block_init(&block, encrypt, key, key_len)) return false;
    int result = key_len == 8 ? mbedtls_des_crypt_cbc(
                                    &block.context.des,
                                    encrypt ? MBEDTLS_DES_ENCRYPT : MBEDTLS_DES_DECRYPT,
                                    length,
                                    iv,
                                    input,
                                    output) :
                                mbedtls_des3_crypt_cbc(
                                    &block.context.des3,
                                    encrypt ? MBEDTLS_DES_ENCRYPT : MBEDTLS_DES_DECRYPT,
                                    length,
                                    iv,
                                    input,
                                    output);
    dfc_crypto_des_block_clear(&block);
    return result == 0;
}

bool dfc_crypto_des_ecb(
    bool encrypt,
    const uint8_t* key,
    size_t key_len,
    const uint8_t input[8],
    uint8_t output[8]) {
    DfcCryptoDesBlock block;
    if(!dfc_crypto_des_block_init(&block, encrypt, key, key_len)) return false;
    bool success = dfc_crypto_des_block_crypt(&block, input, output);
    dfc_crypto_des_block_clear(&block);
    return success;
}
#endif

#else

#include <tiny_crypto/aes.h>
#include <tiny_crypto/des.h>

#if DFC_ENABLE_AES_CRYPTO
bool dfc_crypto_aes_block_init(DfcCryptoAesBlock* block, const uint8_t* key, size_t key_len) {
    if(!block || !key || key_len != 16) return false;
    memset(block, 0, sizeof(*block));
    if(TC_AES_init(&block->context, (TC_bytes){key, key_len}) != TC_OK) {
        TC_AES_ctx_clear(&block->context);
        return false;
    }
    block->initialized = true;
    return true;
}

bool dfc_crypto_aes_block_encrypt(
    DfcCryptoAesBlock* block,
    const uint8_t input[16],
    uint8_t output[16]) {
    if(!block || !block->initialized || !input || !output) return false;
    uint8_t zero_iv[TC_AES_BLOCKLEN] = {0};
    if(TC_AES_set_iv(&block->context, (TC_bytes){zero_iv, sizeof(zero_iv)}) != TC_OK) return false;
    memmove(output, input, TC_AES_BLOCKLEN);
    return TC_AES_CBC_encrypt(&block->context, (TC_buffer){output, TC_AES_BLOCKLEN}) == TC_OK;
}

void dfc_crypto_aes_block_clear(DfcCryptoAesBlock* block) {
    if(!block) return;
    TC_AES_ctx_clear(&block->context);
    memset(block, 0, sizeof(*block));
}
#endif

#if DFC_ENABLE_DES_CRYPTO
bool dfc_crypto_des_block_init(
    DfcCryptoDesBlock* block,
    bool encrypt,
    const uint8_t* key,
    size_t key_len) {
    if(!block || !key || (key_len != 8 && key_len != 16 && key_len != 24)) return false;
    memset(block, 0, sizeof(*block));
    if(TC_DES_init(&block->context, (TC_bytes){key, key_len}) != TC_OK) {
        TC_DES_ctx_clear(&block->context);
        return false;
    }
    block->key_len = key_len;
    block->encrypt = encrypt;
    block->initialized = true;
    return true;
}

bool dfc_crypto_des_block_crypt(
    DfcCryptoDesBlock* block,
    const uint8_t input[8],
    uint8_t output[8]) {
    if(!block || !block->initialized || !input || !output) return false;
    memmove(output, input, TC_DES_BLOCKLEN);
    TC_status result =
        block->encrypt ?
            TC_DES_ECB_encrypt(&block->context, (TC_buffer){output, TC_DES_BLOCKLEN}) :
            TC_DES_ECB_decrypt(&block->context, (TC_buffer){output, TC_DES_BLOCKLEN});
    return result == TC_OK;
}

void dfc_crypto_des_block_clear(DfcCryptoDesBlock* block) {
    if(!block) return;
    TC_DES_ctx_clear(&block->context);
    memset(block, 0, sizeof(*block));
}
#endif

#if DFC_ENABLE_AES_CRYPTO
bool dfc_crypto_aes_cbc(
    bool encrypt,
    const uint8_t* key,
    size_t key_len,
    uint8_t iv[16],
    const uint8_t* input,
    uint8_t* output,
    size_t length) {
    if(!key || !iv || key_len != 16 || length % TC_AES_BLOCKLEN != 0 ||
       (length && (!input || !output))) {
        return false;
    }

    struct TC_AES_ctx ctx;
    TC_status result = TC_AES_init(&ctx, (TC_bytes){key, key_len});
    if(result == TC_OK) result = TC_AES_set_iv(&ctx, (TC_bytes){iv, TC_AES_BLOCKLEN});
    if(result == TC_OK) {
        if(length) memmove(output, input, length);
        result = encrypt ? TC_AES_CBC_encrypt(&ctx, (TC_buffer){output, length}) :
                           TC_AES_CBC_decrypt(&ctx, (TC_buffer){output, length});
    }
    if(result == TC_OK) memcpy(iv, ctx.iv, TC_AES_BLOCKLEN);
    TC_AES_ctx_clear(&ctx);
    return result == TC_OK;
}
#endif

#if DFC_ENABLE_DES_CRYPTO
bool dfc_crypto_des_cbc(
    bool encrypt,
    const uint8_t* key,
    size_t key_len,
    uint8_t iv[8],
    const uint8_t* input,
    uint8_t* output,
    size_t length) {
    if(!key || !iv || (key_len != 8 && key_len != 16 && key_len != 24) ||
       length % TC_DES_BLOCKLEN != 0 || (length && (!input || !output))) {
        return false;
    }

    DfcCryptoDesBlock block;
    if(!dfc_crypto_des_block_init(&block, encrypt, key, key_len)) return false;
    TC_status result = TC_DES_set_iv(&block.context, (TC_bytes){iv, TC_DES_BLOCKLEN});
    if(result == TC_OK) {
        if(length) memmove(output, input, length);
        result = encrypt ? TC_DES_CBC_encrypt(&block.context, (TC_buffer){output, length}) :
                           TC_DES_CBC_decrypt(&block.context, (TC_buffer){output, length});
    }
    if(result == TC_OK) memcpy(iv, block.context.iv, TC_DES_BLOCKLEN);
    dfc_crypto_des_block_clear(&block);
    return result == TC_OK;
}

bool dfc_crypto_des_ecb(
    bool encrypt,
    const uint8_t* key,
    size_t key_len,
    const uint8_t input[8],
    uint8_t output[8]) {
    DfcCryptoDesBlock block;
    if(!dfc_crypto_des_block_init(&block, encrypt, key, key_len)) return false;
    bool success = dfc_crypto_des_block_crypt(&block, input, output);
    dfc_crypto_des_block_clear(&block);
    return success;
}
#endif

#endif
