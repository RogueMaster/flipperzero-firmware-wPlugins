#pragma once

#include <stdint.h>

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint8_t sound;
    uint8_t reserved[5];
    uint32_t xp;
    uint32_t level;
    uint32_t completed_mask;
} PocketLabState;

/** Load persisted state, or populate defaults when no valid file exists. */
void pocketlab_storage_load(PocketLabState* state);

/** Persist state to the SD card. */
void pocketlab_storage_save(const PocketLabState* state);
