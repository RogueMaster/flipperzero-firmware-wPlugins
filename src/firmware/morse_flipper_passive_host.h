#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <input/input.h>

typedef struct MorseFlipperApp MorseFlipperApp;

bool morse_flipper_passive_host_enter(
    MorseFlipperApp* app,
    uint32_t now_ms,
    uint8_t entry_kind);
