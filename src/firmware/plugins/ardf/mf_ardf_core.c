#include "mf_ardf_core.h"

#include "../../cw.h"
#include "mf_ardf_settings.h"

#include <limits.h>
#include <string.h>

#define MF_ARDF_MAX_START_LATE_MS 5U

static const char* const identifier_text[MfArdfMessageCount] = {
    "MOE",
    "MOI",
    "MOS",
    "MOH",
    "MO5",
    "S",
    "MO",
};
static const char* const identifier_morse[MfArdfMessageCount] = {
    "-- --- .",
    "-- --- ..",
    "-- --- ...",
    "-- --- ....",
    "-- --- .....",
    "...",
    "-- ---",
};

static MorseFlipperMappedFalResult mf_ardf_result(MfArdfState* state, bool handled) {
    MorseFlipperMappedFalResult result = {
        .handled = handled,
        .redraw = handled || (state != NULL && state->redraw_pending),
        .phase = state != NULL ? state->snapshot.view : 0U,
        .playback_active = state != NULL && state->snapshot.settings.audio_output &&
                           state->snapshot.transmitting,
        .playback_mark = state != NULL && state->snapshot.settings.audio_output &&
                         state->snapshot.transmitting && state->snapshot.mark,
        .backlight_wake = state != NULL && state->wake_pending,
        .backlight_off = state != NULL && state->backlight_off_pending,
        .transition = state != NULL && state->snapshot.run_pending,
        .feedback = state != NULL ? state->snapshot.host_action : 0U,
    };
    if(state != NULL) {
        state->wake_pending = false;
        state->backlight_off_pending = false;
        state->redraw_pending = false;
    }
    return result;
}

static MorseFlipperMappedFalResult mf_ardf_tick_result(MfArdfState* state, bool handled) {
    bool redraw = state != NULL && state->redraw_pending;
    MorseFlipperMappedFalResult result = mf_ardf_result(state, handled);
    result.redraw = redraw;
    return result;
}

bool mf_ardf_hardware_ops_valid(const MfArdfHardwareOps* ops) {
    return ops != NULL && ops->frequency_allowed != NULL && ops->prepare != NULL &&
           ops->set_mark != NULL && ops->stop != NULL && ops->set_p15 != NULL &&
           ops->set_p16 != NULL && ops->set_led != NULL && ops->set_clock != NULL;
}

const char* mf_ardf_identifier_text(MfArdfMessage message) {
    return message < MfArdfMessageCount ? identifier_text[message] : "";
}

const char* mf_ardf_identifier_morse(MfArdfMessage message) {
    return message < MfArdfMessageCount ? identifier_morse[message] : "";
}

uint16_t mf_ardf_wpm_to_dit_ms(uint8_t wpm) {
    if(wpm < 8U) wpm = 8U;
    if(wpm > 30U) wpm = 30U;
    return (uint16_t)((1200U + (wpm / 2U)) / wpm);
}

bool mf_ardf_sequence_build(MfArdfSequence* sequence, const char* text, uint8_t wpm) {
    char normalized[MF_ARDF_CUSTOM_CAPACITY + 1U];
    size_t i;
    uint16_t dit_ms;
    if(sequence == NULL || mf_ardf_normalize_custom(normalized, sizeof(normalized), text) == 0U)
        return false;
    memset(sequence, 0, sizeof(*sequence));
    dit_ms = mf_ardf_wpm_to_dit_ms(wpm);
    for(i = 0U; normalized[i] != '\0'; i++) {
        uint8_t code;
        uint8_t mark_index = 0U;
        if(normalized[i] == ' ') continue;
        code = cw(normalized[i]);
        if(code == CW_INVALID) return false;
        while(code > 1U) {
            bool dah = (code & 1U) != 0U;
            if(sequence->count >= MF_ARDF_SEQUENCE_CAPACITY) return false;
            sequence->steps[sequence->count++] =
                (MfArdfSequenceStep){.units = dah ? 3U : 1U, .mark = true};
            sequence->duration_ms += (uint32_t)(dah ? 3U : 1U) * dit_ms;
            code >>= 1U;
            mark_index++;
            if(code > 1U) {
                if(sequence->count >= MF_ARDF_SEQUENCE_CAPACITY) return false;
                sequence->steps[sequence->count++] = (MfArdfSequenceStep){.units = 1U};
                sequence->duration_ms += dit_ms;
            }
        }
        if(normalized[i + 1U] != '\0') {
            uint8_t gap = normalized[i + 1U] == ' ' ? 7U : 3U;
            if(sequence->count >= MF_ARDF_SEQUENCE_CAPACITY) return false;
            sequence->steps[sequence->count++] = (MfArdfSequenceStep){.units = gap};
            sequence->duration_ms += (uint32_t)gap * dit_ms;
            if(normalized[i + 1U] == ' ') i++;
        }
        (void)mark_index;
    }
    return sequence->count != 0U && sequence->steps[sequence->count - 1U].mark;
}

bool mf_ardf_time_reached(uint32_t now_ms, uint32_t deadline_ms) {
    return (int32_t)(now_ms - deadline_ms) >= 0;
}

uint32_t mf_ardf_countdown_seconds(uint32_t now_ms, uint32_t deadline_ms) {
    int32_t remaining = (int32_t)(deadline_ms - now_ms);
    return remaining <= 0 ? 0U : ((uint32_t)remaining + 999U) / 1000U;
}

uint32_t mf_ardf_cycle_seconds(MfArdfMode mode) {
    return mode == MfArdfModeSprint ? 60U : 300U;
}

uint32_t mf_ardf_target_phase(MfArdfMode mode, MfArdfMessage message) {
    if(message >= MfArdfMessageS) return 0U;
    return (uint32_t)message * (mode == MfArdfModeSprint ? 12U : 60U);
}

