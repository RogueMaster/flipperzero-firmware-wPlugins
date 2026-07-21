#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "intervals.h"

#define LEVEL_COUNT       13
#define MAX_NEW_PER_LEVEL 2

typedef struct {
    const char* label; /* shown in level select, e.g. "P1 P8" */
    const uint8_t* new_intervals; /* introduced here; NULL on challenge levels */
    uint8_t new_count;
    bool challenge; /* no new material, mistakes are capped */
} EarLevel;

const EarLevel* curriculum_get(uint8_t level_index);
bool curriculum_is_challenge(uint8_t level_index);

/* Fills buf with every interval unlocked in levels 0..level_index inclusive,
 * ascending. Returns how many were written. */
uint8_t curriculum_learned_upto(uint8_t level_index, uint8_t* buf, uint8_t buf_size);
