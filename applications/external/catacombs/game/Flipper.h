#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdbool.h>
#include <stdint.h>

#define DISPLAY_WIDTH  128
#define DISPLAY_HEIGHT 64
#define BUFFER_SIZE    (DISPLAY_WIDTH * DISPLAY_HEIGHT / 8)

#define FRAMEBUFFER_GUARD_SIZE 8
#define FRAMEBUFFER_GUARD_BYTE  0xA5

typedef struct {
    // The framebuffer used to sit at offset 0 of this malloc'd block, so a write
    // to framebuffer[-1] landed on the heap block header. That corrupts
    // xBlockSize and only detonates at free() on app exit, wrecking the
    // allocator for the whole system. These pads absorb such a write and make it
    // detectable instead of fatal.
    uint8_t guard_low[FRAMEBUFFER_GUARD_SIZE];
    uint8_t framebuffer[BUFFER_SIZE];
    uint8_t guard_high[FRAMEBUFFER_GUARD_SIZE];

    FuriMutex* mutex;

    volatile uint8_t input_state;
    volatile bool exit_requested;
    volatile bool audio_enabled;
    // Not persisted: the backlight lock starts on every launch and is always
    // released on exit, so a dimmed screen never outlives the app
    volatile bool backlight_enabled;
} FlipperState;

extern FlipperState* g_state;
