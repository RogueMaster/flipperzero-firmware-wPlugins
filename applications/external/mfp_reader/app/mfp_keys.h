#pragma once

#include "mfp_poller.h"
#include <storage/storage.h>

/* ---- Hardcoded default keys ---- */

#define MFP_DEFAULT_KEY_COUNT 6

extern const MfpKey mfp_default_keys[MFP_DEFAULT_KEY_COUNT];

/* ---- Sector geometry helpers ---- */

/** First block number of a sector. */
uint8_t mfp_sector_first_block(MfpCardSize size, uint8_t sector);

/** Number of blocks in a sector (4 for small, 16 for large 4K sectors). */
uint8_t mfp_sector_block_count(MfpCardSize size, uint8_t sector);

/** Total number of sectors for a given card size. */
uint8_t mfp_sector_count(MfpCardSize size);

/* ---- Dictionary helpers ---- */

/** Load dictionary from file into a flat key buffer.
 *  Allocates *out_buf (caller must free). Returns key count. */
uint32_t mfp_keys_load_dict(Storage* storage, const char* path, uint8_t** out_buf);

