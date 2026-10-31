#pragma once
// Stand-ins for the few Arduino calls the shared game logic makes, so the
// Cardputer headers compile against the Flipper SDK unchanged.
#include <furi.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// The Flipper kernel ticks at 1 kHz, so a tick is a millisecond.
inline uint32_t millis() {
    return furi_get_tick();
}