uint32_t mf_ardf_next_cycle_wall_s(uint32_t wall_s, MfArdfMode mode, MfArdfMessage message) {
    uint32_t cycle = mf_ardf_cycle_seconds(mode);
    uint32_t target = mf_ardf_target_phase(mode, message);
    uint32_t phase = wall_s % cycle;
    uint32_t forward = (target + cycle - phase) % cycle;
    if(forward == 0U) forward = cycle;
    return wall_s + forward;
}

uint32_t mf_ardf_custom_next_wall_s(
    uint32_t anchor_wall_s,
    uint32_t interval_s,
    uint32_t not_before_wall_s) {
    uint32_t delta;
    uint32_t steps;
    if(interval_s == 0U || not_before_wall_s <= anchor_wall_s) return anchor_wall_s;
    delta = not_before_wall_s - anchor_wall_s;
    steps = delta / interval_s + (delta % interval_s != 0U);
    return anchor_wall_s + steps * interval_s;
}

uint8_t mf_ardf_progress_width(uint32_t now_ms, uint32_t start_ms, uint32_t deadline_ms) {
    uint32_t duration = deadline_ms - start_ms;
    uint32_t elapsed = now_ms - start_ms;
    if(duration == 0U || mf_ardf_time_reached(now_ms, deadline_ms)) return 120U;
    if((int32_t)(now_ms - start_ms) < 0) return 0U;
    if(elapsed >= duration) return 120U;
    return (uint8_t)((elapsed * 120U) / duration);
}

static uint32_t mf_ardf_clock_seconds(MfArdfClockTime time) {
    return (uint32_t)time.hour * 3600U + (uint32_t)time.minute * 60U + time.second;
}

static uint32_t mf_ardf_wall_forward(uint32_t from_s, uint32_t to_s) {
    return to_s >= from_s ? to_s - from_s : 86400U - from_s + to_s;
}

static bool mf_ardf_numbered(const MfArdfState* state) {
    return state->snapshot.settings.mode != MfArdfModeCustom &&
           state->snapshot.settings.message < MfArdfMessageS;
}

static uint32_t mf_ardf_slot_ms(const MfArdfState* state) {
    return state->snapshot.settings.mode == MfArdfModeSprint ? 12000U : 60000U;
}

static uint32_t mf_ardf_repeat_gap_ms(const MfArdfState* state) {
    return (uint32_t)mf_ardf_wpm_to_dit_ms(state->snapshot.settings.wpm) * 7U;
}

static bool mf_ardf_fits_by(uint32_t start_ms, uint32_t duration_ms, uint32_t limit_ms) {
    return (int32_t)(limit_ms - (start_ms + duration_ms)) >= 0;
}

static void mf_ardf_outputs_off(MfArdfState* state) {
    if(state == NULL) return;
    state->hardware.set_mark(state->hardware.context, false);
    state->hardware.set_p15(state->hardware.context, false);
    state->hardware.set_p16(state->hardware.context, false);
    state->hardware.set_led(state->hardware.context, false);
    state->hardware.stop(state->hardware.context);
    state->prepared = false;
    state->snapshot.transmitting = false;
    state->snapshot.mark = false;
    state->snapshot.ptt = false;
    state->snapshot.playback_active = false;
    state->snapshot.playback_mark = false;
    state->sequence_next_ms = 0U;
    state->sequence_index = 0U;
    state->repeat_gap = false;
    state->preamble = false;
    if(state->backlight_requested) {
        state->backlight_requested = false;
        state->backlight_off_pending = true;
    }
}

static bool mf_ardf_prepare(MfArdfState* state) {
    if(state->prepared) return true;
    if(!state->hardware.prepare(
           state->hardware.context,
           state->frequency_hz,
           (MfArdfModulation)state->snapshot.settings.modulation))
        return false;
    state->prepared = true;
    state->hardware.set_p16(state->hardware.context, true);
    state->snapshot.ptt = true;
    return true;
}

static bool mf_ardf_set_mark(MfArdfState* state, bool mark) {
    if(!state->hardware.set_mark(state->hardware.context, mark)) return false;
    state->hardware.set_p15(state->hardware.context, mark);
    state->hardware.set_led(state->hardware.context, mark);
    state->snapshot.mark = mark;
    state->snapshot.playback_mark = mark && state->snapshot.settings.audio_output;
    return true;
}

static void mf_ardf_backlight_refresh(MfArdfState* state) {
    if(!state->snapshot.settings.light_assistance) return;
    state->wake_pending = true;
    state->backlight_requested = true;
}

static bool mf_ardf_build_sequence(MfArdfState* state, uint32_t start_ms, uint32_t end_limit_ms) {
    const char* text =
        state->snapshot.settings.mode == MfArdfModeCustom ?
            state->snapshot.settings.custom :
            mf_ardf_identifier_text((MfArdfMessage)state->snapshot.settings.message);
    return mf_ardf_sequence_build(&state->sequence, text, state->snapshot.settings.wpm) &&
           (end_limit_ms == 0U ||
            mf_ardf_fits_by(start_ms, state->sequence.duration_ms, end_limit_ms));
}

static bool mf_ardf_begin_sequence(MfArdfState* state, uint32_t start_ms) {
    uint16_t dit_ms = mf_ardf_wpm_to_dit_ms(state->snapshot.settings.wpm);
    if(!state->prepared || state->sequence.count == 0U) return false;
    state->snapshot.transmitting = true;
    state->snapshot.playback_active = state->snapshot.settings.audio_output;
    state->sequence_index = 0U;
    if(!mf_ardf_set_mark(state, state->sequence.steps[0].mark)) return false;
    state->sequence_next_ms = start_ms + (uint32_t)state->sequence.steps[0].units * dit_ms;
    state->repeat_gap = false;
    mf_ardf_backlight_refresh(state);
    return true;
}

static bool mf_ardf_start_sequence(MfArdfState* state, uint32_t start_ms, uint32_t end_limit_ms) {
    return mf_ardf_build_sequence(state, start_ms, end_limit_ms) && mf_ardf_prepare(state) &&
           mf_ardf_begin_sequence(state, start_ms);
}

