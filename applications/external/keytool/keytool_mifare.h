#pragma once

#include <furi.h>
#include <storage/storage.h>

// Maximum number of keys we will ever try to extract from one dump
// (40 sectors max for a MIFARE Classic 4K, x2 keys per sector = 80)
#define KEYTOOL_MAX_KEYS 80

typedef struct {
    uint8_t bytes[6];
    uint32_t sector;
    char type; // 'A' or 'B'
} KeyToolKeyEntry;

// Parses a Flipper .nfc MIFARE Classic dump file and extracts every
// readable key (Key A / Key B) from its sector trailer blocks.
// Blocks whose key bytes are unknown in the dump (shown as "??" because
// the sector was never authenticated) are skipped, not reported as absent.
//
// Returns false (with `error` filled in) if the file could not be opened
// or no usable key was found at all.
bool keytool_mifare_parse_dump(
    Storage* storage,
    const char* path,
    KeyToolKeyEntry* keys_out,
    size_t* key_count_out,
    FuriString* error);
