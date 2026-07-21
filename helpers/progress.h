#pragma once

#include <stdint.h>

#include "curriculum.h"

typedef enum {
    ModeAscending,
    ModeDescending,
    ModeMixed,
    MODE_COUNT,
} TrainMode;

/* Kept flat and packed so a future version can append fields and still read
 * an older file as a byte-prefix of the newer struct. */
typedef struct __attribute__((packed)) {
    uint8_t unlocked[MODE_COUNT]; /* highest level reachable, 1-based */
    uint8_t stars[MODE_COUNT][LEVEL_COUNT]; /* 0..3 */
    uint32_t answered; /* lifetime totals, all modes */
    uint32_t correct;
    uint16_t best_streak;
} EarProgress;

typedef struct __attribute__((packed)) {
    uint8_t note_ms; /* 0 short, 1 medium, 2 long */
    uint8_t random_root; /* 0 = always C4, 1 = random root each question */
    uint8_t vibro;
    uint8_t led;
    uint8_t show_mnemonic; /* show the tune hint on the answer screen */
} EarSettings;

void ear_progress_load(EarProgress* progress);
void ear_progress_save(const EarProgress* progress);
void ear_settings_load(EarSettings* settings);
void ear_settings_save(const EarSettings* settings);

/* Note length in ms for the settings index. */
uint16_t ear_note_duration_ms(uint8_t note_ms_index);
