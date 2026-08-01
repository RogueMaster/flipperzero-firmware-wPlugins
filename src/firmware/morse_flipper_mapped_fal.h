#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <input/input.h>

typedef struct Canvas Canvas;

typedef struct {
    const char* header;
    const char* text;
    bool confirm;
} MorseFlipperHostDialog;

/* Every embedded FAL exposes these mapped operations before feature-specific calls. */
typedef struct {
    uint32_t handled         : 1;
    uint32_t redraw          : 1;
    uint32_t transition      : 1;
    uint32_t request_exit    : 1;
    uint32_t phase           : 8;
    uint32_t playback_active : 1;
    uint32_t playback_mark   : 1;
    uint32_t backlight_wake  : 1;
    uint32_t backlight_off   : 1;
    uint32_t feedback        : 8;
    uint32_t                 : 8;
} MorseFlipperMappedFalResult;

typedef struct {
    uint32_t magic;
    uint32_t api_version;
    uint32_t struct_size;
    void* (*alloc)(void);
    void (*free)(void* state);
    bool (*enter)(void* state, const void* args, MorseFlipperMappedFalResult* initial);
    void (*leave)(void* state);
    MorseFlipperMappedFalResult (*input)(void* state, const InputEvent* event, uint32_t now_ms);
    MorseFlipperMappedFalResult (*tick)(void* state, uint32_t now_ms);
    void (*draw)(void* state, Canvas* canvas, uint32_t now_ms);
} MorseFlipperMappedFalApi;

/* Radio-sized FALs share one typed command slot without changing smaller mapped FALs. */
typedef MorseFlipperMappedFalResult (*MorseFlipperMappedFalCommand)(
    void* state,
    uint32_t command,
    const void* input,
    void* output,
    uint32_t now_ms);

typedef struct {
    MorseFlipperMappedFalApi mapped;
    MorseFlipperMappedFalCommand command;
} MorseFlipperCommandFalApi;
