#include "aes_cmac.h"

#define BLOCK_SIZE 16

#define TAG "AESCMAC"

static uint8_t zeroes[] =
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static uint8_t Rb[] =
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x87};

void aes_cmac_padBlock(uint8_t* block, size_t len) {
    block[len] = 0x80;
}

bool aes_cmac_aes(uint8_t* key, uint8_t* plain, size_t plain_len, uint8_t* enc) {
    uint8_t iv[BLOCK_SIZE];
    memset(iv, 0, BLOCK_SIZE);
    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);
    mbedtls_aes_setkey_enc(&ctx, key, BLOCK_SIZE * 8);
    int rtn = mbedtls_aes_crypt_cbc(&ctx, MBEDTLS_AES_ENCRYPT, plain_len, iv, plain, enc);
    mbedtls_aes_free(&ctx);

    return rtn == 0;
}

void aes_cmac_bitShiftLeft(uint8_t* input, uint8_t* output, size_t len) {
    size_t last = len - 1;
    for(size_t i = 0; i < last; i++) {
        output[i] = input[i] << 1;
        if(input[i + 1] & 0x80) {
            output[i] += 0x01;
        }
    }
    output[last] = input[last] << 1;
}

// x = a ^ b
void aes_cmac_xor(uint8_t* a, uint8_t* b, uint8_t* x, size_t len) {
    for(size_t i = 0; i < len; i++) {
        x[i] = a[i] ^ b[i];
    }
}

bool aes_cmac_generateSubkeys(uint8_t* key, uint8_t* subkey1, uint8_t* subkey2) {
    uint8_t l[BLOCK_SIZE] = {0};
    aes_cmac_aes(key, zeroes, BLOCK_SIZE, l);

    aes_cmac_bitShiftLeft(l, subkey1, BLOCK_SIZE);
    if(l[0] & 0x80) {
        aes_cmac_xor(subkey1, Rb, subkey1, BLOCK_SIZE);
    }

    aes_cmac_bitShiftLeft(subkey1, subkey2, BLOCK_SIZE);
    if(subkey1[0] & 0x80) {
        aes_cmac_xor(subkey2, Rb, subkey2, BLOCK_SIZE);
    }

    return true;
}

bool aes_cmac(uint8_t* key, size_t key_len, uint8_t* message, size_t message_len, uint8_t* cmac) {
    uint8_t subkey1[BLOCK_SIZE] = {0};
    uint8_t subkey2[BLOCK_SIZE] = {0};
    uint8_t blockCount = (message_len + BLOCK_SIZE - 1) / BLOCK_SIZE;
    bool lastBlockCompleteFlag;
    uint8_t lastBlockIndex;
    uint8_t lastBlock[BLOCK_SIZE] = {0};

    // Only support key length of 16 bytes
    if(key_len != BLOCK_SIZE) {
        return false;
    }

    aes_cmac_generateSubkeys(key, subkey1, subkey2);

    if(blockCount == 0) {
        blockCount = 1;
        lastBlockCompleteFlag = false;
    } else {
        lastBlockCompleteFlag = (message_len % BLOCK_SIZE == 0);
    }
    lastBlockIndex = blockCount - 1;

    if(lastBlockCompleteFlag) {
        memcpy(lastBlock, message + (lastBlockIndex * BLOCK_SIZE), BLOCK_SIZE);
        aes_cmac_xor(lastBlock, subkey1, lastBlock, BLOCK_SIZE);
    } else {
        memcpy(lastBlock, message + (lastBlockIndex * BLOCK_SIZE), message_len % BLOCK_SIZE);
        aes_cmac_padBlock(lastBlock, message_len % BLOCK_SIZE);
        aes_cmac_xor(lastBlock, subkey2, lastBlock, BLOCK_SIZE);
    }

    uint8_t x[BLOCK_SIZE];
    uint8_t y[BLOCK_SIZE];
    memset(x, 0, sizeof(x));
    memset(y, 0, sizeof(y));

    for(size_t i = 0; i < lastBlockIndex; i++) {
        aes_cmac_xor(x, message + (i * BLOCK_SIZE), y, BLOCK_SIZE);
        aes_cmac_aes(key, y, BLOCK_SIZE, x);
    }

    aes_cmac_xor(x, lastBlock, y, BLOCK_SIZE);

    bool success = aes_cmac_aes(key, y, BLOCK_SIZE, cmac);

    return success;
}