static bool
    mf_ardf_arm_preamble(MfArdfState* state, uint32_t first_mark_ms, uint32_t end_limit_ms) {
    if(!mf_ardf_build_sequence(state, first_mark_ms, end_limit_ms) || !mf_ardf_prepare(state))
        return false;
    state->preamble = true;
    state->sequence_next_ms = first_mark_ms;
    state->snapshot.transmitting = false;
    state->snapshot.mark = false;
    state->snapshot.playback_active = false;
    state->snapshot.playback_mark = false;
    return true;
}

static bool mf_ardf_begin_immediate(MfArdfState* state, uint32_t now_ms, uint32_t end_limit_ms) {
    if(state->snapshot.settings.modulation != MfArdfModulationCwfm)
        return mf_ardf_start_sequence(state, now_ms, end_limit_ms);
    return mf_ardf_arm_preamble(state, now_ms + MF_ARDF_CWFM_ACQUIRE_MS, end_limit_ms);
}

static bool mf_ardf_start_numbered_slot(MfArdfState* state, uint32_t deadline_ms) {
    uint32_t cycle_s = mf_ardf_cycle_seconds((MfArdfMode)state->snapshot.settings.mode);
    state->slot_end_ms = deadline_ms + mf_ardf_slot_ms(state);
    state->cycle_deadline_ms = deadline_ms + cycle_s * 1000U;
    state->snapshot.segment_start_ms = deadline_ms;
    state->snapshot.next_deadline_ms = state->slot_end_ms;
    state->slot_active = true;
    state->deadline_wall_s += cycle_s;
    if(!mf_ardf_start_sequence(state, deadline_ms, state->slot_end_ms)) {
        state->snapshot.next_deadline_ms = state->cycle_deadline_ms;
        return false;
    }
    return true;
}

static bool mf_ardf_arm_cwfm_numbered_slot(
    MfArdfState* state,
    uint32_t deadline_ms,
    uint32_t first_mark_ms) {
    uint32_t cycle_s = mf_ardf_cycle_seconds((MfArdfMode)state->snapshot.settings.mode);
    state->slot_end_ms = deadline_ms + mf_ardf_slot_ms(state);
    state->cycle_deadline_ms = deadline_ms + cycle_s * 1000U;
    state->snapshot.segment_start_ms = deadline_ms;
    state->snapshot.next_deadline_ms = state->slot_end_ms;
    state->slot_active = true;
    state->deadline_wall_s += cycle_s;
    if(!mf_ardf_arm_preamble(state, first_mark_ms, state->slot_end_ms)) {
        state->snapshot.next_deadline_ms = state->cycle_deadline_ms;
        return false;
    }
    return true;
}

static bool mf_ardf_restart_sequence(MfArdfState* state, uint32_t start_ms) {
    uint16_t dit_ms = mf_ardf_wpm_to_dit_ms(state->snapshot.settings.wpm);
    state->sequence_index = 0U;
    state->repeat_gap = false;
    if(!mf_ardf_set_mark(state, state->sequence.steps[0].mark)) return false;
    state->sequence_next_ms = start_ms + (uint32_t)state->sequence.steps[0].units * dit_ms;
    mf_ardf_backlight_refresh(state);
    return true;
}

static uint32_t mf_ardf_deadline_from_edge(const MfArdfState* state, uint32_t target_wall_s) {
    uint32_t target_day = target_wall_s % 86400U;
    return state->accepted_edge_ms +
           mf_ardf_wall_forward(state->accepted_wall_s, target_day) * 1000U;
}

static bool mf_ardf_schedule_numbered_on_enter(MfArdfState* state, uint32_t now_ms) {
    uint32_t wall_s = mf_ardf_clock_seconds(state->live_time);
    uint32_t cycle_s = mf_ardf_cycle_seconds((MfArdfMode)state->snapshot.settings.mode);
    uint32_t slot_ms = mf_ardf_slot_ms(state);
    uint32_t target_s = mf_ardf_target_phase(
        (MfArdfMode)state->snapshot.settings.mode,
        (MfArdfMessage)state->snapshot.settings.message);
    uint32_t phase_s = wall_s % cycle_s;
    uint32_t offset_s = (phase_s + cycle_s - target_s) % cycle_s;
    if(offset_s < slot_ms / 1000U) {
        uint32_t second_start =
            state->calibration_valid ? mf_ardf_deadline_from_edge(state, wall_s) : now_ms;
        uint32_t slot_start = second_start - offset_s * 1000U;
        uint32_t first_mark = now_ms;
        if(state->snapshot.settings.modulation == MfArdfModulationCwfm)
            first_mark += MF_ARDF_CWFM_ACQUIRE_MS;
        state->slot_end_ms = slot_start + slot_ms;
        state->cycle_deadline_ms = slot_start + cycle_s * 1000U;
        state->snapshot.segment_start_ms = slot_start;
        state->snapshot.next_deadline_ms = state->slot_end_ms;
        state->slot_active = true;
        state->join_active = mf_ardf_build_sequence(state, first_mark, state->slot_end_ms);
        state->snapshot.run_pending = state->join_active;
        return state->join_active;
    }
    state->slot_active = false;
    state->join_active = false;
    state->snapshot.run_pending = false;
    if(state->calibration_valid) {
        state->deadline_wall_s = mf_ardf_next_cycle_wall_s(
            wall_s,
            (MfArdfMode)state->snapshot.settings.mode,
            (MfArdfMessage)state->snapshot.settings.message);
        state->cycle_deadline_ms = mf_ardf_deadline_from_edge(state, state->deadline_wall_s);
        state->snapshot.next_deadline_ms = state->cycle_deadline_ms;
        state->snapshot.segment_start_ms = state->cycle_deadline_ms - cycle_s * 1000U;
        state->sampling = false;
    } else {
        state->snapshot.next_deadline_ms = 0U;
        state->sampling = true;
    }
    return false;
}

