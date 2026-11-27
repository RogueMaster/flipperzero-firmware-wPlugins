#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "plugins/ardf/mf_ardf_api.h"

typedef struct MorseFlipperApp MorseFlipperApp;

#define MORSE_FLIPPER_ARDF_VIEW_NONE 0xFFU
#define MORSE_FLIPPER_ARDF_VIEW_TEXT 0xFEU

bool morse_flipper_ardf_host_open(MorseFlipperApp* app, uint32_t now_ms);
bool morse_flipper_ardf_host_input(MorseFlipperApp* app, const InputEvent* event, uint32_t now_ms);
void morse_flipper_ardf_host_tick(MorseFlipperApp* app, uint32_t now_ms);
bool morse_flipper_ardf_host_text_result(
    MorseFlipperApp* app,
    const char* text,
    bool accepted,
    uint32_t now_ms);
void morse_flipper_ardf_host_close(void* context);
