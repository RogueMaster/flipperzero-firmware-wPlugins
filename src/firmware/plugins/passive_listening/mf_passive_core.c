#include "mf_passive_core.h"

#include <limits.h>
#include <string.h>

#include "../../cw.h"

static bool mf_passive_reached(uint32_t now, uint32_t deadline) {
    return (int32_t)(now - deadline) >= 0;
}

static MfPassiveResult mf_passive_result(const MfPassiveState* state, bool redraw) {
    return (MfPassiveResult){.handled = true, .redraw = redraw, .phase = state->phase, .error = state->error};
}

static void mf_passive_fail(MfPassiveState* state) {
    state->error = 1U;
    state->phase = MfPassivePhaseError;
    mf_passive_voice_pack_close(&state->pack);
    if(state->services != NULL) {
        state->services->set_silence(state->services->context);
        state->services->set_vibration(state->services->context, false);
    }
}

static bool mf_passive_start_mark(MfPassiveState* state, uint32_t now) {
    uint8_t symbol = cw(state->callsign.text[state->char_index]);
    if(symbol == CW_INVALID || state->mark_index >= cw_symbol_count(symbol)) return false;
    if(!state->services->set_tone(state->services->context, state->tone_hz)) return false;
    state->cw_mark = true;
    state->next_at = now + cw_symbol_units(symbol, state->mark_index) * state->dit_ms;
    return true;
}

static bool mf_passive_next_call(MfPassiveState* state) {
    MfCallsign previous = state->callsign;
    for(uint8_t tries = 0U; tries < 32U; tries++) {
        if(mf_callsign_generate(&state->callsign_gen, &state->rng, 4U, &state->callsign) &&
           memcmp(previous.text, state->callsign.text, 4U) != 0)
            return true;
    }
    return false;
}

bool mf_passive_enter(MfPassiveState* state, const MfPassiveEnterArgs* args, MfPassiveResult* result) {
    if(state == NULL || result == NULL || args == NULL || args->struct_size != sizeof(*args) ||
       args->services == NULL || args->services->struct_size != sizeof(MfPassiveHostServices) ||
       args->services->claim == NULL || args->services->set_silence == NULL ||
       args->services->set_tone == NULL || args->services->set_voice == NULL ||
       args->services->set_vibration == NULL || args->services->release == NULL || args->dit_ms == 0U ||
       args->char_gap_ms == 0U || args->tone_hz == 0U || args->output_target > MfPassiveOutputP2)
        return false;
    memset(state, 0, sizeof(*state));
    state->services = args->services;
    state->dit_ms = args->dit_ms;
    state->char_gap_ms = args->char_gap_ms;
    state->tone_hz = args->tone_hz;
    mf_rx_rng_init(&state->rng, args->rng_seed);
    mf_callsign_gen_init(&state->callsign_gen);
    if(!mf_passive_voice_pack_open_asset(&state->pack) || !mf_passive_next_call(state) ||
       !state->services->claim(state->services->context, args->output_target, args->volume_pct, &state->pipe)) {
        mf_passive_fail(state);
        *result = mf_passive_result(state, true);
        return false;
    }
    state->audio_claimed = true;
    state->phase = MfPassivePhaseCw;
    if(!mf_passive_start_mark(state, args->now_ms)) mf_passive_fail(state);
    *result = mf_passive_result(state, true);
    return state->error == 0U;
}

void mf_passive_leave(MfPassiveState* state) {
    if(state == NULL) return;
    if(state->services != NULL) {
        state->services->set_silence(state->services->context);
        state->services->set_vibration(state->services->context, false);
        if(state->audio_claimed) state->services->release(state->services->context);
    }
    mf_passive_voice_pack_close(&state->pack);
    memset(state, 0, sizeof(*state));
}