static void mf_ardf_arm_from_calibration(MfArdfState* state, uint32_t now_ms) {
    uint32_t wall_s;
    uint32_t deadline;
    if(!state->calibration_valid || state->snapshot.view != MfArdfViewRun ||
       state->snapshot.next_deadline_ms != 0U || state->snapshot.settings.mode == MfArdfModeCustom)
        return;
    wall_s = state->accepted_wall_s;
    state->deadline_wall_s = mf_ardf_next_cycle_wall_s(
        wall_s,
        (MfArdfMode)state->snapshot.settings.mode,
        (MfArdfMessage)state->snapshot.settings.message);
    deadline = mf_ardf_deadline_from_edge(state, state->deadline_wall_s);
    state->snapshot.next_deadline_ms = deadline;
    state->cycle_deadline_ms = deadline;
    state->snapshot.segment_start_ms =
        deadline - mf_ardf_cycle_seconds((MfArdfMode)state->snapshot.settings.mode) * 1000U;
    (void)now_ms;
}

static void mf_ardf_custom_anchor(MfArdfState* state) {
    uint32_t anchor_wall;
    uint32_t not_before_ms;
    uint32_t not_before_wall;
    uint32_t target_wall;
    uint32_t interval_s;
    if(!state->calibration_valid || state->snapshot.settings.mode != MfArdfModeCustom ||
       state->snapshot.view != MfArdfViewRun || !state->provisional_custom)
        return;
    anchor_wall = state->custom_anchor_wall_s;
    not_before_ms = state->run_started_ms + state->sequence.duration_ms;
    not_before_wall =
        mf_ardf_time_reached(state->accepted_edge_ms, not_before_ms) ?
            state->accepted_wall_s :
            state->accepted_wall_s + ((not_before_ms - state->accepted_edge_ms + 999U) / 1000U);
    interval_s = mf_ardf_interval_seconds(state->snapshot.settings.interval_index);
    target_wall = mf_ardf_custom_next_wall_s(anchor_wall, interval_s, not_before_wall);
    state->deadline_wall_s = target_wall;
    state->provisional_deadline_ms = mf_ardf_deadline_from_edge(state, target_wall);
    if(!state->preamble) state->snapshot.next_deadline_ms = state->provisional_deadline_ms;
    state->provisional_custom = false;
    state->sampling = false;
}

bool mf_ardf_core_enter(
    MfArdfState* state,
    const MfArdfEnterArgs* args,
    const MfArdfHardwareOps* hardware,
    MorseFlipperMappedFalResult* initial) {
    if(initial != NULL) *initial = (MorseFlipperMappedFalResult){0};
    if(state == NULL || args == NULL || args->struct_size != sizeof(*args) ||
       !mf_ardf_hardware_ops_valid(hardware))
        return false;
    memset(state, 0, sizeof(*state));
    state->hardware = *hardware;
    state->frequency_hz = args->frequency_hz;
    state->snapshot.struct_size = sizeof(state->snapshot);
    state->snapshot.view = MfArdfViewSettings;
    mf_ardf_settings_load(&state->snapshot.settings);
    state->entered = true;
    if(initial != NULL) *initial = mf_ardf_result(state, true);
    return true;
}

void mf_ardf_core_leave(MfArdfState* state) {
    if(state == NULL) return;
    mf_ardf_outputs_off(state);
    state->entered = false;
    state->snapshot.running = false;
    state->snapshot.gpio_owned = false;
    state->snapshot.run_pending = false;
}

MorseFlipperMappedFalResult
    mf_ardf_core_set_view(MfArdfState* state, MfArdfView view, uint32_t now_ms) {
    if(state == NULL || !state->entered || view > MfArdfViewRun)
        return (MorseFlipperMappedFalResult){0};
    if(state->snapshot.view == MfArdfViewRun && view != MfArdfViewRun) mf_ardf_outputs_off(state);
    state->snapshot.view = view;
    if(view == MfArdfViewClock) {
        state->snapshot.clock_state = MfArdfClockConfirm;
        state->snapshot.clock_field = MfArdfClockHours;
        state->sampling = true;
        state->previous_sample_valid = false;
    } else if(view == MfArdfViewRun) {
        state->snapshot.running = true;
        state->snapshot.gpio_owned = true;
        state->run_started_ms = now_ms;
    }
    return mf_ardf_result(state, true);
}

static void mf_ardf_request_run(MfArdfState* state, uint32_t now_ms) {
    state->snapshot.host_action = MfArdfHostActionNone;
    if(!state->hardware.frequency_allowed(
           state->hardware.context,
           state->frequency_hz,
           (MfArdfModulation)state->snapshot.settings.modulation)) {
        state->snapshot.error = MfArdfErrorFrequency;
        state->snapshot.host_action = MfArdfHostActionShowError;
        return;
    }
    state->continuous = false;
    state->slot_end_ms = 0U;
    state->cycle_deadline_ms = 0U;
    state->repeat_gap = false;
    state->preamble = false;
    state->slot_active = false;
    state->join_active = false;
    state->snapshot.view = MfArdfViewRun;
    state->snapshot.running = true;
    state->snapshot.gpio_owned = false;
    state->run_started_ms = now_ms;
    if(state->snapshot.settings.mode == MfArdfModeCustom ||
       state->snapshot.settings.message >= MfArdfMessageS) {
        state->snapshot.run_pending = true;
    } else {
        (void)mf_ardf_schedule_numbered_on_enter(state, now_ms);
    }
}

