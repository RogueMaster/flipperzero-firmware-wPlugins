#include "maze3d.h"

// Sound is intentionally a no-op stub for stability.
// Speaker hardware access (furi_hal_speaker_*) is disabled to rule out
// any hardware-related crashes. Can be re-enabled once the core is stable.

void sfx_init(void) {
}

void sfx_deinit(void) {
}

void sfx_play(SfxType t) {
    (void)t;
}

void sfx_tick_update(void) {
}
