#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct Canvas Canvas;

/* Every embedded FAL exposes these mapped operations before feature-specific calls. */
typedef struct {
    bool handled;
    bool redraw;
    bool decoder_reset;
    bool request_exit;
    uint8_t phase;
    bool playback_active;
    bool playback_mark;
    uint8_t feedback;
} MorseFlipperMappedFalResult;

typedef struct {
    uint32_t magic;
    uint32_t api_version;
    uint32_t struct_size;
    void* (*alloc)(void);
    void (*free)(void* state);
    void (*leave)(void* state);
    MorseFlipperMappedFalResult (*tick)(void* state, uint32_t now_ms);
    void (*draw)(void* state, Canvas* canvas, uint32_t now_ms);
} MorseFlipperMappedFalApi;
