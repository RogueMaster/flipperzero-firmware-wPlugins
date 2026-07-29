#pragma once

#include "mf_passive_types.h"
#include "mf_passive_loading.h"
#include "mf_passive_policy.h"
#include "mf_passive_voice_pack.h"

typedef struct {
    MfRxRng rng;
    MfCallsignGen callsign_gen;
    MfCallsign callsign;
    char prompt[MF_CALLSIGN_MAX_LEN + 1U];
    MfPassivePcmPipe pipe;
    MfPassiveVoicePack pack;
    const MfPassiveHostServices* services;
    uint32_t next_at;
    uint32_t last_back_at;
    uint16_t dit_ms;
    uint16_t char_gap_ms;
    uint16_t tone_hz;
    uint16_t answer_delay_ms;
    uint16_t courtesy_delay_ms;
    uint8_t phase;
    uint8_t char_index;
    uint8_t mark_index;
    uint8_t voice_index;
    uint8_t revealed_count;
    uint8_t back_clicks;
    uint8_t voice_gain_pct;
    uint8_t mode;
    uint8_t length_setting;
    uint8_t prompt_length;
    uint8_t prompt_len;
    uint8_t lesson_charset_len;
    uint8_t vibrate;
    uint8_t repeat_after_answer;
    uint8_t error;
    uint8_t output_target;
    uint8_t volume_pct;
    char lesson_charset[MF_PASSIVE_LESSON_CHARSET_CAP];
    bool cw_mark;
    bool audio_claimed;
    MfPassiveLoading loading;
    MfPassiveSettingsModel settings_model;
} MfPassiveState;

bool mf_passive_enter(MfPassiveState* state, const MfPassiveEnterArgs* args, MfPassiveResult* result);
void mf_passive_leave(MfPassiveState* state);
MfPassiveResult mf_passive_input(MfPassiveState* state, const InputEvent* event, uint32_t now_ms);
MfPassiveResult mf_passive_tick(MfPassiveState* state, uint32_t now_ms);