MfPassiveResult mf_passive_input(MfPassiveState* state, const InputEvent* event, uint32_t now_ms) {
    MfPassiveResult result;
    if(state == NULL || event == NULL) return (MfPassiveResult){0};
    if(event->key == InputKeyBack && event->type == InputTypeLong) state->back_clicks = 0U;
    else if(event->key == InputKeyBack && event->type == InputTypeShort) {
        if(state->back_clicks == 0U || (uint32_t)(now_ms - state->last_back_at) > 700U) state->back_clicks = 1U;
        else state->back_clicks++;
        state->last_back_at = now_ms;
    } else if(event->type == InputTypePress || event->type == InputTypeShort) state->back_clicks = 0U;
    result = mf_passive_result(state, false);
    if(state->back_clicks >= 3U) result.request_exit = true;
    return result;
}

MfPassiveResult mf_passive_tick(MfPassiveState* state, uint32_t now_ms) {
    if(state == NULL) return (MfPassiveResult){0};
    if(state->phase == MfPassivePhaseCw && mf_passive_reached(now_ms, state->next_at)) {
        uint8_t symbol = cw(state->callsign.text[state->char_index]);
        if(state->cw_mark) {
            state->services->set_silence(state->services->context);
            state->cw_mark = false;
            state->mark_index++;
            if(state->mark_index < cw_symbol_count(symbol)) state->next_at = now_ms + state->dit_ms;
            else if(state->char_index == 3U) { state->phase = MfPassivePhasePostCw; state->next_at = now_ms + 3000U; }
            else { state->char_index++; state->mark_index = 0U; state->next_at = now_ms + state->char_gap_ms; }
        } else if(!mf_passive_start_mark(state, now_ms)) mf_passive_fail(state);
        return mf_passive_result(state, false);
    }
    if(state->phase == MfPassivePhasePostCw) {
        if(!state->pack.active && state->pipe.read_pos == state->pipe.write_pos &&
           !mf_passive_voice_pack_begin(&state->pack, &state->pipe, state->callsign.text[0]))
            mf_passive_fail(state);
        mf_passive_voice_pack_refill(&state->pack, &state->pipe);
        if(mf_passive_voice_pack_failed(&state->pack)) mf_passive_fail(state);
        if(state->phase != MfPassivePhaseError && mf_passive_reached(now_ms, state->next_at))
            state->phase = MfPassivePhaseVoicePrime;
        return mf_passive_result(state, false);
    }
    if(state->phase == MfPassivePhaseVoicePrime) {
        if(!state->pack.active && state->pipe.read_pos == state->pipe.write_pos &&
           !mf_passive_voice_pack_begin(
               &state->pack, &state->pipe, state->callsign.text[state->voice_index]))
            mf_passive_fail(state);
        mf_passive_voice_pack_refill(&state->pack, &state->pipe);
        if(mf_passive_voice_pack_failed(&state->pack)) mf_passive_fail(state);
        if(state->phase != MfPassivePhaseError &&
           mf_passive_voice_pack_primed(&state->pack, &state->pipe)) {
            if(!state->services->set_voice(state->services->context, state->pack.sample_rate_hz))
                mf_passive_fail(state);
            else {
                state->revealed_count = (uint8_t)(state->voice_index + 1U);
                state->phase = MfPassivePhaseVoice;
            }
        }
        return mf_passive_result(state, state->phase == MfPassivePhaseVoice);
    }
    if(state->phase == MfPassivePhaseVoice) {
        mf_passive_voice_pack_refill(&state->pack, &state->pipe);
        if(mf_passive_voice_pack_failed(&state->pack)) mf_passive_fail(state);
        else if(mf_passive_voice_pack_drained(&state->pack, &state->pipe)) {
            if(state->voice_index == 3U) {
                state->phase = MfPassivePhasePostVoice;
                state->next_at = now_ms + 1000U;
            } else {
                state->voice_index++;
                state->phase = MfPassivePhaseVoicePrime;
            }
        }
        return mf_passive_result(state, false);
    }
    return mf_passive_result(state, false);
}
