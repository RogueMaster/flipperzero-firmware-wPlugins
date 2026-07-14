/*
 * Purpose: Publish custom trainer character set storage helpers.
 * Owns: custom set limits, parsed set model, and load/path APIs.
 * Depends on: trainer.h.
 * Tests: tests/test_trainer_files.c.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "trainer.h"

#define MORSE_TRAINER_CUSTOM_SET_CAP  8U
#define MORSE_TRAINER_CUSTOM_NAME_CAP 24U
#define MORSE_TRAINER_CUSTOM_TEXT_CAP 512U

typedef struct {
    char name[MORSE_TRAINER_CUSTOM_NAME_CAP];
    char chars[MORSE_TRAINER_CHARSET_CAP];
} MorseTrainerCustomSet;

typedef struct {
    uint8_t count;
    MorseTrainerCustomSet sets[MORSE_TRAINER_CUSTOM_SET_CAP];
} MorseTrainerCustomSets;

const char* morse_trainer_custom_chars_path(void);
bool morse_trainer_load_custom_sets(MorseTrainerCustomSets* sets);
