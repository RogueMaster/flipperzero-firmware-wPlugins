#pragma once

#include <gui/canvas.h>
#include <input/input.h>

#include <stdbool.h>
#include <stdint.h>

#define MORSE_FLIPPER_ICR_API_VERSION 1U
#define MORSE_FLIPPER_ICR_API_MAGIC 0x4D464943UL

typedef enum {
    MorseFlipperIcrFeedbackNone = 0,
    MorseFlipperIcrFeedbackClear,
    MorseFlipperIcrFeedbackGood,
    MorseFlipperIcrFeedbackFail,
    MorseFlipperIcrFeedbackTimeout,
} MorseFlipperIcrFeedback;

typedef struct {
    bool handled;
    bool redraw;
    bool request_back;
    bool playback_active;
    bool playback_mark;
    bool prompt_visible;
    uint8_t prompt_char;
    MorseFlipperIcrFeedback feedback;
} MorseFlipperIcrResult;

typedef struct {
    bool draw_prompt;
    uint8_t prompt_char;
    int16_t prompt_cx;
    int16_t prompt_cy;
} MorseFlipperIcrDrawResult;

typedef struct {
    uint32_t now_ms;
    uint32_t rng_seed;
} MorseFlipperIcrEnterArgs;

typedef struct {
    uint32_t magic;
    uint32_t api_version;
    uint32_t struct_size;
    void* (*alloc)(void);
    void (*free)(void* state);
    bool (*enter)(void* state, const MorseFlipperIcrEnterArgs* args, MorseFlipperIcrResult* initial);
    void (*leave)(void* state);
    MorseFlipperIcrResult (*input)(void* state, const InputEvent* event, uint32_t now_ms);
    MorseFlipperIcrResult (*tick)(void* state, uint32_t now_ms);
    MorseFlipperIcrDrawResult (*draw)(void* state, Canvas* canvas, uint32_t now_ms);
} MorseFlipperIcrApi;
