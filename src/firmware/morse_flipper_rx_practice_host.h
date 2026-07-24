#pragma once

#include "plugins/rx_practice/morse_flipper_rx_practice_types.h"

typedef struct MorseFlipperApp MorseFlipperApp;
typedef struct Canvas Canvas;

bool morse_flipper_rx_practice_host_enter(
    MorseFlipperApp* app,
    MfRxPracticeMode mode,
    uint32_t now_ms);
void morse_flipper_rx_practice_host_unload(MorseFlipperApp* app);
void morse_flipper_rx_practice_host_unload_locked(MorseFlipperApp* app);
bool morse_flipper_rx_practice_host_command(
    MorseFlipperApp* app,
    MfRxPracticeCommand command,
    uint32_t now_ms);
bool morse_flipper_rx_practice_host_feed(MorseFlipperApp* app, const char* text, size_t length, uint32_t now_ms);
void morse_flipper_rx_practice_host_tick(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_rx_practice_host_draw(MorseFlipperApp* app, Canvas* canvas);
