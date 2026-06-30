#include <furi.h>
#include <furi_hal.h>
#include "../helpers/time.h"
#include "flipp_pomodoro.h"
#include "flipp_pomodoro_settings.h"

PomodoroStage stages_sequence[] = {
    FlippPomodoroStageFocus,
    FlippPomodoroStageRest,

    FlippPomodoroStageFocus,
    FlippPomodoroStageRest,

    FlippPomodoroStageFocus,
    FlippPomodoroStageRest,

    FlippPomodoroStageFocus,
    FlippPomodoroStageLongBreak,
};

char *current_stage_label[] = {
    [FlippPomodoroStageFocus] = "Focusing...",
    [FlippPomodoroStageRest] = "Short Break...",
    [FlippPomodoroStageLongBreak] = "Long Break...",
};

char *next_stage_label[] = {
    [FlippPomodoroStageFocus] = "Focus",
    [FlippPomodoroStageRest] = "Short Break",
    [FlippPomodoroStageLongBreak] = "Long Break",
};

static uint8_t s_focus_min = 25;
static uint8_t s_short_break_min = 5;
static uint8_t s_long_break_min = 30;

static void flipp_pomodoro__load_settings_or_default(void) {
    FlippPomodoroSettings s;
    flipp_pomodoro_settings_load(&s);
    s_focus_min = s.focus_minutes;
    s_short_break_min = s.short_break_minutes;
    s_long_break_min = s.long_break_minutes;
}

void flipp_pomodoro__apply_settings(const FlippPomodoroSettings* s) {
    if(!s) return;
    s_focus_min = s->focus_minutes;
    s_short_break_min = s->short_break_minutes;
    s_long_break_min = s->long_break_minutes;
}

PomodoroStage flipp_pomodoro__stage_by_index(int index) {
    const int one_loop_size = (int)(sizeof(stages_sequence) / sizeof(stages_sequence[0]));
    return stages_sequence[index % one_loop_size];
}

void flipp_pomodoro__toggle_stage(FlippPomodoroState *state)
{
    furi_assert(state);
    state->current_stage_index = state->current_stage_index + 1;
    state->started_at_timestamp = time_now();
};

PomodoroStage flipp_pomodoro__get_stage(FlippPomodoroState *state)
{
    furi_assert(state);
    return flipp_pomodoro__stage_by_index(state->current_stage_index);
};

char *flipp_pomodoro__current_stage_label(FlippPomodoroState *state)
{
    furi_assert(state);
    return current_stage_label[flipp_pomodoro__get_stage(state)];
};

char *flipp_pomodoro__next_stage_label(FlippPomodoroState *state)
{
    furi_assert(state);
    return next_stage_label[flipp_pomodoro__stage_by_index(state->current_stage_index + 1)];
};

void flipp_pomodoro__destroy(FlippPomodoroState *state)
{
    furi_assert(state);
    free(state);
};

uint32_t flipp_pomodoro__current_stage_total_duration(FlippPomodoroState *state)
{
    const int32_t stage_duration_seconds_map[] = {
        [FlippPomodoroStageFocus] = s_focus_min * TIME_SECONDS_IN_MINUTE,
        [FlippPomodoroStageRest] = s_short_break_min * TIME_SECONDS_IN_MINUTE,
        [FlippPomodoroStageLongBreak] = s_long_break_min * TIME_SECONDS_IN_MINUTE,
    };

    return stage_duration_seconds_map[flipp_pomodoro__get_stage(state)];
};

uint32_t flipp_pomodoro__stage_expires_timestamp(FlippPomodoroState *state)
{
    return state->started_at_timestamp + flipp_pomodoro__current_stage_total_duration(state);
};

TimeDifference flipp_pomodoro__stage_remaining_duration(FlippPomodoroState *state)
{
    const uint32_t stage_ends_at = flipp_pomodoro__stage_expires_timestamp(state);
    return time_difference_seconds(time_now(), stage_ends_at);
};

bool flipp_pomodoro__is_stage_expired(FlippPomodoroState *state)
{
    const uint32_t expired_by = flipp_pomodoro__stage_expires_timestamp(state);
    // precise response (there was an unclear bug with "eating" a second)
    return time_now() >= expired_by;
};

FlippPomodoroState *flipp_pomodoro__new()
{
    // ensure durations reflect settings file (or defaults)
    flipp_pomodoro__load_settings_or_default();

    FlippPomodoroState *state = malloc(sizeof(FlippPomodoroState));
    const uint32_t now = time_now();
    state->started_at_timestamp = now;
    state->current_stage_index = 0;
    return state;
};

const char* flipp_pomodoro__settings_button_label() {
    return "Settings";
}
