#include "mf_passive_core.h"

#include <limits.h>
#include <string.h>

#include "../../cw.h"

#define MF_PASSIVE_INITIAL_CW_MS    1000U
#define MF_PASSIVE_BETWEEN_TOKEN_MS 100U
#define MF_PASSIVE_POST_VOICE_MS    1000U
#define MF_PASSIVE_CUE_MS           120U
#define MF_PASSIVE_POST_CUE_MS      1000U
#define MF_PASSIVE_MAX_UNDERRUNS     64U

static bool mf_passive_reached(uint32_t now, uint32_t deadline) {
    return (int32_t)(now - deadline) >= 0;
}

static MfPassiveResult mf_passive_result(const MfPassiveState* state, bool redraw) {
    return (MfPassiveResult){.handled = true, .redraw = redraw, .phase = state->phase, .error = state->error};
}

static void mf_passive_reset_pipe(MfPassivePcmPipe* pipe) {
    if(pipe == NULL) return;
    pipe->read_pos = 0U;
    pipe->write_pos = 0U;
    pipe->eof = false;
    pipe->drained = false;
    pipe->underruns = 0U;
}

static bool mf_passive_silence(MfPassiveState* state) {
    if(!mf_passive_host_command(state ? state->services : NULL, MfPassiveHostCommandSilence, 0U)) return false;
    mf_passive_reset_pipe(&state->pipe);
    return true;
}

static void mf_passive_fail(MfPassiveState* state) {
    if(state == NULL) return;
    state->error = 1U;
    state->phase = MfPassivePhaseError;
    mf_passive_host_command(state->services, MfPassiveHostCommandSilence, 0U);
    mf_passive_host_command(state->services, MfPassiveHostCommandVibration, 0U);
    mf_passive_reset_pipe(&state->pipe);
    if(state->audio_claimed) {
        mf_passive_host_command(state->services, MfPassiveHostCommandRelease, 0U);
        state->audio_claimed = false;
    }
    mf_passive_voice_pack_close(&state->pack);
}

static bool mf_passive_start_mark(MfPassiveState* state, uint32_t now) {
    uint8_t symbol = cw(state->prompt[state->char_index]);
    if(symbol == CW_INVALID || state->mark_index >= cw_symbol_count(symbol) ||
       !mf_passive_host_command(state->services, MfPassiveHostCommandTone, state->tone_hz))
        return false;
    state->cw_mark = true;
    state->next_at = now + cw_symbol_units(symbol, state->mark_index) * state->dit_ms;
    return true;
}

static bool mf_passive_next_prompt(MfPassiveState* state) {
    char previous[MF_CALLSIGN_MAX_LEN + 1U];
    char candidate[MF_CALLSIGN_MAX_LEN + 1U];
    uint8_t length;

    if(state == NULL || state->prompt_length < 3U || state->prompt_length > MF_CALLSIGN_MAX_LEN)
        return false;
    length = state->prompt_length;
    memcpy(previous, state->prompt, sizeof(previous));
    for(uint8_t tries = 0U; tries < 32U; tries++) {
        if(state->mode == 0U) {
            if(!mf_callsign_generate(&state->callsign_gen, &state->rng, length, &state->callsign))
                continue;
            memcpy(candidate, state->callsign.text, length);
        } else {
            for(uint8_t i = 0U; i < length; i++)
                candidate[i] =
                    state->lesson_charset[mf_rx_rng_bounded(&state->rng, state->lesson_charset_len)];
        }
        candidate[length] = '\0';
        if(memcmp(previous, candidate, length) != 0) {
            memcpy(state->prompt, candidate, sizeof(state->prompt));
            return true;
        }
    }
    return false;
}

static bool mf_passive_start_round(MfPassiveState* state, uint32_t now) {
    if(!mf_passive_next_prompt(state)) return false;
    state->prompt_len = state->prompt_length;
    state->char_index = 0U;
    state->mark_index = 0U;
    state->voice_index = 0U;
    state->revealed_count = 0U;
    state->cw_mark = false;
    mf_passive_reset_pipe(&state->pipe);
    state->phase = MfPassivePhaseCw;
    return mf_passive_start_mark(state, now);
}

static bool mf_passive_prime_token(MfPassiveState* state) {
    if(!state->pack.active && state->pipe.read_pos == state->pipe.write_pos &&
       !mf_passive_voice_pack_begin(
           &state->pack, &state->pipe, state->prompt[state->voice_index]))
        return false;
    mf_passive_voice_pack_refill(&state->pack, &state->pipe, state->voice_gain_pct);
    return !mf_passive_voice_pack_failed(&state->pack);
}

static bool mf_passive_start_voice(MfPassiveState* state) {
    if(!mf_passive_prime_token(state)) return false;
    if(!mf_passive_voice_pack_primed(&state->pack, &state->pipe)) return true;
    if(!mf_passive_host_command(
           state->services, MfPassiveHostCommandVoice, state->pack.sample_rate_hz))
        return false;
    state->revealed_count = (uint8_t)(state->voice_index + 1U);
    state->phase = MfPassivePhaseVoice;
    return true;
}

