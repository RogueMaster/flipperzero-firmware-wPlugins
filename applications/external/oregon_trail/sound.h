#pragma once
#include <stdbool.h>

// ── Sound events ──────────────────────────────────────────────
typedef enum {
    SND_TRAIL_ADVANCE,    // light crunch — day advancing
    SND_HUNT_FIRE,        // sharp crack — rifle shot
    SND_HUNT_HIT,         // thud — animal hit
    SND_HUNT_MISS,        // descending whiff
    SND_HUNT_GORED,       // low aggressive burst — wolf/buffalo attack
    SND_EVENT_GOOD,       // ascending two-note chirp
    SND_EVENT_BAD,        // descending minor three notes
    SND_PLAYER_DEATH,     // slow descending chromatic — the classic
    SND_GAME_OVER,        // full death jingle, longer
    SND_FORT_ARRIVE,      // two-note ascending fanfare
    SND_RIVER_SPLASH,     // rapid low pulse — danger
    SND_RIVER_SAFE,       // gentle resolved chord
    SND_WIN,              // triumphant ascending run
} SoundEvent;

// ── API ───────────────────────────────────────────────────────
void sound_init   (bool enabled);
void sound_toggle (void);
bool sound_is_on  (void);
void sound_play   (SoundEvent ev);
