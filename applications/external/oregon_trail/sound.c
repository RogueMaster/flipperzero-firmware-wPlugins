#include "sound.h"
#include <furi_hal.h>
#include <furi.h>

// ── Module state ──────────────────────────────────────────────
static bool s_enabled = true;

void sound_init(bool enabled) { s_enabled = enabled; }
void sound_toggle(void)       { s_enabled = !s_enabled; }
bool sound_is_on(void)        { return s_enabled; }

// ── Tone primitive ────────────────────────────────────────────
// Acquire speaker, play one tone, release. Blocking but very short.
static void tone(float freq, uint32_t ms) {
    if(furi_hal_speaker_acquire(30)) {
        furi_hal_speaker_start(freq, 0.6f);
        furi_delay_ms(ms);
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }
}

// Short gap between notes (releases speaker briefly)
static void gap(uint32_t ms) {
    furi_delay_ms(ms);
}

// ── Sound definitions ─────────────────────────────────────────
// Frequencies chosen to evoke the Apple II originals where documented,
// adapted to the Flipper's piezo range (best: 200–4000 Hz).
//
// Apple II original notable frequencies (from source documentation):
//   Good event: ~880 Hz → ~1108 Hz (A5→C#6 chirp)
//   Bad event:  ~440 Hz → ~349 Hz → ~294 Hz (A4→F4→D4 descend)
//   Death:      ~392→349→330→294→262 Hz (chromatic descent G4→C4)
//   Fort:       ~523→659 Hz (C5→E5 fanfare)

void sound_play(SoundEvent ev) {
    if(!s_enabled) return;

    switch(ev) {

        case SND_TRAIL_ADVANCE:
            // Light crunch — very short noise burst at mid-low freq
            tone(180.0f, 18);
            gap(6);
            tone(160.0f, 12);
            break;

        case SND_HUNT_FIRE:
            // Sharp crack — brief high then immediate low
            tone(1200.0f, 15);
            gap(5);
            tone(200.0f, 25);
            break;

        case SND_HUNT_HIT:
            // Satisfying thud — descending two notes
            tone(600.0f, 30);
            gap(10);
            tone(300.0f, 40);
            break;

        case SND_HUNT_MISS:
            // Descending whiff — three notes falling
            tone(500.0f, 25);
            gap(8);
            tone(380.0f, 25);
            gap(8);
            tone(260.0f, 35);
            break;

        case SND_HUNT_GORED:
            // Low aggressive burst — wolf/buffalo attack
            tone(120.0f, 40);
            gap(15);
            tone(100.0f, 40);
            gap(15);
            tone(80.0f,  60);
            break;

        case SND_EVENT_GOOD:
            // Ascending two-note chirp (Apple II ~880→1108 Hz)
            tone(880.0f, 80);
            gap(20);
            tone(1108.0f, 100);
            break;

        case SND_EVENT_BAD:
            // Descending minor three notes (Apple II ~440→349→294)
            tone(440.0f, 80);
            gap(20);
            tone(349.0f, 80);
            gap(20);
            tone(294.0f, 120);
            break;

        case SND_PLAYER_DEATH:
            // Classic descending chromatic run (Apple II G4→C4)
            tone(392.0f, 120);
            gap(30);
            tone(370.0f, 120);
            gap(30);
            tone(349.0f, 120);
            gap(30);
            tone(330.0f, 120);
            gap(30);
            tone(294.0f, 120);
            gap(30);
            tone(262.0f, 200);
            break;

        case SND_GAME_OVER:
            // Full death jingle — longer, more dramatic
            tone(392.0f, 150);
            gap(40);
            tone(370.0f, 150);
            gap(40);
            tone(349.0f, 150);
            gap(40);
            tone(330.0f, 150);
            gap(40);
            tone(294.0f, 150);
            gap(40);
            tone(262.0f, 150);
            gap(40);
            tone(247.0f, 300);
            break;

        case SND_FORT_ARRIVE:
            // Two-note ascending fanfare (C5→E5, Apple II ~523→659)
            tone(523.0f, 120);
            gap(30);
            tone(659.0f, 200);
            break;

        case SND_RIVER_SPLASH:
            // Rapid low pulse — danger at the crossing
            for(int i = 0; i < 4; i++) {
                tone(220.0f, 30);
                gap(20);
            }
            tone(160.0f, 60);
            break;

        case SND_RIVER_SAFE:
            // Gentle resolved two notes — you made it
            tone(523.0f, 80);
            gap(20);
            tone(659.0f, 120);
            break;

        case SND_WIN:
            // Triumphant ascending run
            tone(523.0f, 80);
            gap(15);
            tone(587.0f, 80);
            gap(15);
            tone(659.0f, 80);
            gap(15);
            tone(784.0f, 80);
            gap(15);
            tone(1047.0f, 200);
            break;
    }
}
