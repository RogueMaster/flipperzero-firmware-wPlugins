#include "mf_rx_practice_core.h"

#include <limits.h>
#include <string.h>

#include "../../cw.h"
#include "../../morse_flipper_time.h"

static MfRxPracticeResult mf_result(
    const MfRxPracticeState* state,
    bool handled,
    bool redraw,
    bool decoder_reset,
    bool request_exit,
    MfRxPracticeFeedback feedback) {
    return (MfRxPracticeResult){
        .handled = handled,
        .redraw = redraw,
        .decoder_reset = decoder_reset,
        .request_exit = request_exit,
        .phase = state == NULL ? MfRxPracticePhaseFinal : state->phase,
        .playback_mark = state != NULL && state->playback_mark,
        .feedback = feedback,
    };
}

static bool mf_begin_round(MfRxPracticeState* state, uint32_t now_ms) {
    MfCallsign call;
    bool ok;
    state->answer[0] = '\0';
    state->answer_len = 0U;
    state->answer_started = false;
    state->playback_char = 0U;
    state->playback_mark_index = 0U;
    state->playback_mark = false;
    state->target_len = mf_callsign_pick_length(&state->rng);
    ok = mf_callsign_generate(&state->callsigns, &state->rng, state->target_len, &call);
    if(ok) memcpy(state->target, call.text, sizeof(state->target));
    if(!ok || state->target_len == 0U || cw_symbol_count(cw(state->target[0])) == 0U) {
        state->phase = MfRxPracticePhaseFinal;
        state->internal_error = true;
        return false;
    }
    state->phase = MfRxPracticePhasePlayback;
    state->playback_mark = true;
    state->next_at = now_ms +
                     (uint32_t)state->dit_ms * cw_symbol_units(cw(state->target[0]), 0U);
    return true;
}

static MfRxPracticeResult mf_finish_answer(
    MfRxPracticeState* state,
    uint32_t now_ms,
    MfRxPracticeFeedback feedback) {
    state->phase = MfRxPracticePhaseResult;
    state->playback_mark = false;
    state->last_passed = feedback == MfRxPracticeFeedbackPass;
    if(state->session_total != UINT16_MAX) {
        state->session_total++;
        if(state->last_passed) state->session_passed++;
    }
    state->result_deadline = now_ms + state->result_hold_ms;
    uint32_t countdown_s = (state->result_hold_ms + 999U) / 1000U;
    state->countdown_draw_s = countdown_s > UINT8_MAX ? UINT8_MAX : (uint8_t)countdown_s;
    return mf_result(state, true, true, true, false, feedback);
}

static char mf_normalize(char ch) {
    if(ch >= 'a' && ch <= 'z') ch = (char)(ch - ('a' - 'A'));
    if(ch >= 'A' && ch <= 'Z') return ch;
    if(ch >= '0' && ch <= '9') return ch;
    return '\0';
}

static MfRxPracticeResult mf_hurry(
    MfRxPracticeState* state,
    uint32_t now_ms) {
    bool shortened = false;
    if(morse_flipper_time_pending(now_ms, state->result_deadline) &&
       state->result_deadline - now_ms > 1000U) {
        state->result_deadline = now_ms + 1000U;
        state->countdown_draw_s = 0xFFU;
        shortened = true;
    }
    return mf_result(
        state, true, shortened, false, false, MfRxPracticeFeedbackNone);
}

bool mf_rx_practice_enter(
    MfRxPracticeState* state,
    const MfRxPracticeEnterArgs* args,
    MfRxPracticeResult* initial) {
    if(state != NULL) *state = (MfRxPracticeState){0};
    if(initial != NULL) *initial = (MfRxPracticeResult){0};
    if(state == NULL || args == NULL || initial == NULL || args->struct_size < sizeof(*args) ||
       args->dit_ms == 0U || args->char_gap_ms == 0U || args->answer_timeout_ms == 0U ||
       args->result_hold_ms == 0U ||
       args->answer_timeout_ms >= INT32_MAX || args->result_hold_ms >= INT32_MAX)
        return false;
    state->phase = MfRxPracticePhaseIdle;
    state->answer_timeout_ms = args->answer_timeout_ms;
    state->result_hold_ms = args->result_hold_ms;
    state->dit_ms = args->dit_ms;
    state->char_gap_ms = args->char_gap_ms;
    state->physical_key_can_start = args->physical_key_can_start;
    state->draw_services = args->draw_services;
    state->countdown_draw_s = 0xFFU;
    mf_rx_rng_init(&state->rng, args->rng_seed);
    mf_callsign_gen_init(&state->callsigns);
    *initial = mf_result(state, true, true, true, false, MfRxPracticeFeedbackNone);
    return true;
}