MorseFlipperMappedFalResult mf_ardf_core_activate_run(MfArdfState* state, uint32_t now_ms) {
    uint32_t rtc_wall;
    if(state == NULL || !state->entered || !state->snapshot.run_pending)
        return mf_ardf_result(state, false);
    state->snapshot.run_pending = false;
    state->snapshot.gpio_owned = true;
    state->run_started_ms = now_ms;
    rtc_wall = mf_ardf_clock_seconds(state->live_time);
    if(state->snapshot.settings.mode == MfArdfModeCustom) {
        uint32_t duration_s;
        uint32_t anchor_wall;
        uint32_t target_wall;
        uint32_t first_mark = now_ms;
        if(state->snapshot.settings.modulation == MfArdfModulationCwfm)
            first_mark += MF_ARDF_CWFM_ACQUIRE_MS;
        if(!mf_ardf_begin_immediate(state, now_ms, 0U)) {
            state->snapshot.error = MfArdfErrorHardware;
            state->snapshot.host_action = MfArdfHostActionShowError;
            mf_ardf_outputs_off(state);
            return mf_ardf_result(state, true);
        }
        state->run_started_ms = first_mark;
        duration_s = (MF_ARDF_CWFM_ACQUIRE_MS + state->sequence.duration_ms + 999U) / 1000U;
        if(state->snapshot.settings.modulation != MfArdfModulationCwfm)
            duration_s = (state->sequence.duration_ms + 999U) / 1000U;
        anchor_wall = rtc_wall + (60U - rtc_wall % 60U);
        state->custom_anchor_wall_s = anchor_wall;
        target_wall = mf_ardf_custom_next_wall_s(
            anchor_wall,
            mf_ardf_interval_seconds(state->snapshot.settings.interval_index),
            rtc_wall + duration_s);
        state->provisional_deadline_ms = now_ms + (target_wall - rtc_wall) * 1000U;
        state->snapshot.next_deadline_ms = state->preamble ? first_mark :
                                                             state->provisional_deadline_ms;
        state->snapshot.segment_start_ms = first_mark;
        state->provisional_custom = true;
        state->sampling = !state->calibration_valid;
        if(state->calibration_valid) mf_ardf_custom_anchor(state);
        return mf_ardf_result(state, true);
    }
    state->continuous = state->snapshot.settings.message >= MfArdfMessageS;
    if(state->continuous) {
        uint32_t first_mark = now_ms;
        if(state->snapshot.settings.modulation == MfArdfModulationCwfm)
            first_mark += MF_ARDF_CWFM_ACQUIRE_MS;
        state->snapshot.segment_start_ms = first_mark;
        if(!mf_ardf_begin_immediate(state, now_ms, 0U)) {
            state->snapshot.error = MfArdfErrorHardware;
            state->snapshot.host_action = MfArdfHostActionShowError;
            mf_ardf_outputs_off(state);
        } else if(state->preamble)
            state->snapshot.next_deadline_ms = first_mark;
        state->sampling = false;
        return mf_ardf_result(state, true);
    }
    if(state->join_active) {
        if(!mf_ardf_begin_immediate(state, now_ms, state->slot_end_ms)) {
            state->snapshot.error = MfArdfErrorHardware;
            state->snapshot.host_action = MfArdfHostActionShowError;
            mf_ardf_outputs_off(state);
        }
        state->join_active = false;
        state->sampling = !state->calibration_valid;
        return mf_ardf_result(state, true);
    }
    if(state->snapshot.next_deadline_ms != 0U) {
        uint32_t deadline = state->snapshot.next_deadline_ms;
        uint32_t first_mark = now_ms + MF_ARDF_CWFM_ACQUIRE_MS;
        if(state->snapshot.settings.modulation == MfArdfModulationCwfm &&
           mf_ardf_time_reached(first_mark, deadline) &&
           !mf_ardf_arm_cwfm_numbered_slot(state, deadline, first_mark)) {
            state->snapshot.error = MfArdfErrorHardware;
            state->snapshot.host_action = MfArdfHostActionShowError;
            mf_ardf_outputs_off(state);
        } else if(
            state->snapshot.settings.modulation != MfArdfModulationCwfm &&
            !mf_ardf_prepare(state)) {
            state->snapshot.error = MfArdfErrorHardware;
            state->snapshot.host_action = MfArdfHostActionShowError;
            mf_ardf_outputs_off(state);
        }
        return mf_ardf_result(state, true);
    }
    state->snapshot.next_deadline_ms = 0U;
    if(state->calibration_valid) {
        mf_ardf_arm_from_calibration(state, now_ms);
    } else {
        state->sampling = true;
    }
    return mf_ardf_result(state, true);
}

static void mf_ardf_clock_adjust(MfArdfClockTime* time, MfArdfClockField field, int delta) {
    uint8_t* value = field == MfArdfClockHours   ? &time->hour :
                     field == MfArdfClockMinutes ? &time->minute :
                                                   &time->second;
    uint8_t limit = field == MfArdfClockHours ? 24U : 60U;
    *value = (uint8_t)((*value + limit + delta) % limit);
}

