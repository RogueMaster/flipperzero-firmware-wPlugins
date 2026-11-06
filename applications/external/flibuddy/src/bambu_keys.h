/* Bambu Lab MIFARE Classic Key A derivation, as in the SpoolBuddy daemon:
 * HKDF-SHA256 with the Bambu master key as salt, the 4-byte UID as input
 * key material and "RFID-A\0" as info; 96 bytes out are 16 six-byte keys,
 * sector 0 first. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define BAMBU_UID_LEN      4
#define BAMBU_SECTOR_COUNT 16
#define BAMBU_KEY_LEN      6
#define BAMBU_KEYS_LEN     (BAMBU_SECTOR_COUNT * BAMBU_KEY_LEN)

void bambu_derive_keys(const uint8_t uid[BAMBU_UID_LEN], uint8_t out[BAMBU_KEYS_LEN]);

/* Copies the Key A of `sector` out of a derived key set. Returns false,
 * leaving `out` untouched, when `sector` is not below BAMBU_SECTOR_COUNT. */
bool bambu_sector_key(
    const uint8_t keys[BAMBU_KEYS_LEN],
    uint8_t sector,
    uint8_t out[BAMBU_KEY_LEN]);
