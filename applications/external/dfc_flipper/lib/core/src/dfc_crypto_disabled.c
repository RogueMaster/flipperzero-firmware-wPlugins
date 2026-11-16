#include "dfc_build_config.h"

#include <string.h>

#include "aes_cmac.h"
#include "cmac_internal.h"
#include "des_cmac.h"
#include "dfc_common.h"
#include "dfc_crypto_internal.h"

// Keep the public surface linkable in a reduced profile. Unsupported cipher
// families fail before touching caller output and carry no primitive code.
#if !DFC_ENABLE_AES_CRYPTO
bool dfc_crypto_aes_block_init(DfcCryptoAesBlock* block, const uint8_t* key, size_t key_len) {
    (void)block;
    (void)key;
    (void)key_len;
    return false;
}

bool dfc_crypto_aes_block_encrypt(
    DfcCryptoAesBlock* block,
    const uint8_t input[16],
    uint8_t output[16]) {
    (void)block;
    (void)input;
    if(output) memset(output, 0, 16);
    return false;
}

void dfc_crypto_aes_block_clear(DfcCryptoAesBlock* block) {
    if(block) memset(block, 0, sizeof(*block));
}

bool dfc_crypto_aes_cbc(
    bool encrypt,
    const uint8_t* key,
    size_t key_len,
    uint8_t iv[16],
    const uint8_t* input,
    uint8_t* output,
    size_t length) {
    (void)encrypt;
    (void)key;
    (void)key_len;
    (void)iv;
    (void)input;
    (void)output;
    (void)length;
    return false;
}

bool dfc_worker_aes_cbc_decrypt(
    const uint8_t* key,
    size_t key_len,
    uint8_t iv[16],
    size_t length,
    const uint8_t* encrypted,
    uint8_t* clear) {
    return dfc_crypto_aes_cbc(false, key, key_len, iv, encrypted, clear, length);
}

bool dfc_worker_aes_cbc_encrypt(
    const uint8_t* key,
    size_t key_len,
    uint8_t iv[16],
    size_t length,
    const uint8_t* clear,
    uint8_t* encrypted) {
    return dfc_crypto_aes_cbc(true, key, key_len, iv, clear, encrypted, length);
}

bool dfc_aes_cmac_context_init(DfcAesCmacContext* context, const uint8_t* key, size_t key_len) {
    (void)key;
    (void)key_len;
    if(context) memset(context, 0, sizeof(*context));
    return false;
}

void dfc_aes_cmac_context_clear(DfcAesCmacContext* context) {
    if(context) memset(context, 0, sizeof(*context));
}

bool dfc_aes_cmac_context_compute(
    DfcAesCmacContext* context,
    const DfcCryptoSegment* segments,
    size_t segment_count,
    const uint8_t iv[16],
    DfcCmacFinalMode final_mode,
    uint8_t cmac[16]) {
    (void)context;
    (void)segments;
    (void)segment_count;
    (void)iv;
    (void)final_mode;
    if(cmac) memset(cmac, 0, 16);
    return false;
}

bool aes_cmac_segments_with_iv(
    uint8_t* key,
    size_t key_len,
    const DfcCryptoSegment* segments,
    size_t segment_count,
    uint8_t* iv,
    uint8_t* cmac) {
    (void)key;
    (void)key_len;
    (void)segments;
    (void)segment_count;
    (void)iv;
    if(cmac) memset(cmac, 0, 16);
    return false;
}

bool aes_cmac_with_iv(
    uint8_t* key,
    size_t key_len,
    uint8_t* message,
    size_t message_len,
    uint8_t* iv,
    uint8_t* cmac) {
    DfcCryptoSegment segment = {.data = message, .length = message_len};
    return aes_cmac_segments_with_iv(key, key_len, &segment, 1, iv, cmac);
}

bool aes_cmac(uint8_t* key, size_t key_len, uint8_t* message, size_t message_len, uint8_t* cmac) {
    uint8_t iv[16] = {0};
    return aes_cmac_with_iv(key, key_len, message, message_len, iv, cmac);
}
#endif

