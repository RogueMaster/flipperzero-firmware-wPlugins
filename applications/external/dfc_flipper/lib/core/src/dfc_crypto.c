#include "dfc_crypto.h"

#include <string.h>

/* Use mistial-dev/tiny-aes-c and mistial-dev/tiny-des-c with the feature
 * macros listed in the integration guide. */
#include <aes.h>
#include <des.h>

#if AES_KEYLEN != 16
#error "DFC requires tiny-aes-c configured for AES-128"
#endif

bool dfc_crypto_aes_cbc(
    bool encrypt,
    const uint8_t* key,
    size_t key_len,
    uint8_t iv[16],
    const uint8_t* input,
    uint8_t* output,
    size_t length) {
    if(key_len != 16 || length % AES_BLOCKLEN != 0) return false;

    memmove(output, input, length);
    struct AES_ctx ctx;
    AES_init_ctx_iv(&ctx, key, iv);
    int result = encrypt ? AES_CBC_encrypt(&ctx, output, length) :
                           AES_CBC_decrypt(&ctx, output, length);
    memcpy(iv, ctx.Iv, AES_BLOCKLEN);
    AES_ctx_clear(&ctx);
    return result == AES_OK;
}

bool dfc_crypto_des_cbc(
    bool encrypt,
    const uint8_t* key,
    size_t key_len,
    uint8_t iv[8],
    const uint8_t* input,
    uint8_t* output,
    size_t length) {
    if((key_len != 8 && key_len != 16 && key_len != 24) || length % DES_BLOCKLEN != 0) {
        return false;
    }

    memmove(output, input, length);
    if(key_len == 8) {
        struct DES_ctx ctx;
        DES_init_ctx_iv(&ctx, key, iv);
        int result = encrypt ? DES_CBC_encrypt(&ctx, output, length) :
                               DES_CBC_decrypt(&ctx, output, length);
        memcpy(iv, ctx.Iv, DES_BLOCKLEN);
        DES_ctx_clear(&ctx);
        return result == DES_OK;
    }

    struct DES3_ctx ctx;
    if(DES3_init_ctx_iv(&ctx, key, key_len, iv) != DES_OK) return false;
    int result = encrypt ? DES3_CBC_encrypt(&ctx, output, length) :
                           DES3_CBC_decrypt(&ctx, output, length);
    memcpy(iv, ctx.Iv, DES_BLOCKLEN);
    DES3_ctx_clear(&ctx);
    return result == DES_OK;
}

bool dfc_crypto_des_ecb(
    bool encrypt,
    const uint8_t* key,
    size_t key_len,
    const uint8_t input[8],
    uint8_t output[8]) {
    if(key_len != 8 && key_len != 16 && key_len != 24) return false;

    memcpy(output, input, DES_BLOCKLEN);
    if(key_len == 8) {
        struct DES_ctx ctx;
        DES_init_ctx(&ctx, key);
        if(encrypt) DES_ECB_encrypt(&ctx, output);
        else DES_ECB_decrypt(&ctx, output);
        DES_ctx_clear(&ctx);
        return true;
    }

    struct DES3_ctx ctx;
    if(DES3_init_ctx(&ctx, key, key_len) != DES_OK) return false;
    if(encrypt) DES3_ECB_encrypt(&ctx, output);
    else DES3_ECB_decrypt(&ctx, output);
    DES3_ctx_clear(&ctx);
    return true;
}
