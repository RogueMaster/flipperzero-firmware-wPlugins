#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "plugins/radio/mf_radio_api.h"

typedef struct MorseFlipperApp MorseFlipperApp;

const MfRadioDecoderServices* morse_flipper_radio_decoder_services(void);
bool morse_flipper_radio_host_open(MorseFlipperApp* app, uint32_t now_ms);
bool morse_flipper_radio_host_active(const MorseFlipperApp* app);
bool morse_flipper_radio_host_set_page(MorseFlipperApp* app, MfRadioPage page, uint32_t now_ms);
bool morse_flipper_radio_host_input(MorseFlipperApp* app, const InputEvent* event, uint32_t now_ms);
void morse_flipper_radio_host_tick(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_radio_host_sync_tx(
    MorseFlipperApp* app,
    MfRadioTxInterval completed_interval,
    uint16_t duration_ms,
    bool level,
    uint32_t now_ms);
void morse_flipper_radio_host_draw(MorseFlipperApp* app, Canvas* canvas, uint32_t now_ms);
void morse_flipper_radio_host_close(MorseFlipperApp* app, uint32_t now_ms);