MorseFlipperMappedFalResult
    mf_ardf_core_input(MfArdfState* state, const InputEvent* event, uint32_t now_ms) {
    if(state == NULL || event == NULL || !state->entered || event->type != InputTypeShort)
        return mf_ardf_result(state, false);
    if(state->snapshot.view == MfArdfViewClock) {
        MfArdfClockState clock_state = (MfArdfClockState)state->snapshot.clock_state;
        if(event->key == InputKeyBack) {
            if(clock_state == MfArdfClockEdit) {
                state->draft_time = state->live_time;
                state->snapshot.clock_state = MfArdfClockSelect;
            } else if(clock_state == MfArdfClockSelect) {
                state->snapshot.clock_state = MfArdfClockConfirm;
            } else {
                state->snapshot.settings.selected_row = 0U;
                state->draft_time = state->live_time;
                (void)mf_ardf_settings_save(&state->snapshot.settings);
                state->snapshot.view = MfArdfViewSettings;
            }
            return mf_ardf_result(state, true);
        }
        if(clock_state == MfArdfClockConfirm) {
            if(event->key == InputKeyOk)
                mf_ardf_request_run(state, now_ms);
            else if(event->key == InputKeyRight) {
                state->snapshot.clock_field = MfArdfClockHours;
                state->snapshot.clock_state = MfArdfClockSelect;
            } else if(event->key == InputKeyLeft) {
                state->snapshot.clock_field = MfArdfClockSeconds;
                state->snapshot.clock_state = MfArdfClockSelect;
            } else
                return mf_ardf_result(state, false);
        } else if(clock_state == MfArdfClockSelect) {
            if(event->key == InputKeyLeft) {
                if(state->snapshot.clock_field == MfArdfClockHours)
                    state->snapshot.clock_state = MfArdfClockConfirm;
                else
                    state->snapshot.clock_field--;
            } else if(event->key == InputKeyRight) {
                if(state->snapshot.clock_field == MfArdfClockSeconds)
                    state->snapshot.clock_state = MfArdfClockConfirm;
                else
                    state->snapshot.clock_field++;
            } else if(event->key == InputKeyOk) {
                state->draft_time = state->live_time;
                state->snapshot.clock_state = MfArdfClockEdit;
            } else
                return mf_ardf_result(state, false);
        } else {
            if(event->key == InputKeyLeft && state->snapshot.clock_field > MfArdfClockHours)
                state->snapshot.clock_field--;
            else if(event->key == InputKeyRight && state->snapshot.clock_field < MfArdfClockSeconds)
                state->snapshot.clock_field++;
            else if(event->key == InputKeyUp)
                mf_ardf_clock_adjust(&state->draft_time, state->snapshot.clock_field, 1);
            else if(event->key == InputKeyDown)
                mf_ardf_clock_adjust(&state->draft_time, state->snapshot.clock_field, -1);
            else if(event->key == InputKeyOk) {
                if(state->hardware.set_clock(state->hardware.context, state->draft_time)) {
                    state->live_time = state->draft_time;
                    state->snapshot.clock_state = MfArdfClockConfirm;
                    state->calibration_valid = false;
                    state->previous_sample_valid = false;
                    state->sampling = true;
                }
            } else
                return mf_ardf_result(state, false);
        }
        return mf_ardf_result(state, true);
    }
    if(state->snapshot.view == MfArdfViewRun && event->key == InputKeyBack) {
        mf_ardf_outputs_off(state);
        state->modal = true;
        state->snapshot.host_action = MfArdfHostActionShowStopConfirmation;
        return mf_ardf_result(state, true);
    }
    if(state->snapshot.view == MfArdfViewSettings && event->key == InputKeyBack) {
        state->snapshot.host_action = MfArdfHostActionCloseToRadio;
        return mf_ardf_result(state, true);
    }
    return mf_ardf_result(state, false);
}

static void mf_ardf_finish_sequence(MfArdfState* state, uint32_t planned_end_ms, uint32_t now_ms) {
    uint32_t interval_s;
    uint32_t repeat_start = planned_end_ms + mf_ardf_repeat_gap_ms(state);
    if(state->continuous ||
       (mf_ardf_numbered(state) &&
        mf_ardf_fits_by(repeat_start, state->sequence.duration_ms, state->slot_end_ms))) {
        if(!mf_ardf_set_mark(state, false)) {
            state->snapshot.error = MfArdfErrorHardware;
            state->snapshot.host_action = MfArdfHostActionShowError;
            mf_ardf_outputs_off(state);
            return;
        }
        state->repeat_gap = true;
        state->sequence_next_ms = repeat_start;
        return;
    }
    mf_ardf_outputs_off(state);
    if(state->snapshot.settings.mode != MfArdfModeCustom || state->snapshot.next_deadline_ms == 0U)
        return;
    interval_s = mf_ardf_interval_seconds(state->snapshot.settings.interval_index);
    while((int32_t)(now_ms - state->snapshot.next_deadline_ms) > 0) {
        state->snapshot.next_deadline_ms += interval_s * 1000U;
        state->deadline_wall_s += interval_s;
    }
}

static void mf_ardf_skip_late_deadlines(MfArdfState* state, uint32_t now_ms) {
    uint32_t step_ms = (state->snapshot.settings.mode == MfArdfModeCustom ?
                            mf_ardf_interval_seconds(state->snapshot.settings.interval_index) :
                            mf_ardf_cycle_seconds((MfArdfMode)state->snapshot.settings.mode)) *
                       1000U;
    int32_t late_ms = (int32_t)(now_ms - state->snapshot.next_deadline_ms);
    if(state->snapshot.next_deadline_ms == 0U || late_ms <= (int32_t)MF_ARDF_MAX_START_LATE_MS)
        return;
    uint32_t steps = ((uint32_t)late_ms - MF_ARDF_MAX_START_LATE_MS + step_ms - 1U) / step_ms;
    state->snapshot.segment_start_ms = state->snapshot.next_deadline_ms + (steps - 1U) * step_ms;
    state->snapshot.next_deadline_ms += steps * step_ms;
    state->deadline_wall_s += steps * (step_ms / 1000U);
    if(state->snapshot.settings.mode != MfArdfModeCustom)
        state->cycle_deadline_ms = state->snapshot.next_deadline_ms;
    if(state->prepared) mf_ardf_outputs_off(state);
}

static void mf_ardf_schedule_custom_after(
    MfArdfState* state,
    uint32_t deadline_ms,
    uint32_t completion_ms) {
    uint32_t interval_s = mf_ardf_interval_seconds(state->snapshot.settings.interval_index);
    state->snapshot.segment_start_ms = deadline_ms;
    state->snapshot.next_deadline_ms = deadline_ms + interval_s * 1000U;
    state->deadline_wall_s += interval_s;
    while((int32_t)(state->snapshot.next_deadline_ms - completion_ms) < 0) {
        state->snapshot.next_deadline_ms += interval_s * 1000U;
        state->deadline_wall_s += interval_s;
    }
}

