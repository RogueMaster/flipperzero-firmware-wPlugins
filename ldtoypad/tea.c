#include <furi.h>
#include "bytes.h"
#include "debug.h"

#define DELTA 0x9E3779B9

// Encipher function: core TEA encryption logic
void encipher(uint32_t* v, uint32_t* k, uint32_t* result) {
    uint32_t v0 = v[0], v1 = v[1];
    uint32_t sum = 0;

    for(int i = 0; i < 32; i++) {
        sum += DELTA;
        v0 += (((v1 << 4) + k[0]) ^ (v1 + sum) ^ ((v1 >> 5) + k[1]));
        v1 += (((v0 << 4) + k[2]) ^ (v0 + sum) ^ ((v0 >> 5) + k[3]));
    }

    result[0] = v0;
    result[1] = v1;
}

// Decipher function: core TEA decryption logic
void decipher(uint32_t* v, uint32_t* k, uint32_t* result) {
    uint32_t v0 = v[0], v1 = v[1];
    uint32_t sum = 0xC6EF3720;

    for(int i = 0; i < 32; i++) {
        v1 -= (((v0 << 4) + k[2]) ^ (v0 + sum) ^ ((v0 >> 5) + k[3]));
        v0 -= (((v1 << 4) + k[0]) ^ (v1 + sum) ^ ((v1 >> 5) + k[1]));
        sum -= DELTA;
    }

    result[0] = v0;
    result[1] = v1;
}

// Encryption function
void tea_encrypt(const uint8_t* buffer, const uint8_t* key, uint8_t* out) {
    if(!buffer || !key || !out) return;

    uint32_t v[2] = {readUInt32LE(buffer, 0), readUInt32LE(buffer, 4)};

    uint32_t k[4] = {
        readUInt32LE(key, 0), readUInt32LE(key, 4), readUInt32LE(key, 8), readUInt32LE(key, 12)};

    uint32_t result[2];
    encipher(v, k, result);

    writeUInt32LE(out, result[0], 0);
    writeUInt32LE(out, result[1], 4);
}

// Decryption function
void tea_decrypt(const uint8_t* buffer, const uint8_t* key, uint8_t* out) {
    furi_assert(key);

    uint32_t v[2] = {readUInt32LE(buffer, 0), readUInt32LE(buffer, 4)};

    uint32_t k[4] = {
        readUInt32LE(key, 0), readUInt32LE(key, 4), readUInt32LE(key, 8), readUInt32LE(key, 12)};

    // dechiper the buffer
    uint32_t result[2];
    decipher(v, k, result);

    if(!out || out == NULL) {
        set_debug_text("Error: output pointer is NULL");
        return;
    }

    writeUInt32LE(out, result[0], 0);
    writeUInt32LE(out, result[1], 4);
}