#if !DFC_ENABLE_DES_CRYPTO
bool dfc_crypto_des_block_init(
    DfcCryptoDesBlock* block,
    bool encrypt,
    const uint8_t* key,
    size_t key_len) {
    (void)block;
    (void)encrypt;
    (void)key;
    (void)key_len;
    return false;
}

bool dfc_crypto_des_block_crypt(
    DfcCryptoDesBlock* block,
    const uint8_t input[8],
    uint8_t output[8]) {
    (void)block;
    (void)input;
    if(output) memset(output, 0, 8);
    return false;
}

void dfc_crypto_des_block_clear(DfcCryptoDesBlock* block) {
    if(block) memset(block, 0, sizeof(*block));
}

bool dfc_crypto_des_cbc(
    bool encrypt,
    const uint8_t* key,
    size_t key_len,
    uint8_t iv[8],
    const uint8_t* input,
    uint8_t* output,
    size_t length) {
    (void)encrypt;
    (void)key;
    (void)key_len;
    (void)iv;
    (void)input;
    (void)output;
    (void)length;
    return false;
}

bool dfc_crypto_des_ecb(
    bool encrypt,
    const uint8_t* key,
    size_t key_len,
    const uint8_t input[8],
    uint8_t output[8]) {
    (void)encrypt;
    (void)key;
    (void)key_len;
    (void)input;
    (void)output;
    return false;
}

bool dfc_crypto_des_chain(
    bool primitive_encrypt,
    DfcCryptoDesChainDirection direction,
    const uint8_t* key,
    size_t key_len,
    const uint8_t iv[8],
    const uint8_t* input,
    uint8_t* output,
    size_t length) {
    (void)primitive_encrypt;
    (void)direction;
    (void)key;
    (void)key_len;
    (void)iv;
    (void)input;
    (void)output;
    (void)length;
    return false;
}

bool dfc_worker_des_cbc_decrypt(
    const uint8_t* key,
    size_t key_len,
    uint8_t iv[8],
    size_t length,
    const uint8_t* encrypted,
    uint8_t* clear) {
    return dfc_crypto_des_cbc(false, key, key_len, iv, encrypted, clear, length);
}

bool dfc_worker_des_cbc_encrypt(
    const uint8_t* key,
    size_t key_len,
    uint8_t iv[8],
    size_t length,
    const uint8_t* clear,
    uint8_t* encrypted) {
    return dfc_crypto_des_cbc(true, key, key_len, iv, clear, encrypted, length);
}

bool dfc_des_cmac_context_init(DfcDesCmacContext* context, const uint8_t* key, size_t key_len) {
    (void)key;
    (void)key_len;
    if(context) memset(context, 0, sizeof(*context));
    return false;
}

void dfc_des_cmac_context_clear(DfcDesCmacContext* context) {
    if(context) memset(context, 0, sizeof(*context));
}

bool dfc_des_cmac_context_compute(
    DfcDesCmacContext* context,
    const DfcCryptoSegment* segments,
    size_t segment_count,
    const uint8_t iv[8],
    DfcCmacFinalMode final_mode,
    uint8_t cmac[8]) {
    (void)context;
    (void)segments;
    (void)segment_count;
    (void)iv;
    (void)final_mode;
    if(cmac) memset(cmac, 0, 8);
    return false;
}

bool des_cmac_segments_with_iv(
    uint8_t* key,
    size_t key_len,
    const DfcCryptoSegment* segments,
    size_t segment_count,
    uint8_t* iv,
    uint8_t* cmac) {
    (void)key;
    (void)key_len;
    (void)segments;
    (void)segment_count;
    (void)iv;
    if(cmac) memset(cmac, 0, 8);
    return false;
}

bool des_cmac_with_iv(
    uint8_t* key,
    size_t key_len,
    uint8_t* message,
    size_t message_len,
    uint8_t* iv,
    uint8_t* cmac) {
    DfcCryptoSegment segment = {.data = message, .length = message_len};
    return des_cmac_segments_with_iv(key, key_len, &segment, 1, iv, cmac);
}

bool des_cmac(uint8_t* key, size_t key_len, uint8_t* message, size_t message_len, uint8_t* cmac) {
    uint8_t iv[8] = {0};
    return des_cmac_with_iv(key, key_len, message, message_len, iv, cmac);
}
#endif
