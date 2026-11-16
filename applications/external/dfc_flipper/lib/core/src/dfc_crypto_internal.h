#pragma once

#include "dfc_build_config.h"
#include "dfc_crypto.h"

#if defined(DFC_CRYPTO_BACKEND_TINY)
#include <tiny_crypto/aes.h>
#include <tiny_crypto/des.h>
#else
#include <mbedtls/aes.h>
#include <mbedtls/des.h>
#endif

typedef struct {
#if defined(DFC_CRYPTO_BACKEND_TINY)
    struct TC_AES_ctx context;
#else
    mbedtls_aes_context context;
#endif
    bool initialized;
} DfcCryptoAesBlock;

typedef struct {
#if defined(DFC_CRYPTO_BACKEND_TINY)
    struct TC_DES_ctx context;
#else
    union {
        mbedtls_des_context des;
        mbedtls_des3_context des3;
    } context;
#endif
    size_t key_len;
    bool encrypt;
    bool initialized;
} DfcCryptoDesBlock;

bool dfc_crypto_aes_block_init(DfcCryptoAesBlock* block, const uint8_t* key, size_t key_len);
bool dfc_crypto_aes_block_encrypt(
    DfcCryptoAesBlock* block,
    const uint8_t input[16],
    uint8_t output[16]);
void dfc_crypto_aes_block_clear(DfcCryptoAesBlock* block);

bool dfc_crypto_des_block_init(
    DfcCryptoDesBlock* block,
    bool encrypt,
    const uint8_t* key,
    size_t key_len);
bool dfc_crypto_des_block_crypt(
    DfcCryptoDesBlock* block,
    const uint8_t input[8],
    uint8_t output[8]);
void dfc_crypto_des_block_clear(DfcCryptoDesBlock* block);

typedef enum {
    DfcCryptoDesChainForward,
    DfcCryptoDesChainReverse,
} DfcCryptoDesChainDirection;

// Applies one prepared DES/TDEA primitive across a CBC-style chain. Forward
// computes C[i] = primitive(P[i] xor C[i-1]); reverse computes
// P[i] = primitive(C[i]) xor C[i-1]. Input and output may be the same buffer.
// On failure, the complete output is wiped.
bool dfc_crypto_des_chain(
    bool primitive_encrypt,
    DfcCryptoDesChainDirection direction,
    const uint8_t* key,
    size_t key_len,
    const uint8_t iv[8],
    const uint8_t* input,
    uint8_t* output,
    size_t length);
