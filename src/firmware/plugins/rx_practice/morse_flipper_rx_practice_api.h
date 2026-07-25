#pragma once

#include <gui/canvas.h>
#include <input/input.h>

#include "morse_flipper_rx_practice_types.h"

#define MORSE_FLIPPER_RX_PRACTICE_API_VERSION 6U
#define MORSE_FLIPPER_RX_PRACTICE_API_MAGIC 0x4D465258UL

typedef struct {
    MorseFlipperMappedFalApi mapped;
    bool (*enter)(void* state, const MfRxPracticeEnterArgs* args, MfRxPracticeResult* initial);
    MfRxPracticeResult (*input)(
        void* state,
        const InputEvent* event,
        bool button_paddle,
        uint32_t now_ms);
    MfRxPracticeResult (*command)(void* state, MfRxPracticeCommand command, uint32_t now_ms);
    MfRxPracticeResult (*feed_text)(void* state, const char* text, size_t text_len, uint32_t now_ms);
} MfRxPracticeApi;
