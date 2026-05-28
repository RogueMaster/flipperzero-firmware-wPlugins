#pragma once
#include <furi.h>

#define BG_VOLUME  0.30f   // 0.0 – 1.0
#define BG_NOTE_MS 80u     // note duration (ms)
#define BG_REST_MS 160u    // silence between notes (ms)

typedef struct {
    uint8_t       idx;      // current position in the note loop
    volatile bool active;   // set true during gameplay, false otherwise
    volatile bool running;  // set false to stop the thread
    FuriThread*   thread;
} BgMusic;

void bg_music_start(BgMusic* music);
void bg_music_stop(BgMusic* music);