MorseFlipperMappedFalResult mf_ardf_core_tick(MfArdfState* state, uint32_t now_ms) {
    uint16_t dit_ms;
    if(state == NULL || !state->entered) return (MorseFlipperMappedFalResult){0};
    if(state->snapshot.view != MfArdfViewRun || state->modal)
        return mf_ardf_tick_result(state, false);
    if(state->next_redraw_ms == 0U || mf_ardf_time_reached(now_ms, state->next_redraw_ms)) {
        state->next_redraw_ms = now_ms + MF_ARDF_REDRAW_MS;
        state->redraw_pending = true;
    }
    dit_ms = mf_ardf_wpm_to_dit_ms(state->snapshot.settings.wpm);
    if(state->preamble && mf_ardf_time_reached(now_ms, state->sequence_next_ms)) {
        uint32_t first_mark = state->sequence_next_ms;
        state->preamble = false;
        if(!mf_ardf_begin_sequence(state, first_mark)) {
            state->snapshot.error = MfArdfErrorHardware;
            state->snapshot.host_action = MfArdfHostActionShowError;
            mf_ardf_outputs_off(state);
        } else if(state->snapshot.settings.mode == MfArdfModeCustom && state->provisional_custom) {
            state->snapshot.next_deadline_ms = state->provisional_deadline_ms;
        } else if(state->continuous) {
            state->snapshot.next_deadline_ms = 0U;
        }
    }
    while(state->snapshot.transmitting && mf_ardf_time_reached(now_ms, state->sequence_next_ms)) {
        if(state->repeat_gap) {
            uint32_t repeat_start = state->sequence_next_ms;
            if(!mf_ardf_restart_sequence(state, repeat_start)) {
                state->snapshot.error = MfArdfErrorHardware;
                state->snapshot.host_action = MfArdfHostActionShowError;
                mf_ardf_outputs_off(state);
                break;
            }
            continue;
        }
        state->sequence_index++;
        if(state->sequence_index >= state->sequence.count) {
            uint32_t planned_end = state->sequence_next_ms;
            mf_ardf_finish_sequence(state, planned_end, now_ms);
            if(!state->snapshot.transmitting) break;
            continue;
        }
        if(!mf_ardf_set_mark(state, state->sequence.steps[state->sequence_index].mark)) {
            state->snapshot.error = MfArdfErrorHardware;
            state->snapshot.host_action = MfArdfHostActionShowError;
            mf_ardf_outputs_off(state);
            break;
        }
        state->sequence_next_ms +=
            (uint32_t)state->sequence.steps[state->sequence_index].units * dit_ms;
    }
    if(mf_ardf_numbered(state) && state->slot_active) {
        if(!state->snapshot.transmitting && !state->preamble &&
           mf_ardf_time_reached(now_ms, state->slot_end_ms)) {
            state->slot_active = false;
            state->snapshot.next_deadline_ms = state->cycle_deadline_ms;
            state->snapshot.segment_start_ms = state->slot_end_ms;
        }
        return mf_ardf_tick_result(state, true);
    }
    if(!state->snapshot.transmitting && !state->preamble &&
       state->snapshot.next_deadline_ms != 0U) {
        mf_ardf_skip_late_deadlines(state, now_ms);
        uint32_t lead = state->snapshot.next_deadline_ms - MF_ARDF_PTT_LEAD_MS;
        if(!state->prepared && mf_ardf_time_reached(now_ms, lead) &&
           !mf_ardf_time_reached(now_ms, state->snapshot.next_deadline_ms)) {
            if(!state->snapshot.gpio_owned) {
                state->snapshot.run_pending = true;
                return mf_ardf_result(state, true);
            } else if(state->snapshot.settings.modulation == MfArdfModulationCwfm) {
                uint32_t deadline = state->snapshot.next_deadline_ms;
                uint32_t first_mark = now_ms + MF_ARDF_CWFM_ACQUIRE_MS;
                bool armed = mf_ardf_numbered(state) ?
                                 mf_ardf_arm_cwfm_numbered_slot(state, deadline, first_mark) :
                                 mf_ardf_arm_preamble(state, first_mark, 0U);
                if(armed && state->snapshot.settings.mode == MfArdfModeCustom)
                    mf_ardf_schedule_custom_after(
                        state, deadline, first_mark + state->sequence.duration_ms);
                if(!armed) {
                    state->snapshot.error = MfArdfErrorHardware;
                    state->snapshot.host_action = MfArdfHostActionShowError;
                    mf_ardf_outputs_off(state);
                }
            } else if(!mf_ardf_prepare(state)) {
                state->snapshot.error = MfArdfErrorHardware;
                state->snapshot.host_action = MfArdfHostActionShowError;
                mf_ardf_outputs_off(state);
            }
        }
        if(mf_ardf_time_reached(now_ms, state->snapshot.next_deadline_ms)) {
            uint32_t deadline = state->snapshot.next_deadline_ms;
            if(!state->prepared || !state->snapshot.gpio_owned) {
                mf_ardf_skip_late_deadlines(state, now_ms + MF_ARDF_MAX_START_LATE_MS + 1U);
                return mf_ardf_tick_result(state, true);
            } else if(
                state->snapshot.settings.mode != MfArdfModeCustom &&
                !mf_ardf_start_numbered_slot(state, deadline)) {
                state->snapshot.error = MfArdfErrorHardware;
                state->snapshot.host_action = MfArdfHostActionShowError;
                mf_ardf_outputs_off(state);
            } else if(
                state->snapshot.settings.mode == MfArdfModeCustom &&
                !mf_ardf_start_sequence(state, deadline, 0U)) {
                state->snapshot.error = MfArdfErrorHardware;
                state->snapshot.host_action = MfArdfHostActionShowError;
                mf_ardf_outputs_off(state);
            } else if(state->snapshot.settings.mode == MfArdfModeCustom) {
                mf_ardf_schedule_custom_after(
                    state, deadline, deadline + state->sequence.duration_ms);
            }
        }
    }
    return mf_ardf_tick_result(state, true);
}

