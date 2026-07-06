#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <storage/storage.h>

#include "src/data/cfs_data.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CfsSerialRandom, // random 6 digits every write
    CfsSerialKeepSource, // keep source serial on clone/edit; random for new (default)
    CfsSerialSequential, // increment a stored counter
    CfsSerialPrompt, // ask in the wizard
    CfsSerialModeCount,
} CfsSerialMode;

typedef struct {
    CfsBrand default_brand;
    CfsSerialMode serial_mode;
    uint16_t weight_len[CFS_WEIGHT_COUNT]; // length (m) per fixed weight
    uint32_t serial_counter; // for CfsSerialSequential
    CfsColorPalette color_palette; // how many colors the picker offers
} CfsSettings;

/** Populate with defaults (Creality, keep-source, standard weight table). */
void cfs_settings_default(CfsSettings* settings);

/** Load from disk into settings; falls back to defaults if missing/invalid. */
void cfs_settings_load(Storage* storage, CfsSettings* settings);

/** Persist settings to disk. Returns true on success. */
bool cfs_settings_save(Storage* storage, const CfsSettings* settings);

/** Human label for a serial mode. */
const char* cfs_serial_mode_label(CfsSerialMode mode);

#ifdef __cplusplus
}
#endif
