#pragma once

#include <gui/canvas.h>
#include <input/input.h>

typedef struct MorseFlipperApp MorseFlipperApp;

bool morse_flipper_icr_host_enter(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_icr_host_unload(MorseFlipperApp* app);
bool morse_flipper_icr_host_input(MorseFlipperApp* app, const InputEvent* event, uint32_t now_ms);
void morse_flipper_icr_host_tick(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_icr_host_draw(MorseFlipperApp* app, Canvas* canvas);