void mf_rx_practice_leave(MfRxPracticeState* state) {
    if(state != NULL) *state = (MfRxPracticeState){0};
}

MfRxPracticeResult mf_rx_practice_command(
    MfRxPracticeState* state,
    MfRxPracticeCommand command,
    uint32_t now_ms) {
    if(state == NULL) return mf_result(NULL, false, false, false, false, MfRxPracticeFeedbackNone);
    if(command == MfRxPracticeCommandNone) return mf_result(state, false, false, false, false, MfRxPracticeFeedbackNone);
    if(command == MfRxPracticeCommandExit)
        return mf_result(state, true, false, true, true, MfRxPracticeFeedbackClear);
    if(command == MfRxPracticeCommandBack) {
        if(state->phase == MfRxPracticePhaseIdle || state->phase == MfRxPracticePhaseFinal)
            return mf_result(state, true, false, true, true, MfRxPracticeFeedbackClear);
        state->phase = MfRxPracticePhaseFinal;
        state->playback_mark = false;
        return mf_result(state, true, true, true, false, MfRxPracticeFeedbackClear);
    }
    if(command == MfRxPracticeCommandConfirmExit && state->phase == MfRxPracticePhaseFinal)
        return mf_result(state, true, false, true, true, MfRxPracticeFeedbackClear);
    if(command == MfRxPracticeCommandPrimaryPress) {
        if(state->phase == MfRxPracticePhaseIdle) {
            bool ok = mf_begin_round(state, now_ms);
            (void)ok;
            return mf_result(
                state, true, true, true, false, MfRxPracticeFeedbackClear);
        }
        if(state->phase == MfRxPracticePhaseResult)
            return mf_hurry(state, now_ms);
    }
    if(command == MfRxPracticeCommandPaddleBackPress) {
        if(state->phase == MfRxPracticePhaseFinal)
            return mf_result(
                state, true, false, true, true, MfRxPracticeFeedbackClear);
        if(state->phase == MfRxPracticePhaseIdle) {
            bool ok = mf_begin_round(state, now_ms);
            (void)ok;
            return mf_result(
                state, true, true, true, false, MfRxPracticeFeedbackClear);
        }
        if(state->phase == MfRxPracticePhaseResult)
            return mf_hurry(state, now_ms);
    }
    if(command == MfRxPracticeCommandStart && state->phase == MfRxPracticePhaseIdle) {
        bool ok = mf_begin_round(state, now_ms);
        (void)ok;
        return mf_result(state, true, true, true, false, MfRxPracticeFeedbackClear);
    }
    if(command == MfRxPracticeCommandAnswerActivity &&
       state->phase == MfRxPracticePhaseAnswer && !state->answer_started) {
        state->answer_started = true;
        state->answer_last_activity_ms = now_ms;
        return mf_result(state, true, false, false, false, MfRxPracticeFeedbackNone);
    }
    if(command == MfRxPracticeCommandBackspace && state->phase == MfRxPracticePhaseAnswer) {
        bool changed = state->answer_len != 0U;
        if(changed) {
            state->answer[--state->answer_len] = '\0';
            state->answer_last_activity_ms = now_ms;
        }
        return mf_result(state, true, changed, true, false, MfRxPracticeFeedbackNone);
    }
    if(command == MfRxPracticeCommandClear && state->phase == MfRxPracticePhaseAnswer) {
        bool changed = state->answer_len != 0U;
        if(changed) {
            state->answer_len = 0U;
            state->answer[0] = '\0';
            state->answer_last_activity_ms = now_ms;
        }
        return mf_result(state, true, changed, true, false, MfRxPracticeFeedbackNone);
    }
    if(command == MfRxPracticeCommandHurry && state->phase == MfRxPracticePhaseResult) {
        return mf_hurry(state, now_ms);
    }
    return mf_result(state, false, false, false, false, MfRxPracticeFeedbackNone);
}

