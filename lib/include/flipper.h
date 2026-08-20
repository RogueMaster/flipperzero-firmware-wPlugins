//lib/flipper.h
#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdbool.h>
#include <stdint.h>

#define DISPLAY_WIDTH  128
#define DISPLAY_HEIGHT 64
#define BUFFER_SIZE    (DISPLAY_WIDTH * DISPLAY_HEIGHT / 8)

typedef struct {
    uint8_t back_buffer[BUFFER_SIZE];
    uint8_t front_buffer[BUFFER_SIZE];

    Gui* gui;
    Canvas* canvas;
    FuriMutex* fb_mutex;

    volatile uint8_t input_state;
    volatile bool exit_requested;
    volatile bool audio_enabled;

    // back-hold логика
    bool back_hold_active;
    uint16_t back_hold_start;
    bool back_hold_handled;

    // input pubsub
    FuriPubSub* input_events;
    FuriPubSubSubscription* input_sub;
} FlipperState;

extern FlipperState* g_state;
