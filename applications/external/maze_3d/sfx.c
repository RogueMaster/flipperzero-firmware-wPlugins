#include "maze3d.h"
#include <furi_hal.h>
#include <furi_hal_speaker.h>

// Simple sequencer: one note = frequency + duration in ticks.
typedef struct {
    uint16_t freq; // Hz, 0 = rest
    uint8_t ticks;
} Note;

#define MAX_SEQ_NOTES 8
typedef struct {
    Note notes[MAX_SEQ_NOTES];
    uint8_t count;
} Seq;

#define C5   523
#define D5   587
#define E5   659
#define G5   784
#define C6   1046
#define E6   1318
#define G6   1568
#define REST 0

static const Seq s_seqs[SFX_COUNT] = {
    [SFX_NONE] = {{}, 0},
    [SFX_MENU_MOVE] = {{{C6, 1}}, 1},
    [SFX_MENU_OK] = {{{E6, 1}, {G6, 1}}, 2},
    [SFX_LEVEL_CLEAR] = {{{C5, 1}, {D5, 1}, {E5, 1}, {G5, 1}, {C6, 3}}, 5},
    [SFX_STEP] = {{{E6, 1}}, 1},
};

static const Seq* s_cur = NULL;
static uint8_t s_note = 0;
static uint8_t s_tick = 0;

void sfx_init(void) {
    s_cur = NULL;
    s_note = 0;
    s_tick = 0;
}

void sfx_deinit(void) {
    furi_hal_speaker_stop();
}

void sfx_play(SfxType t) {
    if(t <= SFX_NONE || t >= SFX_COUNT) return;
    s_cur = &s_seqs[t];
    s_note = 0;
    s_tick = 0;
}

void sfx_tick_update(void) {
    if(!s_cur || s_note >= s_cur->count) {
        furi_hal_speaker_stop();
        s_cur = NULL;
        return;
    }
    Note n = s_cur->notes[s_note];
    if(s_tick == 0) {
        if(n.freq != 0) {
            furi_hal_speaker_start(n.freq, 0.5f);
        } else {
            furi_hal_speaker_stop();
        }
    }
    s_tick++;
    if(s_tick >= n.ticks) {
        s_note++;
        s_tick = 0;
    }
}
