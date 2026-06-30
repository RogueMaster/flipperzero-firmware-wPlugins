#include "flipper_wedge_speaker.h"
#include "../flipper_wedge.h"

#define NOTE_INPUT 587.33f

void flipper_wedge_play_input_sound(void* context) {
    FlipperWedge* app = context;
    UNUSED(app);
    float volume = 1.0f;
    if(furi_hal_speaker_is_mine() || furi_hal_speaker_acquire(30)) {
        furi_hal_speaker_start(NOTE_INPUT, volume);
    }
}

void flipper_wedge_stop_all_sound(void* context) {
    FlipperWedge* app = context;
    UNUSED(app);
    if(furi_hal_speaker_is_mine()) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }
}
