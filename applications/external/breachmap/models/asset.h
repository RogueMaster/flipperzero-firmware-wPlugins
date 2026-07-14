#pragma once

#include "recon_types.h"

typedef struct {
    uint16_t id; /* stable within a session */
    AssetType type;
    uint8_t risk; /* 0..100 */
    Severity severity; /* finding severity */
    char name[RECON_NAME_LEN];
    char notes[RECON_NOTE_LEN];
    char remediation[RECON_NOTE_LEN]; /* recommended fix */
    uint8_t gx; /* graph x (0..127), RECON_COORD_AUTO = auto-place */
    uint8_t gy; /* graph y (0..63), RECON_COORD_AUTO = auto-place */
    uint32_t created;
    uint32_t modified;
} Asset;

/* Reset an asset to a blank record with the given id. */
void asset_init(Asset* asset, uint16_t id);
