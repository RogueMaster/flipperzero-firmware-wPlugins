#pragma once

#include "plugins/rx_practice/morse_flipper_rx_practice_types.h"

typedef struct MorseFlipperApp MorseFlipperApp;
typedef struct Canvas Canvas;

bool morse_flipper_rx_practice_host_enter(MorseFlipperApp* app, uint32_t now_ms);

#define MF_RX_START_OK       (1U << 0)
#define MF_RX_START_BACK     (1U << 1)
#define MF_RX_START_STRAIGHT (1U << 2)
#define MF_RX_START_DIT      (1U << 3)
#define MF_RX_START_DAH      (1U << 4)
bool morse_flipper_rx_practice_host_input(
    MorseFlipperApp* app,
    const InputEvent* event,
    uint32_t now_ms);
bool morse_flipper_rx_practice_host_feed(
    MorseFlipperApp* app,
    const char* text,
    size_t length,
    uint32_t now_ms);
bool morse_flipper_rx_practice_host_tick(
    MorseFlipperApp* app,
    uint32_t now_ms,
    uint8_t down_mask);
