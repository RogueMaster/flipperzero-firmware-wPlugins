#include "des_cmac.h"
#include "dfc_crypto.h"

#define BLOCK_SIZE 8

#define TAG "DESCMAC"

static uint8_t zeroes[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static uint8_t Rb[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1b};

void des_cmac_padBlock(uint8_t* block, size_t len) {
    block[len] = 0x80;
}

// CMAC's underlying block cipher, selected by key length: 8 bytes = single DES,
// 16 bytes = 2-key 3DES, 24 bytes = 3-key 3DES.
bool des_cmac_block_cipher(
    uint8_t* key,
    size_t key_len,
    uint8_t* plain,
    size_t plain_len,
    uint8_t* enc) {
    uint8_t iv[BLOCK_SIZE];
    memset(iv, 0, BLOCK_SIZE);
    return dfc_crypto_des_cbc(true, key, key_len, iv, plain, enc, plain_len);
}

void des_cmac_bitShiftLeft(uint8_t* input, uint8_t* output, size_t len) {
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
void des_cmac_xor(uint8_t* a, uint8_t* b, uint8_t* x, size_t len) {
    for(size_t i = 0; i < len; i++) {
        x[i] = a[i] ^ b[i];
    }
}

bool des_cmac_generateSubkeys(uint8_t* key, size_t key_len, uint8_t* subkey1, uint8_t* subkey2) {
    uint8_t l[BLOCK_SIZE] = {0};
    des_cmac_block_cipher(key, key_len, zeroes, BLOCK_SIZE, l);

    des_cmac_bitShiftLeft(l, subkey1, BLOCK_SIZE);
    if(l[0] & 0x80) {
        des_cmac_xor(subkey1, Rb, subkey1, BLOCK_SIZE);
    }

    des_cmac_bitShiftLeft(subkey1, subkey2, BLOCK_SIZE);
    if(subkey1[0] & 0x80) {
        des_cmac_xor(subkey2, Rb, subkey2, BLOCK_SIZE);
    }

    return true;
}

bool des_cmac_with_iv(
    uint8_t* key,
    size_t key_len,
    uint8_t* message,
    size_t message_len,
    uint8_t* iv,
    uint8_t* cmac) {
    uint8_t subkey1[BLOCK_SIZE] = {0};
    uint8_t subkey2[BLOCK_SIZE] = {0};
    uint8_t blockCount = (message_len + BLOCK_SIZE - 1) / BLOCK_SIZE;
    bool lastBlockCompleteFlag;
    uint8_t lastBlockIndex;
    uint8_t lastBlock[BLOCK_SIZE] = {0};

    if(key_len != 8 && key_len != 16 && key_len != 24) {
        return false;
    }

    des_cmac_generateSubkeys(key, key_len, subkey1, subkey2);

    if(blockCount == 0) {
        blockCount = 1;
        lastBlockCompleteFlag = false;
    } else {
        lastBlockCompleteFlag = (message_len % BLOCK_SIZE == 0);
    }
    lastBlockIndex = blockCount - 1;

    if(lastBlockCompleteFlag) {
        memcpy(lastBlock, message + (lastBlockIndex * BLOCK_SIZE), BLOCK_SIZE);
        des_cmac_xor(lastBlock, subkey1, lastBlock, BLOCK_SIZE);
    } else {
        // An empty message has no bytes to copy, and may arrive as a NULL pointer.
        size_t partial = message_len % BLOCK_SIZE;
        if(partial > 0) memcpy(lastBlock, message + (lastBlockIndex * BLOCK_SIZE), partial);
        des_cmac_padBlock(lastBlock, partial);
        des_cmac_xor(lastBlock, subkey2, lastBlock, BLOCK_SIZE);
    }

    uint8_t x[BLOCK_SIZE];
    uint8_t y[BLOCK_SIZE];
    memcpy(x, iv, sizeof(x));
    memset(y, 0, sizeof(y));

    for(size_t i = 0; i < lastBlockIndex; i++) {
        des_cmac_xor(x, message + (i * BLOCK_SIZE), y, BLOCK_SIZE);
        des_cmac_block_cipher(key, key_len, y, BLOCK_SIZE, x);
    }

    des_cmac_xor(x, lastBlock, y, BLOCK_SIZE);

    bool success = des_cmac_block_cipher(key, key_len, y, BLOCK_SIZE, cmac);

    return success;
}

bool des_cmac(uint8_t* key, size_t key_len, uint8_t* message, size_t message_len, uint8_t* cmac) {
    uint8_t iv[BLOCK_SIZE] = {0};
    return des_cmac_with_iv(key, key_len, message, message_len, iv, cmac);
}
