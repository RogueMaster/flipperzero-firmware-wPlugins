#include "dfc_emulator_i.h"

uint32_t dfc_emulator_read_uint24_le(const uint8_t* data) {
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16);
}

uint16_t dfc_emulator_crc16_iso14443(const uint8_t* data, size_t len) {
    uint16_t crc = 0x6363;
    for(size_t i = 0; i < len; i++) {
        uint8_t byte = data[i] ^ (crc & 0xFF);
        byte ^= byte << 4;
        crc = (crc >> 8) ^ ((uint16_t)byte << 8) ^ ((uint16_t)byte << 3) ^ ((uint16_t)byte >> 4);
    }
    return crc;
}

static void des_ecb_crypt(
    bool encrypt,
    const uint8_t* key,
    size_t key_len,
    const uint8_t input[8],
    uint8_t output[8]) {
    DFC_ASSERT(dfc_crypto_des_ecb(encrypt, key, key_len, input, output));
}

void dfc_emulator_d40_receive_plain(
    const uint8_t* key,
    size_t key_len,
    const uint8_t* encrypted,
    size_t encrypted_len,
    uint8_t* plain) {
    uint8_t previous[8] = {0};
    for(size_t offset = 0; offset < encrypted_len; offset += 8) {
        uint8_t block[8];
        des_ecb_crypt(true, key, key_len, encrypted + offset, block);
        for(size_t i = 0; i < 8; i++) {
            plain[offset + i] = block[i] ^ previous[i];
        }
        memcpy(previous, encrypted + offset, sizeof(previous));
    }
}

uint8_t dfc_emulator_des_key_version(const uint8_t* key) {
    uint8_t version = 0;
    for(size_t i = 0; i < 8; i++) {
        version |= (key[i] & 0x01) << (7 - i);
    }
    return version;
}

uint32_t dfc_emulator_crc32(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    for(size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for(size_t bit = 0; bit < 8; bit++) {
            uint32_t mask = 0u - (crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return crc;
}
