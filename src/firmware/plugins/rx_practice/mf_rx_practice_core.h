#pragma once

#include "../common/mf_callsign_gen.h"
#include "morse_flipper_rx_practice_types.h"

typedef struct MfRxPracticeState {
    MfRxPracticePhase phase;
    MfRxRng rng;
    MfCallsignGen callsigns;
    char target[MF_CALLSIGN_MAX_LEN + 1U];
    char answer[MF_CALLSIGN_MAX_LEN + 1U];
    uint32_t next_at;
    uint32_t answer_last_activity_ms;
    uint32_t answer_timeout_ms;
    uint32_t result_deadline;
    uint32_t result_hold_ms;
    uint16_t dit_ms;
    uint16_t char_gap_ms;
    uint16_t session_total;
    uint16_t session_passed;
    uint8_t target_len;
    uint8_t answer_len;
    uint8_t playback_char;
    uint8_t playback_mark_index;
    uint8_t countdown_draw_s;
    bool playback_mark;
    bool answer_started;
    bool last_passed;
    bool internal_error;
    bool physical_key_can_start;
    bool button_paddle;
    const MfRxPracticeDrawSnapshot* draw_snapshot;
} MfRxPracticeState;

_Static_assert(sizeof(MfRxPracticeState) <= 256U, "RX Practice state exceeds its hard memory budget");

bool mf_rx_practice_enter(
    MfRxPracticeState* state,
    const MfRxPracticeEnterArgs* args,
    MfRxPracticeResult* initial);
void mf_rx_practice_leave(MfRxPracticeState* state);
MfRxPracticeResult mf_rx_practice_command(
    MfRxPracticeState* state,
    MfRxPracticeCommand command,
    uint32_t now_ms);
MfRxPracticeResult mf_rx_practice_feed_text(
    MfRxPracticeState* state,
    const char* text,
    size_t text_len,
    uint32_t now_ms);
MfRxPracticeResult mf_rx_practice_tick(MfRxPracticeState* state, uint32_t now_ms);
