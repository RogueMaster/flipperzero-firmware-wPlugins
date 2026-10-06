// Pod encounter log — who you've crossed paths with, persisted on SD.
#pragma once

#include "profile.h"
#include <stdint.h>
#include <stdbool.h>

#define POD_MAX_ENCOUNTERS 64

typedef struct {
    uint32_t profile_id;
    char name[POD_NAME_MAX + 1];
    uint8_t icon;
    uint8_t dolphin_level; // their level, last seen
    uint32_t first_seen; // rtc timestamp
    uint32_t last_seen;
    uint16_t times_seen;
    uint8_t battled; // 0 until a live/practice battle has happened
    int8_t last_result; // +1 won, 0 draw, -1 lost (our perspective)
    uint16_t total_xp; // XP gained from this rival
} PodEncounter;

typedef struct {
    PodEncounter items[POD_MAX_ENCOUNTERS];
    uint16_t count;
} PodEncounters;

void pod_encounters_load(PodEncounters* e);
void pod_encounters_save(const PodEncounters* e);

// Index of the encounter with this id, or -1.
int pod_encounters_find(const PodEncounters* e, uint32_t profile_id);

// Add a new encounter or update an existing one from a beacon. Returns the
// index (or -1 if the table is full). Sets *is_new to true on first sighting.
int pod_encounters_update(PodEncounters* e, const PodBeacon* b, uint32_t now, bool* is_new);
