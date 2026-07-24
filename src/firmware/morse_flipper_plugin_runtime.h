#pragma once

#include <stdbool.h>

typedef struct MorseFlipperApp MorseFlipperApp;

/* Own the app-lifetime lock shared by all embedded FAL hosts. */
bool morse_flipper_plugin_runtime_init(MorseFlipperApp* app);
void morse_flipper_plugin_runtime_deinit(MorseFlipperApp* app);
