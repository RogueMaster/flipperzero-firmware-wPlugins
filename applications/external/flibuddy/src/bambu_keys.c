#include "bambu_keys.h"

#include <string.h>

#include "crypto/hkdf_sha256.h"

static const uint8_t bambu_master_key[16] = {
    0x9a,
    0x75,
    0x9c,
    0xf2,
    0xc4,
    0xf7,
    0xca,
    0xff,
    0x22,
    0x2c,
    0xb9,
    0x76,
    0x9b,
    0x41,
    0xbc,
    0x96,
};

static const uint8_t bambu_info[7] = {'R', 'F', 'I', 'D', '-', 'A', '\0'};

void bambu_derive_keys(const uint8_t uid[BAMBU_UID_LEN], uint8_t out[BAMBU_KEYS_LEN]) {
    /* Cannot fail: the output length is a valid constant. */
    fb_hkdf_sha256(
        bambu_master_key,
        sizeof(bambu_master_key),
        uid,
        BAMBU_UID_LEN,
        bambu_info,
        sizeof(bambu_info),
        out,
        BAMBU_KEYS_LEN);
}

bool bambu_sector_key(
    const uint8_t keys[BAMBU_KEYS_LEN],
    uint8_t sector,
    uint8_t out[BAMBU_KEY_LEN]) {
    if(sector >= BAMBU_SECTOR_COUNT) return false;
    memcpy(out, keys + (size_t)sector * BAMBU_KEY_LEN, BAMBU_KEY_LEN);
    return true;
}
