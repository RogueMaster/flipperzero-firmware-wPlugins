#pragma once

#include "plugins/help_about/morse_flipper_help_about_api.h"

typedef struct MorseFlipperApp MorseFlipperApp;

bool morse_flipper_content_host_enter(
    MorseFlipperApp* app,
    MorseFlipperContentMode mode,
    uint8_t help_topic);
bool morse_flipper_content_host_input(
    MorseFlipperApp* app,
    const InputEvent* event,
    uint32_t now_ms);
bool morse_flipper_content_host_tick(MorseFlipperApp* app, uint32_t now_ms);
bool morse_flipper_onboarding_seen(void);
void morse_flipper_onboarding_finish(MorseFlipperApp* app);
