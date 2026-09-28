#include "dfc_crypto.h"

#include <string.h>

#if defined(DFC_CRYPTO_BACKEND_MBEDTLS) && defined(DFC_CRYPTO_BACKEND_TINY)
#error "Select only one DFC crypto backend"
#elif !defined(DFC_CRYPTO_BACKEND_MBEDTLS) && !defined(DFC_CRYPTO_BACKEND_TINY)
#define DFC_CRYPTO_BACKEND_MBEDTLS 1
#endif

#if defined(DFC_CRYPTO_BACKEND_MBEDTLS)

#include <mbedtls/aes.h>
#include <mbedtls/des.h>

bool dfc_crypto_aes_cbc(
    bool encrypt,
    const uint8_t* key,
    size_t key_len,
    uint8_t iv[16],
    const uint8_t* input,
    uint8_t* output,
    size_t length) {
    if(key_len != 16 || length % 16 != 0) return false;

    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);
    int result = encrypt ? mbedtls_aes_setkey_enc(&ctx, key, 128) :
                           mbedtls_aes_setkey_dec(&ctx, key, 128);
    if(result == 0) {
        result = mbedtls_aes_crypt_cbc(
            &ctx,
            encrypt ? MBEDTLS_AES_ENCRYPT : MBEDTLS_AES_DECRYPT,
            length,
            iv,
            input,
            output);
    }
    mbedtls_aes_free(&ctx);
    return result == 0;
}

bool dfc_crypto_des_cbc(
    bool encrypt,
    const uint8_t* key,
    size_t key_len,
    uint8_t iv[8],
    const uint8_t* input,
    uint8_t* output,
    size_t length) {
    if((key_len != 8 && key_len != 16 && key_len != 24) || length % 8 != 0) return false;

    int result;
    if(key_len == 8) {
        mbedtls_des_context ctx;
        mbedtls_des_init(&ctx);
        result = encrypt ? mbedtls_des_setkey_enc(&ctx, key) : mbedtls_des_setkey_dec(&ctx, key);
        if(result == 0) {
            result = mbedtls_des_crypt_cbc(
                &ctx,
                encrypt ? MBEDTLS_DES_ENCRYPT : MBEDTLS_DES_DECRYPT,
                length,
                iv,
                input,
                output);
        }
        mbedtls_des_free(&ctx);
        return result == 0;
    }

    mbedtls_des3_context ctx;
    mbedtls_des3_init(&ctx);
    if(key_len == 24) {
        result = encrypt ? mbedtls_des3_set3key_enc(&ctx, key) :
                           mbedtls_des3_set3key_dec(&ctx, key);
    } else {
        result = encrypt ? mbedtls_des3_set2key_enc(&ctx, key) :
                           mbedtls_des3_set2key_dec(&ctx, key);
    }
    if(result == 0) {
        result = mbedtls_des3_crypt_cbc(
            &ctx,
            encrypt ? MBEDTLS_DES_ENCRYPT : MBEDTLS_DES_DECRYPT,
            length,
            iv,
            input,
            output);
    }
    mbedtls_des3_free(&ctx);
    return result == 0;
}

bool dfc_crypto_des_ecb(
    bool encrypt,
    const uint8_t* key,
    size_t key_len,
    const uint8_t input[8],
    uint8_t output[8]) {
    if(key_len != 8 && key_len != 16 && key_len != 24) return false;

    int result;
    if(key_len == 8) {
        mbedtls_des_context ctx;
        mbedtls_des_init(&ctx);
        result = encrypt ? mbedtls_des_setkey_enc(&ctx, key) : mbedtls_des_setkey_dec(&ctx, key);
        if(result == 0) result = mbedtls_des_crypt_ecb(&ctx, input, output);
        mbedtls_des_free(&ctx);
        return result == 0;
    }

    mbedtls_des3_context ctx;
    mbedtls_des3_init(&ctx);
    if(key_len == 24) {
        result = encrypt ? mbedtls_des3_set3key_enc(&ctx, key) :
                           mbedtls_des3_set3key_dec(&ctx, key);
    } else {
        result = encrypt ? mbedtls_des3_set2key_enc(&ctx, key) :
                           mbedtls_des3_set2key_dec(&ctx, key);
    }
    if(result == 0) result = mbedtls_des3_crypt_ecb(&ctx, input, output);
    mbedtls_des3_free(&ctx);
    return result == 0;
}

