#pragma once

#include "mf_passive_types.h"
#include "mf_passive_voice_pack.h"

typedef struct {
    MfRxRng rng;
    MfCallsignGen callsign_gen;
    MfCallsign callsign;
    MfPassivePcmPipe pipe;
    MfPassiveVoicePack pack;
    const MfPassiveHostServices* services;
    uint32_t next_at;
    uint32_t last_back_at;
    uint16_t dit_ms;
    uint16_t char_gap_ms;
    uint16_t tone_hz;
    uint16_t answer_delay_ms;
    uint8_t phase;
    uint8_t char_index;
    uint8_t mark_index;
    uint8_t voice_index;
    uint8_t revealed_count;
    uint8_t back_clicks;
    uint8_t voice_gain_pct;
    uint8_t mode;
    uint8_t prompt_length;
    uint8_t lesson_charset_len;
    uint8_t vibrate;
    uint8_t repeat_after_answer;
    uint8_t error;
    char lesson_charset[MF_PASSIVE_LESSON_CHARSET_CAP];
    bool cw_mark;
    bool audio_claimed;
} MfPassiveState;

bool mf_passive_enter(MfPassiveState* state, const MfPassiveEnterArgs* args, MfPassiveResult* result);
void mf_passive_leave(MfPassiveState* state);
MfPassiveResult mf_passive_input(MfPassiveState* state, const InputEvent* event, uint32_t now_ms);
MfPassiveResult mf_passive_tick(MfPassiveState* state, uint32_t now_ms);
