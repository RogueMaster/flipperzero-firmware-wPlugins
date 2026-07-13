#pragma once

#include <stdint.h>

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint8_t sound;
    uint8_t led; // RGB LED indications on/off
    uint8_t vibro; // vibration on/off
    uint8_t reserved;
    uint32_t xp;
    uint32_t level;
    uint64_t completed_mask; // one bit per lab, up to 64 labs
    uint32_t streak_days;
    uint32_t last_day; // day-number of the last completion, for the streak
} PocketLabState;

/** Load persisted state, or populate defaults when no valid file exists. */
void pocketlab_storage_load(PocketLabState* state);

/** Persist state to the SD card. */
void pocketlab_storage_save(const PocketLabState* state);