bool mf_passive_enter(MfPassiveState* state, const MfPassiveEnterArgs* args, MfPassiveResult* result) {
    if(state == NULL || result == NULL || args == NULL || args->struct_size != sizeof(*args) ||
       args->services == NULL || args->services->struct_size != sizeof(MfPassiveHostServices) ||
       args->services->command == NULL || args->dit_ms == 0U || args->answer_delay_ms < 1000U ||
       args->answer_delay_ms > 5000U || args->mode > 1U || args->prompt_length < 3U ||
       args->prompt_length > 6U || (args->mode == 0U && args->prompt_length < 4U) ||
       args->lesson_charset_len > MF_PASSIVE_LESSON_CHARSET_CAP ||
       (args->mode == 1U && args->lesson_charset_len == 0U) || args->vibrate > 1U ||
       args->repeat_after_answer > 1U ||
       args->char_gap_ms == 0U || args->tone_hz == 0U || args->output_target > MfPassiveOutputP2 ||
       args->volume_pct < 10U || args->volume_pct > 100U)
        return false;
    memset(state, 0, sizeof(*state));
    state->services = args->services;
    state->dit_ms = args->dit_ms;
    state->char_gap_ms = args->char_gap_ms;
    state->tone_hz = args->tone_hz;
    state->answer_delay_ms = args->answer_delay_ms;
    state->mode = args->mode;
    state->prompt_length = args->prompt_length;
    state->lesson_charset_len = args->lesson_charset_len;
    state->vibrate = args->vibrate;
    state->repeat_after_answer = args->repeat_after_answer;
    memcpy(state->lesson_charset, args->lesson_charset, sizeof(state->lesson_charset));
    /* Hardware audition selected 70% before SoftBuzz's doubled internal drive. */
    state->voice_gain_pct = args->output_target == MfPassiveOutputInternal ? 70U : 100U;
    mf_rx_rng_init(&state->rng, args->rng_seed);
    mf_callsign_gen_init(&state->callsign_gen);
    if(!mf_passive_voice_pack_open_asset(&state->pack) || !mf_passive_next_prompt(state) ||
       !mf_passive_host_claim(
           state->services, args->output_target, args->tone_hz, args->volume_pct, &state->pipe)) {
        mf_passive_fail(state);
        *result = mf_passive_result(state, true);
        return false;
    }
    state->prompt_len = state->prompt_length;
    state->audio_claimed = true;
    state->phase = MfPassivePhasePrepare;
    *result = mf_passive_result(state, true);
    return state->error == 0U;
}

void mf_passive_leave(MfPassiveState* state) {
    if(state == NULL) return;
    mf_passive_host_command(state->services, MfPassiveHostCommandSilence, 0U);
    mf_passive_host_command(state->services, MfPassiveHostCommandVibration, 0U);
    mf_passive_reset_pipe(&state->pipe);
    if(state->audio_claimed) mf_passive_host_command(state->services, MfPassiveHostCommandRelease, 0U);
    mf_passive_voice_pack_close(&state->pack);
    memset(state, 0, sizeof(*state));
}

