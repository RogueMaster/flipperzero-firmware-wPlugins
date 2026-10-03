#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct MorseFlipperApp MorseFlipperApp;

bool morse_flipper_tx_groups_host_enter(MorseFlipperApp* app);
void morse_flipper_tx_groups_host_detach(void);
