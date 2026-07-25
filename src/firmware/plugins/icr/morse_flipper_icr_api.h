#pragma once

#include <gui/canvas.h>
#include <input/input.h>

#include <stdbool.h>
#include <stdint.h>

#include "../../morse_flipper_mapped_fal.h"

#define MORSE_FLIPPER_ICR_API_VERSION 4U
#define MORSE_FLIPPER_ICR_API_MAGIC 0x4D464943UL

typedef enum {
    MorseFlipperIcrFeedbackNone = 0,
    MorseFlipperIcrFeedbackClear,
    MorseFlipperIcrFeedbackGood,
    MorseFlipperIcrFeedbackFail,
    MorseFlipperIcrFeedbackTimeout,
} MorseFlipperIcrFeedback;

typedef enum {
    MorseFlipperIcrEntryTraining = 0,
    MorseFlipperIcrEntrySettings,
} MorseFlipperIcrEntryKind;

typedef MorseFlipperMappedFalResult MorseFlipperIcrResult;

typedef struct {
    uint32_t now_ms;
    uint32_t rng_seed;
    uint8_t entry_kind;
} MorseFlipperIcrEnterArgs;

typedef struct {
    MorseFlipperMappedFalApi mapped;
    bool (*enter)(void* state, const MorseFlipperIcrEnterArgs* args, MorseFlipperIcrResult* initial);
    MorseFlipperIcrResult (*input)(void* state, const InputEvent* event, uint32_t now_ms);
} MorseFlipperIcrApi;
