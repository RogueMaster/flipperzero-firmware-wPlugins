#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "plugins/ardf/mf_ardf_core.h"
#include "plugins/ardf/mf_ardf_settings.h"

static unsigned checks;
#define CHECK(x)                                                                    \
    do {                                                                            \
        checks++;                                                                   \
        if(!(x)) {                                                                  \
            fprintf(stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__, #x); \
            return 1;                                                               \
        }                                                                           \
    } while(0)

#define PHYSICAL_SHORT(state, input_key, now, result)                       \
    do {                                                                    \
        InputEvent physical = {.key = (input_key), .type = InputTypePress}; \
        (result) = mf_ardf_core_input((state), &physical, (now));           \
        CHECK(!(result).handled);                                           \
        physical.type = InputTypeRelease;                                   \
        (result) = mf_ardf_core_input((state), &physical, (now) + 1U);      \
        CHECK(!(result).handled);                                           \
        physical.type = InputTypeShort;                                     \
        (result) = mf_ardf_core_input((state), &physical, (now) + 2U);      \
    } while(0)

typedef struct {
    char trace[8192];
    size_t used;
    bool allow;
    bool prepare_ok;
    uint8_t prepare_modulation;
} HardwareMock;

static void trace(HardwareMock* mock, char value) {
    if(mock->used + 1U < sizeof(mock->trace)) {
        mock->trace[mock->used++] = value;
        mock->trace[mock->used] = '\0';
    }
}

static bool hw_allowed(void* context, uint32_t frequency_hz, MfArdfModulation modulation) {
    (void)modulation;
    return ((HardwareMock*)context)->allow && frequency_hz != 0U;
}

static bool hw_prepare(void* context, uint32_t frequency_hz, MfArdfModulation modulation) {
    (void)frequency_hz;
    ((HardwareMock*)context)->prepare_modulation = modulation;
    trace(context, 'P');
    return ((HardwareMock*)context)->prepare_ok;
}

static size_t trace_count(const HardwareMock* mock, char value) {
    size_t count = 0U;
    for(size_t i = 0U; i < mock->used; i++)
        if(mock->trace[i] == value) count++;
    return count;
}

static bool hw_mark(void* context, bool on) {
    trace(context, on ? 'M' : 'm');
    return true;
}

static void hw_stop(void* context) {
    trace(context, 'X');
}

static void hw_p15(void* context, bool on) {
    trace(context, on ? 'K' : 'k');
}

static void hw_p16(void* context, bool on) {
    trace(context, on ? 'T' : 't');
}

static void hw_led(void* context, bool on) {
    trace(context, on ? 'L' : 'l');
}

static bool hw_clock(void* context, MfArdfClockTime time) {
    (void)time;
    trace(context, 'C');
    return true;
}

static MfArdfHardwareOps hardware_ops(HardwareMock* mock) {
    return (MfArdfHardwareOps){
        .frequency_allowed = hw_allowed,
        .prepare = hw_prepare,
        .set_mark = hw_mark,
        .stop = hw_stop,
        .set_p15 = hw_p15,
        .set_p16 = hw_p16,
        .set_led = hw_led,
        .set_clock = hw_clock,
        .context = mock,
    };
}

static bool enter_state(MfArdfState* state, HardwareMock* mock) {
    MfArdfHardwareOps ops = hardware_ops(mock);
    MfArdfEnterArgs args = {
        .struct_size = sizeof(args),
        .frequency_hz = 433160000U,
    };
    MorseFlipperMappedFalResult initial;
    return mf_ardf_core_enter(state, &args, &ops, &initial);
}

static int run_numbered_case(
    MfArdfMode mode,
    MfArdfModulation modulation,
    uint8_t wpm,
    uint32_t start_ms,
    bool assisted) {
    HardwareMock mock = {.allow = true, .prepare_ok = true};
    MfArdfState state;
    MfArdfSequence sequence;
    MorseFlipperMappedFalResult result;
    uint32_t slot_ms = mode == MfArdfModeSprint ? 12000U : 60000U;
    uint32_t cycle_ms = mode == MfArdfModeSprint ? 60000U : 300000U;
    uint32_t gap_ms = (uint32_t)mf_ardf_wpm_to_dit_ms(wpm) * 7U;
    uint32_t expected_count;
    uint32_t ui_deadline = start_ms + slot_ms;
    uint32_t identifiers = 0U;
    uint32_t transitions = 0U;
    uint32_t last_start = start_ms;
    bool saw_off = false;

    CHECK(enter_state(&state, &mock));
    state.snapshot.view = MfArdfViewRun;
    state.snapshot.running = true;
    state.snapshot.settings.mode = mode;
    state.snapshot.settings.modulation = modulation;
    state.snapshot.settings.message = MfArdfMessage5;
    state.snapshot.settings.wpm = wpm;
    state.snapshot.settings.light_assistance = assisted;
    state.snapshot.next_deadline_ms = start_ms;
    state.snapshot.gpio_owned = true;
    state.cycle_deadline_ms = start_ms;
    state.deadline_wall_s = 300U;
    CHECK(mf_ardf_sequence_build(&sequence, "MO5", wpm));
    CHECK(sequence.duration_ms <= slot_ms);
    expected_count = 1U + (slot_ms - sequence.duration_ms) / (sequence.duration_ms + gap_ms);

    result = mf_ardf_core_tick(&state, start_ms - MF_ARDF_PTT_LEAD_MS);
    CHECK(state.prepared && state.snapshot.ptt && !state.snapshot.mark);
    result = mf_ardf_core_tick(&state, start_ms);
    CHECK(state.snapshot.transmitting);
    CHECK(state.snapshot.ptt && state.snapshot.mark);
    CHECK(state.snapshot.next_deadline_ms == ui_deadline);
    CHECK(result.backlight_wake == assisted);
    CHECK(!result.backlight_off);
    identifiers = 1U;

    while(state.slot_active) {
        bool was_gap = state.repeat_gap;
        CHECK(++transitions < 4096U);
        if(state.snapshot.transmitting) {
            uint32_t deadline = state.sequence_next_ms;
            if(was_gap) {
                CHECK(state.prepared && state.snapshot.ptt && !state.snapshot.mark);
                CHECK(deadline == last_start + sequence.duration_ms + gap_ms);
                last_start = deadline;
            }
            result = mf_ardf_core_tick(&state, state.sequence_next_ms);
            if(was_gap) {
                identifiers++;
                CHECK(state.snapshot.transmitting && state.snapshot.mark);
                CHECK(result.backlight_wake == assisted);
                CHECK(!result.backlight_off);
            }
            if(result.backlight_off) saw_off = true;
        } else {
            result = mf_ardf_core_tick(&state, ui_deadline);
        }
        CHECK(
            state.snapshot.next_deadline_ms ==
            (state.slot_active ? ui_deadline : start_ms + cycle_ms));
    }

    CHECK(identifiers == expected_count);
    CHECK(mf_ardf_time_reached(start_ms + slot_ms, last_start + sequence.duration_ms));
    CHECK(
        (int32_t)((start_ms + slot_ms) -
                  (last_start + sequence.duration_ms + gap_ms + sequence.duration_ms)) < 0);
    CHECK(state.snapshot.next_deadline_ms == start_ms + cycle_ms);
    CHECK(!state.snapshot.ptt && !state.snapshot.mark);
    CHECK(saw_off == assisted);
    CHECK(trace_count(&mock, 'P') == 1U);
    mf_ardf_core_leave(&state);
    return 0;
}

static int run_continuous_case(MfArdfMode mode, MfArdfMessage message, bool assisted) {
    HardwareMock mock = {.allow = true, .prepare_ok = true};
    MfArdfState state;
    InputEvent ok = {.key = InputKeyOk, .type = InputTypeShort};
    InputEvent back = {.key = InputKeyBack, .type = InputTypeShort};
    MorseFlipperMappedFalResult result;
    uint32_t first_start = 1234U;
    uint32_t expected_repeat;
    unsigned transitions = 0U;

    CHECK(enter_state(&state, &mock));
    CHECK(mf_ardf_core_set_view(&state, MfArdfViewClock, first_start).handled);
    state.snapshot.settings.mode = mode;
    state.snapshot.settings.message = message;
    state.snapshot.settings.wpm = 10U;
    state.snapshot.settings.light_assistance = assisted;
    state.live_time = (MfArdfClockTime){12U, 34U, 56U};
    result = mf_ardf_core_input(&state, &ok, first_start);
    CHECK(result.handled && state.snapshot.run_pending);
    CHECK(!state.snapshot.transmitting);
    result = mf_ardf_core_activate_run(&state, first_start);
    CHECK(result.handled && state.snapshot.transmitting && state.snapshot.mark);
    CHECK(result.backlight_wake == assisted);
    CHECK(!state.sampling);
    expected_repeat = first_start + state.sequence.duration_ms + MF_ARDF_REPEAT_GAP_MS;

    while(state.snapshot.transmitting) {
        CHECK(++transitions < 256U);
        result = mf_ardf_core_tick(&state, state.sequence_next_ms);
    }
    CHECK(result.backlight_off == assisted);
    CHECK(state.repeat_gap && !state.prepared && !state.snapshot.ptt && !state.snapshot.mark);
    CHECK(state.sequence_next_ms == expected_repeat);
    CHECK(state.snapshot.next_deadline_ms == expected_repeat);
    CHECK(
        mf_ardf_countdown_seconds(expected_repeat - 4001U, state.snapshot.next_deadline_ms) == 5U);
    CHECK(
        mf_ardf_countdown_seconds(expected_repeat - 4000U, state.snapshot.next_deadline_ms) == 4U);
    result = mf_ardf_core_tick(&state, expected_repeat - MF_ARDF_PTT_LEAD_MS - 1U);
    CHECK(state.repeat_gap && !state.prepared && !state.snapshot.ptt);
    result = mf_ardf_core_tick(&state, expected_repeat - MF_ARDF_PTT_LEAD_MS);
    CHECK(!state.repeat_gap && state.prepared && state.preamble && state.snapshot.ptt);
    CHECK(!state.snapshot.transmitting && !state.snapshot.mark);
    result = mf_ardf_core_tick(&state, expected_repeat);
    CHECK(!state.preamble && state.snapshot.transmitting && state.snapshot.mark);
    CHECK(state.snapshot.next_deadline_ms == 0U);
    CHECK(result.backlight_wake == assisted);
    result = mf_ardf_core_input(&state, &back, expected_repeat + 1U);
    CHECK(result.handled && state.modal);
    CHECK(!state.snapshot.transmitting && !state.snapshot.ptt && !state.snapshot.mark);
    CHECK(result.backlight_off == assisted);
    CHECK(mf_ardf_core_host_action_result(
              &state, MfArdfHostActionShowStopConfirmation, true, expected_repeat + 1U)
              .handled);
    CHECK(state.snapshot.view == MfArdfViewSettings);
    mf_ardf_core_leave(&state);
    return 0;
}

static int run_numbered_entry_offsets(MfArdfMode mode, MfArdfModulation modulation) {
    MfArdfSequence sequence;
    uint32_t slot_ms = mode == MfArdfModeSprint ? 12000U : 60000U;
    CHECK(mf_ardf_sequence_build(&sequence, "MOE", 30U));
    for(uint32_t offset_ms = 0U; offset_ms < slot_ms; offset_ms++) {
        HardwareMock mock = {.allow = true, .prepare_ok = true};
        MfArdfState state;
        InputEvent ok = {.key = InputKeyOk, .type = InputTypeShort};
        uint32_t now_ms = 100000U + offset_ms % 1000U;
        uint32_t wall_s = offset_ms / 1000U;
        uint32_t second_edge_ms = now_ms - offset_ms % 1000U;
        uint32_t slot_start_ms = second_edge_ms - wall_s * 1000U;
        uint32_t slot_end_ms = slot_start_ms + slot_ms;
        uint32_t first_mark_ms =
            now_ms + (modulation == MfArdfModulationCwfm ? MF_ARDF_CWFM_ACQUIRE_MS : 0U);
        bool fits = mf_ardf_time_reached(slot_end_ms, first_mark_ms + sequence.duration_ms);
        CHECK(enter_state(&state, &mock));
        state.snapshot.view = MfArdfViewClock;
        state.snapshot.clock_state = MfArdfClockConfirm;
        state.snapshot.settings.mode = mode;
        state.snapshot.settings.modulation = modulation;
        state.snapshot.settings.message = MfArdfMessage1;
        state.snapshot.settings.wpm = 30U;
        state.live_time = (MfArdfClockTime){0U, 0U, (uint8_t)wall_s};
        state.calibration_valid = true;
        state.accepted_wall_s = wall_s;
        state.accepted_edge_ms = second_edge_ms;
        MorseFlipperMappedFalResult result = mf_ardf_core_input(&state, &ok, now_ms);
        CHECK(result.handled && state.snapshot.view == MfArdfViewRun);
        CHECK(state.slot_active && state.slot_end_ms == slot_end_ms);
        CHECK(state.snapshot.next_deadline_ms == slot_end_ms);
        CHECK(result.transition == fits && state.snapshot.run_pending == fits);
        CHECK(!state.snapshot.gpio_owned && mock.used == 0U);
        if(fits) {
            result = mf_ardf_core_activate_run(&state, now_ms);
            CHECK(result.handled && state.snapshot.gpio_owned);
            CHECK(trace_count(&mock, 'P') == 1U && trace_count(&mock, 'T') == 1U);
            if(modulation == MfArdfModulationCwfm) {
                CHECK(state.preamble && !state.snapshot.transmitting && !state.snapshot.mark);
                CHECK(!result.backlight_wake && strchr(mock.trace, 'M') == NULL);
            } else {
                CHECK(!state.preamble && state.snapshot.transmitting && state.snapshot.mark);
            }
        } else {
            CHECK(!state.prepared && !state.preamble && !state.snapshot.ptt);
            CHECK(strchr(mock.trace, 'P') == NULL && strchr(mock.trace, 'T') == NULL);
        }
        mf_ardf_core_leave(&state);
    }
    return 0;
}

int main(void) {
    static const char* const texts[] = {"MOE", "MOI", "MOS", "MOH", "MO5", "S", "MO"};
    static const char* const morse[] = {
        "-- --- .", "-- --- ..", "-- --- ...", "-- --- ....", "-- --- .....", "...", "-- ---"};
    static const uint8_t repeat_wpms[] = {8U, 10U, 15U, 30U};
    static const uint16_t custom_intervals[] = {
        3U,   5U,   8U,   10U,  12U,  15U,  20U,  24U,  30U,  45U,  60U,  75U,  90U,  105U,
        120U, 180U, 240U, 300U, 360U, 420U, 480U, 540U, 600U, 660U, 720U, 780U, 840U, 900U,
    };
    MfArdfSequence sequence;
    unsigned i;

    for(i = 0U; i < MfArdfMessageCount; i++) {
        CHECK(strcmp(mf_ardf_identifier_text((MfArdfMessage)i), texts[i]) == 0);
        CHECK(strcmp(mf_ardf_identifier_morse((MfArdfMessage)i), morse[i]) == 0);
    }
    for(i = 8U; i <= 30U; i++)
        CHECK(mf_ardf_wpm_to_dit_ms(i) == (1200U + i / 2U) / i);
    CHECK(mf_ardf_wpm_to_dit_ms(22U) == 55U);
    CHECK(mf_ardf_sequence_build(&sequence, "A B", 10U));
    CHECK(sequence.count == 11U);
    CHECK(sequence.steps[0].mark && sequence.steps[0].units == 1U);
    CHECK(!sequence.steps[3].mark && sequence.steps[3].units == 7U);
    CHECK(sequence.steps[sequence.count - 1U].mark);
    CHECK(sequence.duration_ms == 21U * 120U);

    /* A delayed dispatcher tick may lengthen a step, but must never collapse the next mark. */
    {
        HardwareMock mock = {.allow = true, .prepare_ok = true};
        MfArdfState state;
        uint32_t deadline = 1000U;
        uint16_t dit_ms = mf_ardf_wpm_to_dit_ms(30U);
        CHECK(enter_state(&state, &mock));
        state.snapshot.view = MfArdfViewRun;
        state.snapshot.running = true;
        state.snapshot.gpio_owned = true;
        state.snapshot.settings.mode = MfArdfModeCustom;
        state.snapshot.settings.wpm = 30U;
        state.snapshot.settings.interval_index = 0U;
        memcpy(state.snapshot.settings.custom, "I", 2U);
        state.snapshot.next_deadline_ms = deadline;
        state.deadline_wall_s = 60U;
        CHECK(mf_ardf_core_tick(&state, deadline - MF_ARDF_PTT_LEAD_MS).handled);
        CHECK(mf_ardf_core_tick(&state, deadline).handled);
        CHECK(state.snapshot.mark && state.sequence_next_ms == deadline + dit_ms);

        uint32_t late_space_start = state.sequence_next_ms + 17U;
        CHECK(mf_ardf_core_tick(&state, late_space_start).handled);
        CHECK(!state.snapshot.mark && state.sequence_next_ms == late_space_start + dit_ms);

        uint32_t late_final_mark = state.sequence_next_ms + 33U;
        CHECK(mf_ardf_core_tick(&state, late_final_mark).handled);
        CHECK(state.snapshot.mark && state.sequence_next_ms == late_final_mark + dit_ms);
        CHECK(trace_count(&mock, 'M') == 2U);
        CHECK(mf_ardf_core_tick(&state, state.sequence_next_ms - 1U).handled);
        CHECK(state.snapshot.mark);
        CHECK(mf_ardf_core_tick(&state, state.sequence_next_ms).handled);
        CHECK(!state.snapshot.mark && !state.snapshot.transmitting);
        mf_ardf_core_leave(&state);
    }

    for(i = 0U; i < 5U; i++) {
        uint32_t standard_phase = 300U + i * 60U;
        uint32_t sprint_phase = 60U + i * 12U;
        CHECK(mf_ardf_target_phase(MfArdfModeStandard, (MfArdfMessage)i) == i * 60U);
        CHECK(mf_ardf_target_phase(MfArdfModeSprint, (MfArdfMessage)i) == i * 12U);
        CHECK(
            mf_ardf_next_cycle_wall_s(standard_phase - 1U, MfArdfModeStandard, i) ==
            standard_phase);
        CHECK(
            mf_ardf_next_cycle_wall_s(standard_phase, MfArdfModeStandard, i) ==
            standard_phase + 300U);
        CHECK(
            mf_ardf_next_cycle_wall_s(sprint_phase + 59U, MfArdfModeSprint, i) ==
            sprint_phase + 60U);
    }

    CHECK(mf_ardf_countdown_seconds(1000U, 1001U) == 1U);
    CHECK(mf_ardf_countdown_seconds(1000U, 2001U) == 2U);
    CHECK(mf_ardf_time_reached(3U, UINT32_MAX - 2U));
    CHECK(!mf_ardf_time_reached(UINT32_MAX - 2U, 3U));
    CHECK(mf_ardf_progress_width(50U, UINT32_MAX - 49U, 150U) == 60U);
    CHECK(sizeof(custom_intervals) / sizeof(custom_intervals[0]) == MF_ARDF_INTERVAL_COUNT);
    for(i = 0U; i < MF_ARDF_INTERVAL_COUNT; i++) {
        uint32_t interval = custom_intervals[i];
        uint32_t anchor = 60U;
        uint32_t candidate = anchor + interval * 37U;
        CHECK(mf_ardf_interval_seconds(i) == interval);
        CHECK(mf_ardf_custom_next_wall_s(anchor, interval, candidate) == candidate);
        CHECK(mf_ardf_custom_next_wall_s(anchor, interval, candidate - 1U) == candidate);
        CHECK(
            (mf_ardf_custom_next_wall_s(anchor, interval, candidate + 1U) - anchor) % interval ==
            0U);
    }
    CHECK(mf_ardf_custom_next_wall_s(0U, 8U, 57U) == 64U);
    CHECK(mf_ardf_custom_next_wall_s(0U, 24U, 49U) == 72U);
    CHECK(mf_ardf_custom_next_wall_s(86340U, 8U, 86401U) == 86404U);

    for(i = 0U; i < sizeof(repeat_wpms); i++) {
        CHECK(
            run_numbered_case(
                MfArdfModeStandard, MfArdfModulationCw, repeat_wpms[i], 1000U, true) == 0);
        CHECK(
            run_numbered_case(MfArdfModeSprint, MfArdfModulationCw, repeat_wpms[i], 1000U, true) ==
            0);
    }
    CHECK(
        run_numbered_case(MfArdfModeSprint, MfArdfModulationCwfm, 30U, UINT32_MAX - 5000U, true) ==
        0);
    CHECK(run_numbered_case(MfArdfModeSprint, MfArdfModulationCw, 10U, 1000U, false) == 0);

    CHECK(run_continuous_case(MfArdfModeStandard, MfArdfMessageS, true) == 0);
    CHECK(run_continuous_case(MfArdfModeSprint, MfArdfMessageMo, true) == 0);
    CHECK(run_continuous_case(MfArdfModeStandard, MfArdfMessageS, false) == 0);
    CHECK(run_numbered_entry_offsets(MfArdfModeStandard, MfArdfModulationCw) == 0);
    CHECK(run_numbered_entry_offsets(MfArdfModeStandard, MfArdfModulationCwfm) == 0);
    CHECK(run_numbered_entry_offsets(MfArdfModeSprint, MfArdfModulationCw) == 0);
    CHECK(run_numbered_entry_offsets(MfArdfModeSprint, MfArdfModulationCwfm) == 0);

    /* The first precise RTC edge rebases an active join without restarting its identifier. */
    {
        HardwareMock mock = {.allow = true, .prepare_ok = true};
        MfArdfState state;
        InputEvent ok = {.key = InputKeyOk, .type = InputTypeShort};
        CHECK(enter_state(&state, &mock));
        CHECK(mf_ardf_core_set_view(&state, MfArdfViewClock, 99990U).handled);
        state.snapshot.clock_state = MfArdfClockConfirm;
        state.snapshot.settings.mode = MfArdfModeSprint;
        state.snapshot.settings.modulation = MfArdfModulationCw;
        state.snapshot.settings.message = MfArdfMessage1;
        state.snapshot.settings.wpm = 30U;
        state.live_time = (MfArdfClockTime){0U, 0U, 3U};
        mf_ardf_core_rtc_sample(&state, state.live_time, 99997U, 99997U);

        MorseFlipperMappedFalResult result = mf_ardf_core_input(&state, &ok, 100000U);
        CHECK(result.handled && result.transition && state.snapshot.run_pending);
        CHECK(state.uncalibrated_join && state.sampling);
        CHECK(state.slot_end_ms == 109000U && state.cycle_deadline_ms == 157000U);
        CHECK(mf_ardf_core_activate_run(&state, 100000U).handled);
        CHECK(state.snapshot.transmitting && state.snapshot.mark);
        uint32_t sequence_next_ms = state.sequence_next_ms;
        size_t marks = trace_count(&mock, 'M');

        mf_ardf_core_rtc_sample(&state, (MfArdfClockTime){0U, 0U, 3U}, 100247U, 100247U);
        mf_ardf_core_rtc_sample(&state, (MfArdfClockTime){0U, 0U, 4U}, 100249U, 100251U);
        CHECK(state.calibration_valid && !state.uncalibrated_join && !state.sampling);
        CHECK(state.accepted_edge_ms == 100249U && state.accepted_wall_s == 4U);
        CHECK(state.slot_end_ms == 108249U);
        CHECK(state.cycle_deadline_ms == 156249U);
        CHECK(state.deadline_wall_s == 60U);
        CHECK(state.snapshot.segment_start_ms == 96249U);
        CHECK(state.snapshot.next_deadline_ms == 108249U);
        CHECK(state.snapshot.transmitting && state.snapshot.mark);
        CHECK(state.sequence_next_ms == sequence_next_ms);
        CHECK(trace_count(&mock, 'M') == marks);
        mf_ardf_core_leave(&state);
    }

    /* A known CWFM deadline asserts PTT at D-250 and keys at D when already owned. */
    {
        HardwareMock mock = {.allow = true, .prepare_ok = true};
        MfArdfState state;
        uint32_t deadline = 10000U;
        CHECK(enter_state(&state, &mock));
        state.snapshot.view = MfArdfViewRun;
        state.snapshot.running = true;
        state.snapshot.gpio_owned = true;
        state.snapshot.settings.mode = MfArdfModeSprint;
        state.snapshot.settings.modulation = MfArdfModulationCwfm;
        state.snapshot.settings.message = MfArdfMessage1;
        state.snapshot.settings.wpm = 30U;
        state.snapshot.settings.light_assistance = 1U;
        state.snapshot.next_deadline_ms = deadline;
        state.cycle_deadline_ms = deadline;
        CHECK(mf_ardf_core_tick(&state, deadline - MF_ARDF_CWFM_ACQUIRE_MS).handled);
        CHECK(state.prepared && state.snapshot.ptt && !state.snapshot.mark);
        CHECK(strcmp(mock.trace, "PT") == 0);
        MorseFlipperMappedFalResult result = mf_ardf_core_tick(&state, deadline);
        CHECK(state.snapshot.transmitting && state.snapshot.mark && result.backlight_wake);
        CHECK(strncmp(mock.trace, "PTMKL", 5U) == 0);
        CHECK(strchr(mock.trace, 'X') == NULL && trace_count(&mock, 'P') == 1U);
        uint32_t ui_deadline = state.snapshot.next_deadline_ms;
        while(state.snapshot.transmitting) {
            mf_ardf_core_tick(&state, state.sequence_next_ms);
            CHECK(state.snapshot.next_deadline_ms == ui_deadline);
        }
        CHECK(trace_count(&mock, 'P') == 1U);
        mf_ardf_core_leave(&state);
    }

    /* Reaching a deadline without host GPIO ownership skips it without touching hardware. */
    {
        HardwareMock mock = {.allow = true, .prepare_ok = true};
        MfArdfState state;
        uint32_t deadline = 10000U;
        CHECK(enter_state(&state, &mock));
        state.snapshot.view = MfArdfViewRun;
        state.snapshot.running = true;
        state.snapshot.settings.mode = MfArdfModeSprint;
        state.snapshot.settings.message = MfArdfMessage1;
        state.snapshot.next_deadline_ms = deadline;
        state.cycle_deadline_ms = deadline;
        CHECK(mf_ardf_core_tick(&state, deadline).handled);
        CHECK(!state.snapshot.run_pending && !state.snapshot.gpio_owned && !state.prepared);
        CHECK(!state.snapshot.ptt && !state.snapshot.transmitting && !state.snapshot.mark);
        CHECK(state.snapshot.next_deadline_ms == deadline + 60000U);
        CHECK(mock.used == 0U);
        mf_ardf_core_leave(&state);
    }

    /* An uncalibrated imminent edge anchors only; the next eligible event owns hardware later. */
    {
        HardwareMock mock = {.allow = true, .prepare_ok = true};
        MfArdfState state;
        InputEvent ok = {.key = InputKeyOk, .type = InputTypeShort};
        CHECK(enter_state(&state, &mock));
        mf_ardf_core_set_view(&state, MfArdfViewClock, 900U);
        state.snapshot.settings.mode = MfArdfModeStandard;
        state.snapshot.settings.message = MfArdfMessage1;
        state.live_time = (MfArdfClockTime){0U, 4U, 59U};
        mf_ardf_core_rtc_sample(&state, state.live_time, 995U, 995U);
        MorseFlipperMappedFalResult result = mf_ardf_core_input(&state, &ok, 1000U);
        CHECK(result.handled && !result.transition && state.sampling);
        CHECK(!state.snapshot.run_pending && !state.snapshot.gpio_owned && mock.used == 0U);
        mf_ardf_core_rtc_sample(&state, (MfArdfClockTime){0U, 5U, 0U}, 999U, 1001U);
        CHECK(state.calibration_valid && !state.sampling);
        CHECK(state.snapshot.next_deadline_ms == state.accepted_edge_ms + 300000U);
        CHECK(!state.snapshot.run_pending && !state.snapshot.gpio_owned && mock.used == 0U);
        mf_ardf_core_tick(&state, state.snapshot.next_deadline_ms - MF_ARDF_PTT_LEAD_MS);
        CHECK(state.snapshot.run_pending && !state.snapshot.gpio_owned && mock.used == 0U);
        mf_ardf_core_leave(&state);
    }

    /* A lead tick up to 5 ms late still gives CWFM a full 250 ms quiet carrier. */
    {
        HardwareMock mock = {.allow = true, .prepare_ok = true};
        MfArdfState state;
        uint32_t deadline = 10000U;
        uint32_t takeover_ms = deadline - MF_ARDF_CWFM_ACQUIRE_MS + 5U;
        CHECK(enter_state(&state, &mock));
        state.snapshot.view = MfArdfViewRun;
        state.snapshot.running = true;
        state.snapshot.settings.mode = MfArdfModeSprint;
        state.snapshot.settings.modulation = MfArdfModulationCwfm;
        state.snapshot.settings.message = MfArdfMessage1;
        state.snapshot.settings.wpm = 30U;
        state.snapshot.next_deadline_ms = deadline;
        state.cycle_deadline_ms = deadline;
        MorseFlipperMappedFalResult result = mf_ardf_core_tick(&state, takeover_ms);
        CHECK(result.transition && state.snapshot.run_pending && mock.used == 0U);
        result = mf_ardf_core_activate_run(&state, takeover_ms);
        CHECK(result.handled && state.snapshot.gpio_owned && state.prepared && state.preamble);
        CHECK(state.sequence_next_ms == takeover_ms + MF_ARDF_CWFM_ACQUIRE_MS);
        CHECK(strcmp(mock.trace, "PT") == 0);
        mf_ardf_core_tick(&state, deadline);
        CHECK(!state.snapshot.transmitting && !state.snapshot.mark);
        mf_ardf_core_tick(&state, state.sequence_next_ms - 1U);
        CHECK(!state.snapshot.transmitting && !state.snapshot.mark);
        mf_ardf_core_tick(&state, state.sequence_next_ms);
        CHECK(state.snapshot.transmitting && state.snapshot.mark);
        CHECK(strncmp(mock.trace, "PTMKL", 5U) == 0);
        mf_ardf_core_leave(&state);
    }

    /* Later Custom CWFM events retain ownership but still reacquire for the full 250 ms. */
    {
        HardwareMock mock = {.allow = true, .prepare_ok = true};
        MfArdfState state;
        uint32_t deadline = 10000U;
        uint32_t takeover_ms = deadline - MF_ARDF_CWFM_ACQUIRE_MS + 5U;
        CHECK(enter_state(&state, &mock));
        state.snapshot.view = MfArdfViewRun;
        state.snapshot.running = true;
        state.snapshot.gpio_owned = true;
        state.snapshot.settings.mode = MfArdfModeCustom;
        state.snapshot.settings.modulation = MfArdfModulationCwfm;
        state.snapshot.settings.interval_index = 0U;
        memcpy(state.snapshot.settings.custom, "E", 2U);
        state.snapshot.next_deadline_ms = deadline;
        state.deadline_wall_s = 60U;
        CHECK(mf_ardf_core_tick(&state, takeover_ms).handled);
        CHECK(state.prepared && state.preamble && state.snapshot.ptt && !state.snapshot.mark);
        CHECK(state.sequence_next_ms == takeover_ms + MF_ARDF_CWFM_ACQUIRE_MS);
        uint32_t grid_deadline = state.snapshot.next_deadline_ms;
        CHECK(grid_deadline == deadline + 3000U && strcmp(mock.trace, "PT") == 0);
        mf_ardf_core_tick(&state, deadline);
        CHECK(!state.snapshot.transmitting && !state.snapshot.mark);
        mf_ardf_core_tick(&state, state.sequence_next_ms);
        CHECK(state.snapshot.transmitting && state.snapshot.mark);
        CHECK(state.snapshot.next_deadline_ms == grid_deadline);
        mf_ardf_core_leave(&state);
    }

    {
        HardwareMock mock = {.allow = true, .prepare_ok = true};
        MfArdfState state;
        InputEvent ok = {.key = InputKeyOk, .type = InputTypeShort};
        InputEvent back = {.key = InputKeyBack, .type = InputTypeShort};
        CHECK(enter_state(&state, &mock));
        state.snapshot.view = MfArdfViewClock;
        state.snapshot.clock_state = MfArdfClockConfirm;
        state.snapshot.settings.mode = MfArdfModeCustom;
        state.snapshot.settings.modulation = MfArdfModulationCwfm;
        state.snapshot.settings.light_assistance = 1U;
        memcpy(state.snapshot.settings.custom, "E", 2U);
        state.live_time = (MfArdfClockTime){12U, 0U, 10U};
        CHECK(mf_ardf_core_input(&state, &ok, 2000U).transition);
        MorseFlipperMappedFalResult result = mf_ardf_core_activate_run(&state, 2000U);
        CHECK(result.handled && state.preamble && state.snapshot.ptt);
        CHECK(!state.snapshot.transmitting && !state.snapshot.mark && !result.backlight_wake);
        CHECK(state.snapshot.next_deadline_ms == 2250U && strcmp(mock.trace, "PT") == 0);
        result = mf_ardf_core_tick(&state, 2249U);
        CHECK(state.preamble && !result.backlight_wake && strcmp(mock.trace, "PT") == 0);
        result = mf_ardf_core_tick(&state, 2250U);
        CHECK(!state.preamble && state.snapshot.transmitting && state.snapshot.mark);
        CHECK(result.backlight_wake && strcmp(mock.trace, "PTMKL") == 0);
        uint32_t grid_deadline = state.snapshot.next_deadline_ms;
        CHECK(grid_deadline != 2250U);
        while(state.snapshot.transmitting) {
            mf_ardf_core_tick(&state, state.sequence_next_ms);
            CHECK(state.snapshot.next_deadline_ms == grid_deadline);
        }
        mf_ardf_core_leave(&state);

        memset(&mock, 0, sizeof(mock));
        mock.allow = true;
        mock.prepare_ok = true;
        CHECK(enter_state(&state, &mock));
        state.snapshot.view = MfArdfViewClock;
        state.snapshot.clock_state = MfArdfClockConfirm;
        state.snapshot.settings.mode = MfArdfModeStandard;
        state.snapshot.settings.modulation = MfArdfModulationCwfm;
        state.snapshot.settings.message = MfArdfMessageS;
        state.live_time = (MfArdfClockTime){12U, 0U, 10U};
        CHECK(mf_ardf_core_input(&state, &ok, 3000U).transition);
        CHECK(mf_ardf_core_activate_run(&state, 3000U).handled && state.preamble);
        result = mf_ardf_core_input(&state, &back, 3001U);
        CHECK(result.handled && state.modal && !state.preamble);
        CHECK(!state.snapshot.ptt && !state.snapshot.mark && !state.snapshot.transmitting);
        CHECK(strchr(mock.trace, 'X') != NULL && strchr(mock.trace, 'M') == NULL);
        mf_ardf_core_leave(&state);
    }

    {
        HardwareMock mock = {.allow = true, .prepare_ok = true};
        MfArdfState state;
        CHECK(enter_state(&state, &mock));
        state.snapshot.view = MfArdfViewRun;
        state.snapshot.running = true;
        state.snapshot.settings.mode = MfArdfModeStandard;
        state.snapshot.settings.message = MfArdfMessage1;
        state.snapshot.next_deadline_ms = 9000U;
        state.sampling = false;
        state.calibration_valid = true;
        state.live_time = (MfArdfClockTime){23U, 59U, 59U};
        uint32_t fixed_deadline = state.snapshot.next_deadline_ms;
        mf_ardf_core_rtc_sample(&state, (MfArdfClockTime){0U, 0U, 0U}, 4000U, 4001U);
        CHECK(
            state.live_time.hour == 0U && state.live_time.minute == 0U &&
            state.live_time.second == 0U);
        CHECK(state.snapshot.next_deadline_ms == fixed_deadline);
        mf_ardf_core_leave(&state);
    }

    {
        HardwareMock mock = {.allow = true, .prepare_ok = true};
        MfArdfState state;
        CHECK(enter_state(&state, &mock));
        strcpy(state.snapshot.settings.custom, "FOX");
        state.snapshot.host_action = MfArdfHostActionOpenTextInput;
        CHECK(mf_ardf_core_text_input_result(&state, " a?b9 ", true, 1U).handled);
        CHECK(strcmp(state.snapshot.settings.custom, "AB9") == 0);
        CHECK(state.snapshot.settings.selected_row == MF_ARDF_CUSTOM_ROW);
        state.snapshot.host_action = MfArdfHostActionOpenTextInput;
        CHECK(mf_ardf_core_text_input_result(&state, "ZZ", false, 2U).handled);
        CHECK(strcmp(state.snapshot.settings.custom, "AB9") == 0);
        CHECK(state.snapshot.settings.selected_row == MF_ARDF_CUSTOM_ROW);
        mf_ardf_core_leave(&state);
    }

    for(uint8_t direction = 0U; direction < 2U; direction++) {
        HardwareMock mock = {.allow = true, .prepare_ok = true};
        MfArdfState state;
        MorseFlipperMappedFalResult result;
        InputKey key = direction == 0U ? InputKeyRight : InputKeyLeft;
        CHECK(enter_state(&state, &mock));
        mf_ardf_core_set_view(&state, MfArdfViewClock, 0U);
        for(uint8_t step = 0U; step < 3U; step++) {
            PHYSICAL_SHORT(&state, key, 10U + step * 10U, result);
            CHECK(result.handled && state.snapshot.clock_state == MfArdfClockSelect);
            CHECK(
                state.snapshot.clock_field ==
                (direction == 0U ? MfArdfClockHours + step : MfArdfClockSeconds - step));
        }
        PHYSICAL_SHORT(&state, key, 40U, result);
        CHECK(result.handled && state.snapshot.clock_state == MfArdfClockConfirm);
        CHECK(state.snapshot.view == MfArdfViewClock);
        mf_ardf_core_leave(&state);
    }

    {
        HardwareMock mock = {.allow = true, .prepare_ok = true};
        MfArdfState state;
        MorseFlipperMappedFalResult result;
        CHECK(enter_state(&state, &mock));
        mf_ardf_core_set_view(&state, MfArdfViewClock, 0U);
        PHYSICAL_SHORT(&state, InputKeyUp, 10U, result);
        CHECK(!result.handled && state.snapshot.clock_state == MfArdfClockConfirm);
        PHYSICAL_SHORT(&state, InputKeyDown, 20U, result);
        CHECK(!result.handled && state.snapshot.clock_state == MfArdfClockConfirm);
        CHECK(state.snapshot.view == MfArdfViewClock);
        mf_ardf_core_leave(&state);
    }

    for(uint8_t field = MfArdfClockHours; field <= MfArdfClockSeconds; field++) {
        HardwareMock mock = {.allow = true, .prepare_ok = true};
        MfArdfState state;
        MorseFlipperMappedFalResult result;
        CHECK(enter_state(&state, &mock));
        mf_ardf_core_set_view(&state, MfArdfViewClock, 0U);
        state.live_time = (MfArdfClockTime){12U, 34U, 56U};
        state.draft_time = state.live_time;

        for(uint8_t step = 0U; step <= field; step++)
            PHYSICAL_SHORT(&state, InputKeyRight, 10U + step * 4U, result);
        CHECK(result.handled && state.snapshot.clock_state == MfArdfClockSelect);
        CHECK(state.snapshot.clock_field == field);
        PHYSICAL_SHORT(&state, InputKeyOk, 25U, result);
        CHECK(result.handled && state.snapshot.clock_state == MfArdfClockEdit);
        PHYSICAL_SHORT(&state, InputKeyUp, 30U, result);
        CHECK(result.handled);
        PHYSICAL_SHORT(&state, InputKeyBack, 40U, result);
        CHECK(result.handled && state.snapshot.clock_state == MfArdfClockSelect);
        CHECK(state.snapshot.view == MfArdfViewClock);
        CHECK(
            state.draft_time.hour == state.live_time.hour &&
            state.draft_time.minute == state.live_time.minute &&
            state.draft_time.second == state.live_time.second);
        PHYSICAL_SHORT(&state, InputKeyBack, 45U, result);
        CHECK(result.handled && state.snapshot.clock_state == MfArdfClockConfirm);
        CHECK(state.snapshot.view == MfArdfViewClock);
        PHYSICAL_SHORT(&state, InputKeyBack, 48U, result);
        CHECK(result.handled && state.snapshot.view == MfArdfViewSettings);
        CHECK(!state.snapshot.run_pending && !state.snapshot.gpio_owned);

        mf_ardf_core_set_view(&state, MfArdfViewClock, 50U);
        state.live_time = (MfArdfClockTime){12U, 34U, 56U};
        state.draft_time = state.live_time;
        for(uint8_t step = 0U; step <= field; step++)
            PHYSICAL_SHORT(&state, InputKeyRight, 60U + step * 4U, result);
        CHECK(result.handled && state.snapshot.clock_field == field);
        PHYSICAL_SHORT(&state, InputKeyOk, 75U, result);
        CHECK(result.handled && state.snapshot.clock_state == MfArdfClockEdit);
        for(uint32_t i = 0U; i < 8U; i++) {
            PHYSICAL_SHORT(&state, InputKeyUp, 100U + i * 10U, result);
            CHECK(result.handled && state.snapshot.clock_state == MfArdfClockEdit);
        }
        for(uint32_t i = 0U; i < 8U; i++) {
            PHYSICAL_SHORT(&state, InputKeyDown, 200U + i * 10U, result);
            CHECK(result.handled && state.snapshot.clock_state == MfArdfClockEdit);
        }
        for(uint32_t now_ms = 300U; now_ms < 1300U; now_ms += 5U) {
            mf_ardf_core_rtc_sample(
                &state,
                (MfArdfClockTime){12U, 34U, (uint8_t)(56U + now_ms / 1000U)},
                now_ms,
                now_ms);
            result = mf_ardf_core_tick(&state, now_ms);
            CHECK(!result.redraw);
        }
        PHYSICAL_SHORT(&state, InputKeyOk, 1400U, result);
        CHECK(result.handled && state.snapshot.clock_state == MfArdfClockConfirm);
        state.snapshot.settings.mode = MfArdfModeCustom;
        memcpy(state.snapshot.settings.custom, "E", 2U);
        PHYSICAL_SHORT(&state, InputKeyOk, 1410U, result);
        CHECK(result.handled && result.transition && state.snapshot.view == MfArdfViewRun);
        CHECK(mf_ardf_core_activate_run(&state, 1412U).handled);
        CHECK(strchr(mock.trace, 'P') != NULL);
        mf_ardf_core_leave(&state);
    }

    {
        HardwareMock mock = {.allow = true, .prepare_ok = true};
        MfArdfState state;
        unsigned redraws = 0U;
        CHECK(enter_state(&state, &mock));
        state.snapshot.view = MfArdfViewRun;
        state.snapshot.running = true;
        state.snapshot.settings.mode = MfArdfModeCustom;
        for(uint32_t now_ms = 0U; now_ms < 1000U; now_ms += 5U)
            if(mf_ardf_core_tick(&state, now_ms).redraw) redraws++;
        CHECK(redraws == 20U);
        mf_ardf_core_leave(&state);
    }

    {
        HardwareMock mock = {.allow = false, .prepare_ok = true};
        MfArdfState state;
        InputEvent ok = {.key = InputKeyOk, .type = InputTypeShort};
        CHECK(enter_state(&state, &mock));
        mf_ardf_core_set_view(&state, MfArdfViewClock, 0U);
        CHECK(mf_ardf_core_input(&state, &ok, 1U).handled);
        CHECK(!state.snapshot.run_pending && !state.snapshot.gpio_owned);
        CHECK(state.snapshot.host_action == MfArdfHostActionShowError);
        CHECK(mock.used == 0U);
        mf_ardf_core_leave(&state);
    }

    {
        HardwareMock mock = {.allow = true, .prepare_ok = true};
        MfArdfState state;
        InputEvent ok = {.key = InputKeyOk, .type = InputTypeShort};
        CHECK(enter_state(&state, &mock));
        mf_ardf_core_set_view(&state, MfArdfViewClock, 500U);
        state.snapshot.settings.mode = MfArdfModeCustom;
        state.snapshot.settings.interval_index = 2U;
        memcpy(state.snapshot.settings.custom, "E", 2U);
        state.live_time = (MfArdfClockTime){12U, 0U, 59U};
        state.draft_time = state.live_time;
        mf_ardf_core_rtc_sample(&state, state.live_time, 495U, 495U);
        CHECK(mf_ardf_core_input(&state, &ok, 500U).handled);
        CHECK(mf_ardf_core_activate_run(&state, 500U).handled);
        CHECK(state.snapshot.transmitting);
        CHECK(state.custom_anchor_wall_s == 12U * 3600U + 60U);
        mf_ardf_core_rtc_sample(&state, state.live_time, 995U, 995U);
        mf_ardf_core_rtc_sample(&state, (MfArdfClockTime){12U, 1U, 0U}, 999U, 1001U);
        CHECK(state.calibration_valid);
        CHECK(state.snapshot.next_deadline_ms == state.accepted_edge_ms);
        while(state.snapshot.transmitting)
            mf_ardf_core_tick(&state, state.sequence_next_ms);
        mf_ardf_core_tick(&state, state.accepted_edge_ms - MF_ARDF_PTT_LEAD_MS);
        CHECK(state.prepared && state.snapshot.ptt && !state.snapshot.mark);
        mf_ardf_core_tick(&state, state.accepted_edge_ms);
        CHECK(state.snapshot.transmitting);
        mf_ardf_core_leave(&state);
    }

    {
        HardwareMock mock = {.allow = true, .prepare_ok = true};
        MfArdfState state;
        InputEvent ok = {.key = InputKeyOk, .type = InputTypeShort};
        CHECK(enter_state(&state, &mock));
        mf_ardf_core_set_view(&state, MfArdfViewClock, 1U);
        state.snapshot.settings.mode = MfArdfModeCustom;
        memcpy(state.snapshot.settings.custom, "E", 2U);
        state.live_time = (MfArdfClockTime){12U, 1U, 0U};
        CHECK(mf_ardf_core_input(&state, &ok, 1U).handled);
        CHECK(mf_ardf_core_activate_run(&state, 1U).handled);
        CHECK(state.custom_anchor_wall_s == 12U * 3600U + 120U);
        mf_ardf_core_leave(&state);
    }

    {
        HardwareMock mock = {.allow = true, .prepare_ok = true};
        MfArdfState state;
        uint32_t deadline = UINT32_MAX - 100U;
        CHECK(enter_state(&state, &mock));
        state.snapshot.view = MfArdfViewRun;
        state.snapshot.running = true;
        state.snapshot.settings.mode = MfArdfModeCustom;
        state.snapshot.settings.interval_index = 0U;
        memcpy(state.snapshot.settings.custom, "E", 2U);
        state.snapshot.next_deadline_ms = deadline;
        state.snapshot.gpio_owned = true;
        state.deadline_wall_s = 60U;
        CHECK(mf_ardf_core_tick(&state, deadline - MF_ARDF_PTT_LEAD_MS).handled);
        CHECK(state.prepared && state.snapshot.ptt && !state.snapshot.mark);
        CHECK(mf_ardf_core_tick(&state, deadline).handled);
        CHECK(state.snapshot.transmitting);
        while(state.snapshot.transmitting)
            mf_ardf_core_tick(&state, state.sequence_next_ms);
        CHECK(state.snapshot.next_deadline_ms == deadline + 3000U);
        mf_ardf_core_leave(&state);
    }

    printf("test_ardf_core: %u checks passed\n", checks);
    return 0;
}
