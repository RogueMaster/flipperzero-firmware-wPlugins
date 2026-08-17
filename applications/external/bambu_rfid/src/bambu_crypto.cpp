#include "bambu_crypto.h"

#include <mbedtls/md.h>
#include <string.h>

namespace {
static const uint8_t BrMasterKey[16] = {
    0x9A, 0x75, 0x9C, 0xF2, 0xC4, 0xF7, 0xCA, 0xFF,
    0x22, 0x2C, 0xB9, 0x76, 0x9B, 0x41, 0xBC, 0x96,
};
static const uint8_t BrContextA[] = {'R', 'F', 'I', 'D', '-', 'A', 0x00};
static const uint8_t BrContextB[] = {'R', 'F', 'I', 'D', '-', 'B', 0x00};

bool hmac_sha256(
    const uint8_t* key,
    size_t key_len,
    const uint8_t* data,
    size_t data_len,
    uint8_t out[32]) {
    const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if(!info) return false;
    return mbedtls_md_hmac(info, key, key_len, data, data_len, out) == 0;
}

bool hkdf_sha256_96(
    const uint8_t uid[BR_UID_SIZE],
    const uint8_t* context,
    size_t context_size,
    uint8_t output[BR_SECTOR_COUNT * MF_CLASSIC_KEY_SIZE]) {
    // This mirrors mbedtls_hkdf(SHA256, salt, UID, context, 96 bytes).
    // The Bambu KDF uses the same salt for both key types and distinguishes
    // them with the NUL-terminated contexts "RFID-A\0" and "RFID-B\0".
    uint8_t prk[32];
    if(!hmac_sha256(BrMasterKey, sizeof(BrMasterKey), uid, BR_UID_SIZE, prk)) return false;

    uint8_t previous[32] = {};
    size_t previous_len = 0;
    size_t written = 0;
    uint8_t counter = 1;
    constexpr size_t OutputSize = BR_SECTOR_COUNT * MF_CLASSIC_KEY_SIZE;

    while(written < OutputSize) {
        uint8_t input[32 + sizeof(BrContextA) + 1U];
        size_t input_len = 0;
        if(previous_len) {
            memcpy(input, previous, previous_len);
            input_len += previous_len;
        }
        memcpy(input + input_len, context, context_size);
        input_len += context_size;
        input[input_len++] = counter++;

        if(!hmac_sha256(prk, sizeof(prk), input, input_len, previous)) return false;
        previous_len = sizeof(previous);

        const size_t remaining = OutputSize - written;
        const size_t chunk = remaining < sizeof(previous) ? remaining : sizeof(previous);
        memcpy(output + written, previous, chunk);
        written += chunk;
    }

    return true;
}
} // namespace

bool br_derive_sector_keys(const uint8_t uid[BR_UID_SIZE], MfClassicDeviceKeys* keys) {
    if(!uid || !keys) return false;

    uint8_t key_a[BR_SECTOR_COUNT * MF_CLASSIC_KEY_SIZE];
    uint8_t key_b[BR_SECTOR_COUNT * MF_CLASSIC_KEY_SIZE];
    if(!hkdf_sha256_96(uid, BrContextA, sizeof(BrContextA), key_a) ||
       !hkdf_sha256_96(uid, BrContextB, sizeof(BrContextB), key_b))
        return false;

    memset(keys, 0, sizeof(*keys));
    for(uint8_t sector = 0; sector < BR_SECTOR_COUNT; ++sector) {
        memcpy(
            keys->key_a[sector].data,
            key_a + static_cast<size_t>(sector) * MF_CLASSIC_KEY_SIZE,
            MF_CLASSIC_KEY_SIZE);
        memcpy(
            keys->key_b[sector].data,
            key_b + static_cast<size_t>(sector) * MF_CLASSIC_KEY_SIZE,
            MF_CLASSIC_KEY_SIZE);
        FURI_BIT_SET(keys->key_a_mask, sector);
        FURI_BIT_SET(keys->key_b_mask, sector);
    }
    return true;
}
