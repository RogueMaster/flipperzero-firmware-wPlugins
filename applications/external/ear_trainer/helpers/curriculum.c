#include "curriculum.h"

/* Intervals are introduced widest-first: an octave and a fifth are easy to
 * tell apart, seconds and the tritone are not. Challenge levels add nothing
 * new and instead mix everything learned so far. */

static const uint8_t new_l0[] = {IntervalUnison, IntervalOctave};
static const uint8_t new_l1[] = {IntervalPerfect5};
static const uint8_t new_l2[] = {IntervalPerfect4};
static const uint8_t new_l4[] = {IntervalMajor3};
static const uint8_t new_l5[] = {IntervalMinor3};
static const uint8_t new_l7[] = {IntervalMajor2};
static const uint8_t new_l8[] = {IntervalMajor6};
static const uint8_t new_l9[] = {IntervalMinor6};
static const uint8_t new_l10[] = {IntervalMinor2};
static const uint8_t new_l11[] = {IntervalMinor7, IntervalMajor7};
static const uint8_t new_l12[] = {IntervalTritone};

#define LEVEL(lbl, arr) {lbl, arr, (uint8_t)(sizeof(arr) / sizeof(arr[0])), false}
#define CHALLENGE(lbl)  {lbl, NULL, 0, true}

static const EarLevel levels[LEVEL_COUNT] = {
    LEVEL("P1 P8", new_l0),
    LEVEL("P5", new_l1),
    LEVEL("P4", new_l2),
    CHALLENGE("Mix 1"),
    LEVEL("M3", new_l4),
    LEVEL("m3", new_l5),
    CHALLENGE("Mix 2"),
    LEVEL("M2", new_l7),
    LEVEL("M6", new_l8),
    LEVEL("m6", new_l9),
    LEVEL("m2", new_l10),
    LEVEL("m7 M7", new_l11),
    LEVEL("TT", new_l12),
};

const EarLevel* curriculum_get(uint8_t level_index) {
    if(level_index >= LEVEL_COUNT) level_index = LEVEL_COUNT - 1;
    return &levels[level_index];
}

bool curriculum_is_challenge(uint8_t level_index) {
    return curriculum_get(level_index)->challenge;
}

uint8_t curriculum_learned_upto(uint8_t level_index, uint8_t* buf, uint8_t buf_size) {
    if(level_index >= LEVEL_COUNT) level_index = LEVEL_COUNT - 1;

    bool seen[IntervalCount] = {0};
    for(uint8_t l = 0; l <= level_index; l++) {
        const EarLevel* level = &levels[l];
        for(uint8_t i = 0; i < level->new_count; i++)
            seen[level->new_intervals[i]] = true;
    }

    uint8_t count = 0;
    for(uint8_t s = 0; s < IntervalCount && count < buf_size; s++) {
        if(seen[s]) buf[count++] = s;
    }
    return count;
}
