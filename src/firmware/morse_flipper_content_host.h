#pragma once

#include "plugins/help_about/morse_flipper_help_about_api.h"

typedef struct MorseFlipperApp MorseFlipperApp;

bool morse_flipper_content_host_init(MorseFlipperApp* app);
void morse_flipper_content_host_deinit(MorseFlipperApp* app);
bool morse_flipper_content_host_enter(
    MorseFlipperApp* app,
    MorseFlipperContentMode mode,
    uint8_t help_topic);
void morse_flipper_content_host_unload(MorseFlipperApp* app);
bool morse_flipper_content_host_input(MorseFlipperApp* app, const InputEvent* event, uint32_t now_ms);
bool morse_flipper_content_host_tick(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_content_host_draw(MorseFlipperApp* app, Canvas* canvas);
void morse_flipper_content_host_draw_unavailable(MorseFlipperApp* app, Canvas* canvas);
bool morse_flipper_onboarding_seen(void);
void morse_flipper_onboarding_finish(MorseFlipperApp* app);