MfPassiveResult mf_passive_input(MfPassiveState* state, const InputEvent* event, uint32_t now_ms) {
    MfPassiveResult result;
    if(state == NULL || event == NULL) return (MfPassiveResult){0};
    if((event->key == InputKeyUp || event->key == InputKeyDown) &&
       (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        uint8_t voice_gain_pct = state->voice_gain_pct;
        if(event->key == InputKeyUp)
            voice_gain_pct =
                voice_gain_pct < 95U ? (uint8_t)(voice_gain_pct + 5U) : 100U;
        else
            voice_gain_pct =
                voice_gain_pct > 15U ? (uint8_t)(voice_gain_pct - 5U) : 10U;
        state->back_clicks = 0U;
        state->voice_gain_pct = voice_gain_pct;
        return mf_passive_result(state, false);
    }
    if(event->key == InputKeyBack && event->type == InputTypeShort) {
        if(state->back_clicks == 0U || (uint32_t)(now_ms - state->last_back_at) > 700U) state->back_clicks = 1U;
        else state->back_clicks++;
        state->last_back_at = now_ms;
    } else if(event->key == InputKeyBack && event->type == InputTypeLong) {
        state->back_clicks = 0U;
    } else if(event->key != InputKeyBack &&
              (event->type == InputTypePress || event->type == InputTypeShort)) {
        state->back_clicks = 0U;
    }
    result = mf_passive_result(state, false);
    if(state->back_clicks >= 3U) result.request_exit = true;
    return result;
}

MfPassiveResult mf_passive_tick(MfPassiveState* state, uint32_t now_ms) {
    bool redraw = false;
    if(state == NULL) return (MfPassiveResult){0};
    if(state->phase == MfPassivePhasePrepare) {
        if(!state->cw_mark) {
            state->cw_mark = true;
            state->next_at = now_ms + MF_PASSIVE_INITIAL_CW_MS;
        } else if(mf_passive_reached(now_ms, state->next_at)) {
            state->cw_mark = false;
            state->phase = MfPassivePhaseCw;
            if(!mf_passive_start_mark(state, now_ms)) mf_passive_fail(state);
        }
        return mf_passive_result(state, false);
    }
    if(state->phase == MfPassivePhaseCw && mf_passive_reached(now_ms, state->next_at)) {
        uint8_t symbol = cw(state->prompt[state->char_index]);
        if(state->cw_mark) {
            if(!mf_passive_silence(state)) mf_passive_fail(state);
            state->cw_mark = false;
            state->mark_index++;
            if(state->phase != MfPassivePhaseError && state->mark_index < cw_symbol_count(symbol))
                state->next_at = now_ms + state->dit_ms;
            else if(state->phase != MfPassivePhaseError &&
                    state->char_index + 1U == state->prompt_len) {
                state->phase = MfPassivePhasePostCw;
                state->next_at = now_ms + state->answer_delay_ms;
            } else if(state->phase != MfPassivePhaseError) {
                state->char_index++;
                state->mark_index = 0U;
                state->next_at = now_ms + state->char_gap_ms;
            }
        } else if(!mf_passive_start_mark(state, now_ms)) {
            mf_passive_fail(state);
        }
        return mf_passive_result(state, false);
    }
    if(state->phase == MfPassivePhasePostCw) {
        if(!mf_passive_prime_token(state)) mf_passive_fail(state);
        if(state->phase != MfPassivePhaseError && mf_passive_reached(now_ms, state->next_at)) {
            state->phase = MfPassivePhaseVoicePrime;
            if(!mf_passive_start_voice(state)) mf_passive_fail(state);
            redraw = state->phase == MfPassivePhaseVoice;
        }
        return mf_passive_result(state, redraw);
    }
    if(state->phase == MfPassivePhaseVoicePrime) {
        if(!mf_passive_start_voice(state)) mf_passive_fail(state);
        return mf_passive_result(state, state->phase == MfPassivePhaseVoice);
    }
    if(state->phase == MfPassivePhaseVoice) {
        mf_passive_voice_pack_refill(&state->pack, &state->pipe, state->voice_gain_pct);
        if(mf_passive_voice_pack_failed(&state->pack)) {
            mf_passive_fail(state);
        } else if(state->pipe.underruns > MF_PASSIVE_MAX_UNDERRUNS) {
            mf_passive_fail(state);
        } else if(mf_passive_voice_pack_drained(&state->pack, &state->pipe)) {
            if(!mf_passive_silence(state)) {
                mf_passive_fail(state);
            } else if(state->voice_index + 1U == state->prompt_len) {
                state->phase = MfPassivePhasePostVoice;
                state->next_at = now_ms + MF_PASSIVE_POST_VOICE_MS;
            } else {
                state->voice_index++;
                state->phase = MfPassivePhaseBetweenTokens;
                state->next_at = now_ms + MF_PASSIVE_BETWEEN_TOKEN_MS;
                if(!mf_passive_prime_token(state)) mf_passive_fail(state);
            }
        }
        return mf_passive_result(state, false);
    }
    if(state->phase == MfPassivePhaseBetweenTokens) {
        if(!mf_passive_prime_token(state)) mf_passive_fail(state);
        if(state->phase != MfPassivePhaseError && mf_passive_reached(now_ms, state->next_at)) {
            state->phase = MfPassivePhaseVoicePrime;
            if(!mf_passive_start_voice(state)) mf_passive_fail(state);
            redraw = state->phase == MfPassivePhaseVoice;
        }
        return mf_passive_result(state, redraw);
    }
    if(state->phase == MfPassivePhasePostVoice && mf_passive_reached(now_ms, state->next_at)) {
        if(!mf_passive_host_command(state->services, MfPassiveHostCommandTone, state->tone_hz)) {
            mf_passive_fail(state);
        } else {
            mf_passive_host_command(state->services, MfPassiveHostCommandVibration, 1U);
            state->phase = MfPassivePhaseCue;
            state->next_at = now_ms + MF_PASSIVE_CUE_MS;
        }
        return mf_passive_result(state, false);
    }
    if(state->phase == MfPassivePhaseCue && mf_passive_reached(now_ms, state->next_at)) {
        if(!mf_passive_silence(state)) {
            mf_passive_fail(state);
        } else {
            mf_passive_host_command(state->services, MfPassiveHostCommandVibration, 0U);
            state->phase = MfPassivePhasePostCue;
            state->next_at = now_ms + MF_PASSIVE_POST_CUE_MS;
        }
        return mf_passive_result(state, false);
    }
    if(state->phase == MfPassivePhasePostCue && mf_passive_reached(now_ms, state->next_at)) {
        if(!mf_passive_start_round(state, now_ms)) mf_passive_fail(state);
        return mf_passive_result(state, state->phase == MfPassivePhaseCw);
    }
    return mf_passive_result(state, false);
}
