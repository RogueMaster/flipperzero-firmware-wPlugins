#include "music.h"
#include <furi_hal.h>

// Base melody: G3 → A3 → B3 → C4 → B3 → A3 → G3 → F3
static const float bg_notes[] = {
    196.00f, 220.00f, 246.94f, 261.63f,
    246.94f, 220.00f, 196.00f, 174.61f,
};
#define BG_NOTES_COUNT 8

// Pitch slowly breathes up two semitones and back down every 30 s.
// Phase changes every 5 s: each step is one semitone (×1.0595).
static const float pitch_cycle[] = {
    1.0000f,   // normal
    1.0595f,   // +1 semitone
    1.1225f,   // +2 semitones
    1.0595f,   // +1 semitone
    1.0000f,   // normal
    0.9439f,   // -1 semitone
};
#define PITCH_CYCLE_COUNT 6

static int32_t bg_music_thread(void* ctx) {
    BgMusic* music = ctx;
    while(music->running) {
        if(music->active) {
            if(furi_hal_speaker_acquire(50)) {
                uint32_t phase = (furi_get_tick() / 5000) % PITCH_CYCLE_COUNT;
                float freq = bg_notes[music->idx % BG_NOTES_COUNT] * pitch_cycle[phase];
                furi_hal_speaker_start(freq, BG_VOLUME);
                furi_delay_ms(BG_NOTE_MS);
                furi_hal_speaker_stop();
                furi_hal_speaker_release();
                music->idx++;
            }
            furi_delay_ms(BG_REST_MS);
        } else {
            furi_delay_ms(100);
        }
    }
    return 0;
}

void bg_music_start(BgMusic* music) {
    music->idx     = 0;
    music->active  = false;
    music->running = true;
    music->thread  = furi_thread_alloc_ex("SimonBG", 512, bg_music_thread, music);
    furi_thread_set_priority(music->thread, FuriThreadPriorityLow);
    furi_thread_start(music->thread);
}

void bg_music_stop(BgMusic* music) {
    music->running = false;
    furi_thread_join(music->thread);
    furi_thread_free(music->thread);
}