#else

#include <tiny_crypto/aes.h>
#include <tiny_crypto/des.h>

bool dfc_crypto_aes_cbc(
    bool encrypt,
    const uint8_t* key,
    size_t key_len,
    uint8_t iv[16],
    const uint8_t* input,
    uint8_t* output,
    size_t length) {
    if(key_len != 16 || length % TC_AES_BLOCKLEN != 0) return false;

    memmove(output, input, length);
    struct TC_AES_ctx ctx;
    if(TC_AES_init_ctx_iv(&ctx, key, iv) != TC_OK) return false;
    TC_status result = encrypt ? TC_AES_CBC_encrypt(&ctx, output, length) :
                                 TC_AES_CBC_decrypt(&ctx, output, length);
    memcpy(iv, ctx.iv, TC_AES_BLOCKLEN);
    TC_AES_ctx_clear(&ctx);
    return result == TC_OK;
}

bool dfc_crypto_des_cbc(
    bool encrypt,
    const uint8_t* key,
    size_t key_len,
    uint8_t iv[8],
    const uint8_t* input,
    uint8_t* output,
    size_t length) {
    if((key_len != 8 && key_len != 16 && key_len != 24) || length % TC_DES_BLOCKLEN != 0) {
        return false;
    }

    memmove(output, input, length);
    if(key_len == 8) {
        struct TC_DES_ctx ctx;
        if(TC_DES_init_ctx_iv(&ctx, key, iv) != TC_OK) return false;
        TC_status result = encrypt ? TC_DES_CBC_encrypt(&ctx, output, length) :
                                     TC_DES_CBC_decrypt(&ctx, output, length);
        memcpy(iv, ctx.Iv, TC_DES_BLOCKLEN);
        TC_DES_ctx_clear(&ctx);
        return result == TC_OK;
    }

    struct TC_DES3_ctx ctx;
    if(TC_DES3_init_ctx_iv(&ctx, key, key_len, iv) != TC_OK) return false;
    TC_status result = encrypt ? TC_DES3_CBC_encrypt(&ctx, output, length) :
                                 TC_DES3_CBC_decrypt(&ctx, output, length);
    memcpy(iv, ctx.Iv, TC_DES_BLOCKLEN);
    TC_DES3_ctx_clear(&ctx);
    return result == TC_OK;
}

bool dfc_crypto_des_ecb(
    bool encrypt,
    const uint8_t* key,
    size_t key_len,
    const uint8_t input[8],
    uint8_t output[8]) {
    if(key_len != 8 && key_len != 16 && key_len != 24) return false;

    memcpy(output, input, TC_DES_BLOCKLEN);
    if(key_len == 8) {
        struct TC_DES_ctx ctx;
        if(TC_DES_init_ctx(&ctx, key) != TC_OK) return false;
        TC_status result = encrypt ? TC_DES_ECB_encrypt(&ctx, output) :
                                     TC_DES_ECB_decrypt(&ctx, output);
        TC_DES_ctx_clear(&ctx);
        return result == TC_OK;
    }

    struct TC_DES3_ctx ctx;
    if(TC_DES3_init_ctx(&ctx, key, key_len) != TC_OK) return false;
    TC_status result = encrypt ? TC_DES3_ECB_encrypt(&ctx, output) :
                                 TC_DES3_ECB_decrypt(&ctx, output);
    TC_DES3_ctx_clear(&ctx);
    return result == TC_OK;
}

#endif