MorseFlipperMappedFalResult mf_ardf_core_text_input_result(
    MfArdfState* state,
    const char* text,
    bool accepted,
    uint32_t now_ms) {
    char normalized[MF_ARDF_CUSTOM_CAPACITY + 1U];
    (void)now_ms;
    if(state == NULL || state->snapshot.host_action != MfArdfHostActionOpenTextInput)
        return mf_ardf_result(state, false);
    state->snapshot.host_action = MfArdfHostActionNone;
    state->snapshot.settings.selected_row = MF_ARDF_CUSTOM_ROW;
    if(accepted && mf_ardf_normalize_custom(normalized, sizeof(normalized), text) != 0U) {
        memcpy(state->snapshot.settings.custom, normalized, sizeof(normalized));
    }
    mf_ardf_settings_save(&state->snapshot.settings);
    return mf_ardf_result(state, true);
}

MorseFlipperMappedFalResult mf_ardf_core_host_action_result(
    MfArdfState* state,
    MfArdfHostAction action,
    bool accepted,
    uint32_t now_ms) {
    if(state == NULL || state->snapshot.host_action != action) return mf_ardf_result(state, false);
    state->snapshot.host_action = MfArdfHostActionNone;
    if(action == MfArdfHostActionShowStopConfirmation) {
        state->modal = false;
        if(accepted) {
            state->snapshot.running = false;
            state->snapshot.gpio_owned = false;
            state->snapshot.run_pending = false;
            state->snapshot.view = MfArdfViewSettings;
        } else if(state->continuous) {
            state->snapshot.segment_start_ms = now_ms;
            if(!mf_ardf_begin_immediate(state, now_ms, 0U)) {
                state->snapshot.error = MfArdfErrorHardware;
                state->snapshot.host_action = MfArdfHostActionShowError;
                mf_ardf_outputs_off(state);
            } else if(state->preamble)
                state->snapshot.next_deadline_ms = now_ms + MF_ARDF_CWFM_ACQUIRE_MS;
        } else {
            uint32_t cycle_ms;
            if(mf_ardf_numbered(state))
                state->snapshot.next_deadline_ms = state->cycle_deadline_ms;
            cycle_ms =
                state->snapshot.settings.mode == MfArdfModeCustom ?
                    mf_ardf_interval_seconds(state->snapshot.settings.interval_index) * 1000U :
                    mf_ardf_cycle_seconds((MfArdfMode)state->snapshot.settings.mode) * 1000U;
            int32_t late_ms = (int32_t)(now_ms - state->snapshot.next_deadline_ms);
            if(state->snapshot.next_deadline_ms != 0U && late_ms >= 0) {
                uint32_t steps = (uint32_t)late_ms / cycle_ms + 1U;
                state->snapshot.segment_start_ms =
                    state->snapshot.next_deadline_ms + (steps - 1U) * cycle_ms;
                state->snapshot.next_deadline_ms += steps * cycle_ms;
                state->deadline_wall_s += steps * (cycle_ms / 1000U);
            }
            if(mf_ardf_numbered(state))
                state->cycle_deadline_ms = state->snapshot.next_deadline_ms;
        }
    } else if(action == MfArdfHostActionShowError) {
        state->snapshot.error = MfArdfErrorNone;
        state->snapshot.running = false;
        state->snapshot.gpio_owned = false;
        state->snapshot.run_pending = false;
        state->snapshot.view = MfArdfViewSettings;
    }
    return mf_ardf_result(state, true);
}

bool mf_ardf_core_snapshot(const MfArdfState* state, MfArdfSnapshot* snapshot) {
    if(state == NULL || snapshot == NULL || !state->entered) return false;
    *snapshot = state->snapshot;
    return true;
}

void mf_ardf_core_rtc_sample(
    MfArdfState* state,
    MfArdfClockTime time,
    uint32_t sample_before_ms,
    uint32_t sample_after_ms) {
    uint32_t wall_s;
    uint32_t width;
    bool edge_accepted = false;
    if(state == NULL || !state->entered) return;
    if(state->snapshot.view == MfArdfViewClock && state->snapshot.clock_state != MfArdfClockEdit &&
       (state->live_time.hour != time.hour || state->live_time.minute != time.minute ||
        state->live_time.second != time.second))
        state->redraw_pending = true;
    state->live_time = time;
    if(state->snapshot.clock_state != MfArdfClockEdit) state->draft_time = time;
    if(!state->sampling) return;
    wall_s = mf_ardf_clock_seconds(time);
    if(state->previous_sample_valid && wall_s != state->previous_wall_s) {
        width = sample_after_ms - state->previous_before_ms;
        if(width <= 10U) {
            state->accepted_edge_ms = state->previous_before_ms + width / 2U;
            state->accepted_wall_s = wall_s;
            state->calibration_valid = true;
            edge_accepted = true;
        }
        if(edge_accepted) {
            if(state->uncalibrated_schedule) {
                state->deadline_wall_s = mf_ardf_next_cycle_wall_s(
                    wall_s,
                    (MfArdfMode)state->snapshot.settings.mode,
                    (MfArdfMessage)state->snapshot.settings.message);
                state->snapshot.next_deadline_ms =
                    mf_ardf_deadline_from_edge(state, state->deadline_wall_s);
                state->cycle_deadline_ms = state->snapshot.next_deadline_ms;
                state->snapshot.segment_start_ms =
                    state->snapshot.next_deadline_ms -
                    mf_ardf_cycle_seconds((MfArdfMode)state->snapshot.settings.mode) * 1000U;
                state->uncalibrated_schedule = false;
            } else {
                mf_ardf_arm_from_calibration(state, sample_after_ms);
                mf_ardf_custom_anchor(state);
            }
            if(state->snapshot.view == MfArdfViewRun && !state->provisional_custom)
                state->sampling = false;
        }
    }
    state->previous_before_ms = sample_before_ms;
    state->previous_wall_s = wall_s;
    state->previous_sample_valid = true;
}
