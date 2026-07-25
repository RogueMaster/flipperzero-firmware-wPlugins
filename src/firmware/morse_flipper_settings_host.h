#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct MorseFlipperApp MorseFlipperApp;

bool morse_flipper_settings_host_enter(MorseFlipperApp* app, uint8_t entry, uint32_t selected_state);
bool morse_flipper_settings_host_close(MorseFlipperApp* app, uint32_t scene);
void morse_flipper_settings_host_leave(MorseFlipperApp* app, uint32_t scene);