MfRxPracticeResult mf_rx_practice_feed_text(
    MfRxPracticeState* state,
    const char* text,
    size_t text_len,
    uint32_t now_ms) {
    if(state == NULL || state->phase != MfRxPracticePhaseAnswer || text == NULL)
        return mf_result(state, false, false, false, false, MfRxPracticeFeedbackNone);
    if(text_len > MF_RX_PRACTICE_FEED_MAX) text_len = MF_RX_PRACTICE_FEED_MAX;
    bool accepted = false;
    for(size_t i = 0U; i < text_len; i++) {
        char ch = mf_normalize(text[i]);
        if(ch == '\0') continue;
        if(state->answer_len < state->target_len) {
            state->answer[state->answer_len++] = ch;
            state->answer[state->answer_len] = '\0';
            state->answer_started = true;
            state->answer_last_activity_ms = now_ms;
            accepted = true;
        }
        if(state->answer_len == state->target_len)
            return mf_finish_answer(
                state,
                now_ms,
                strcmp(state->answer, state->target) == 0 ? MfRxPracticeFeedbackPass :
                                                             MfRxPracticeFeedbackFail);
    }
    return mf_result(state, accepted, accepted, false, false, MfRxPracticeFeedbackNone);
}

MfRxPracticeResult mf_rx_practice_tick(MfRxPracticeState* state, uint32_t now_ms) {
    uint8_t marks;
    uint16_t code;
    if(state == NULL) return mf_result(NULL, false, false, false, false, MfRxPracticeFeedbackNone);
    if(state->phase == MfRxPracticePhaseAnswer && state->answer_started &&
       now_ms - state->answer_last_activity_ms >= state->answer_timeout_ms)
        return mf_finish_answer(state, now_ms, MfRxPracticeFeedbackTimeout);
    if(state->phase == MfRxPracticePhaseResult) {
        if(morse_flipper_time_reached(now_ms, state->result_deadline)) {
            bool ok = mf_begin_round(state, now_ms);
            (void)ok;
            return mf_result(state, true, true, true, false, MfRxPracticeFeedbackClear);
        }
        uint32_t remaining_ms = state->result_deadline - now_ms;
        uint32_t remaining_s = (remaining_ms + 999U) / 1000U;
        if(remaining_s > UINT8_MAX) remaining_s = UINT8_MAX;
        if(state->countdown_draw_s != (uint8_t)remaining_s) {
            state->countdown_draw_s = (uint8_t)remaining_s;
            return mf_result(state, true, true, false, false, MfRxPracticeFeedbackNone);
        }
        return mf_result(state, false, false, false, false, MfRxPracticeFeedbackNone);
    }
    if(state->phase != MfRxPracticePhasePlayback || !morse_flipper_time_reached(now_ms, state->next_at))
        return mf_result(state, false, false, false, false, MfRxPracticeFeedbackNone);
    code = cw(state->target[state->playback_char]);
    marks = cw_symbol_count(code);
    if(marks == 0U || state->playback_mark_index >= marks) {
        state->phase = MfRxPracticePhaseFinal;
        state->playback_mark = false;
        state->internal_error = true;
        return mf_result(state, true, true, true, false, MfRxPracticeFeedbackClear);
    }
    if(state->playback_mark) {
        state->playback_mark = false;
        if(state->playback_mark_index + 1U < marks) {
            state->playback_mark_index++;
            state->next_at = now_ms + state->dit_ms;
        } else if(state->playback_char + 1U >= state->target_len) {
            state->phase = MfRxPracticePhaseAnswer;
            state->answer_started = false;
            state->answer_last_activity_ms = now_ms;
        } else {
            state->playback_char++;
            state->playback_mark_index = 0U;
            state->next_at = now_ms + state->char_gap_ms;
        }
    } else {
        state->playback_mark = true;
        state->next_at = now_ms +
                         (uint32_t)state->dit_ms *
                             cw_symbol_units(cw(state->target[state->playback_char]), state->playback_mark_index);
    }
    return mf_result(state, true, true, state->phase == MfRxPracticePhaseAnswer, false, MfRxPracticeFeedbackNone);
}
