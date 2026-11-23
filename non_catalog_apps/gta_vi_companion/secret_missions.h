#pragma once
#include <stddef.h>
#include <stdint.h>
/* Ordered by the earliest published availability, then by optional route.
 * A question mark means that the story gate has NOT been established. */
typedef struct {
    const char* stage;
    const char* title;
    const char* when;
    const char* where;
    const char* steps;
    const char* reward;
    const char* status;
    const char* sources;
    /* Stable progress key: never reuse or renumber when the list is sorted. */
    uint8_t progress_id;
} GtaSecretMission;
extern const GtaSecretMission gta_secret_missions[];
extern const size_t gta_secret_mission_count;
