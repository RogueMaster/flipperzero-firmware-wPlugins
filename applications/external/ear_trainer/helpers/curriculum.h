#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "chords.h"
#include "intervals.h"

/* Three kinds of ear training, sharing one level/quiz engine. Intervals get
 * three modes because direction genuinely changes the skill; chords and
 * scales are always played upward. */
typedef enum {
    ModeIntervalAsc,
    ModeIntervalDesc,
    ModeIntervalMixed,
    ModeChords,
    ModeScales,
    MODE_COUNT,
} TrainMode;

typedef enum {
    ContentInterval,
    ContentChord,
    ContentScale,
} ContentType;

/* Longest curriculum of the three; the save file sizes its arrays to this. */
#define LEVEL_COUNT_MAX   13
#define MAX_NEW_PER_LEVEL 2

typedef struct {
    const char* label; /* shown in level select */
    const uint8_t* new_items; /* introduced here; NULL on review levels */
    uint8_t new_count;
    bool challenge; /* no new material, mistakes are capped */
} EarLevel;

ContentType mode_content(uint8_t mode);
const char* mode_name(uint8_t mode);

uint8_t curriculum_level_count(uint8_t mode);
const EarLevel* curriculum_get(uint8_t mode, uint8_t level_index);
bool curriculum_is_challenge(uint8_t mode, uint8_t level_index);

/* Everything unlocked in levels 0..level_index inclusive, ascending.
 * Returns how many ids were written. */
uint8_t curriculum_learned_upto(uint8_t mode, uint8_t level_index, uint8_t* buf, uint8_t buf_size);

/* Display names for one answer id, whichever content type is in play. */
const char* content_shortname(uint8_t mode, uint8_t id);
const char* content_name(uint8_t mode, uint8_t id);
const char* content_hint(uint8_t mode, uint8_t id);
