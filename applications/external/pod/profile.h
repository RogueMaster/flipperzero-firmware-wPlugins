// Pod identity — a small profile persisted on the SD card.
#pragma once

#include <stdint.h>
#include <stdbool.h>

#define POD_NAME_MAX 12

typedef struct {
    uint32_t magic; // file format marker
    uint32_t profile_id; // random identity, recognizes repeat encounters
    char name[POD_NAME_MAX + 1]; // callsign, NUL-terminated
    uint8_t icon; // icon id (Phase 4)
    uint32_t wins;
    uint32_t losses;
    uint32_t xp; // app-progression, separate from the real dolphin
} PodProfile;

// A decoded beacon received over the air (or simulated in debug).
typedef struct {
    uint32_t profile_id;
    char name[POD_NAME_MAX + 1];
    uint8_t icon;
    uint8_t dolphin_level;
    uint16_t nonce;
} PodBeacon;

// Load the profile from SD, or create+save a default one on first run.
void pod_profile_load(PodProfile* profile);

// Persist the profile to SD.
void pod_profile_save(const PodProfile* profile);

// Read the real dolphin level live (never mutated by Pod).
uint8_t pod_profile_dolphin_level(void);
