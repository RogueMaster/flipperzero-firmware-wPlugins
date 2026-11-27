#include "mf_ardf_draw.h"

#include <stdio.h>

#include <gui/canvas.h>
#include <gui/elements.h>

static void mf_ardf_draw_stock_arrow(Canvas* canvas, int32_t x, int32_t y, bool up) {
    if(up) {
        canvas_draw_box(canvas, x + 2, y, 1, 1);
        canvas_draw_box(canvas, x + 1, y + 1, 3, 1);
        canvas_draw_box(canvas, x, y + 2, 5, 1);
    } else {
        canvas_draw_box(canvas, x, y, 5, 1);
        canvas_draw_box(canvas, x + 1, y + 1, 3, 1);
        canvas_draw_box(canvas, x + 2, y + 2, 1, 1);
    }
}

static void mf_ardf_draw_clock_box(
    const MfArdfState* state,
    Canvas* canvas,
    uint8_t field,
    uint8_t value,
    int32_t x) {
    char text[3];
    bool selected = state->snapshot.clock_state != MfArdfClockConfirm &&
                    state->snapshot.clock_field == field;
    value %= 100U;
    text[0] = (char)('0' + value / 10U);
    text[1] = (char)('0' + value % 10U);
    text[2] = '\0';
    if(selected) {
        canvas_draw_rbox(canvas, x, 24, 28, 20, 1);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, x, 24, 28, 20, 1);
    }
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(canvas, x + 14, 34, AlignCenter, AlignCenter, text);
    canvas_set_color(canvas, ColorBlack);
    if(selected && state->snapshot.clock_state == MfArdfClockEdit) {
        mf_ardf_draw_stock_arrow(canvas, x + 12, 20, true);
        mf_ardf_draw_stock_arrow(canvas, x + 12, 45, false);
    }
}

static void mf_ardf_draw_clock(const MfArdfState* state, Canvas* canvas) {
    MfArdfClockTime time = state->snapshot.clock_state == MfArdfClockEdit ? state->draft_time :
                                                                            state->live_time;
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignCenter, "Confirm or set time");
    mf_ardf_draw_clock_box(state, canvas, MfArdfClockHours, time.hour, 16);
    mf_ardf_draw_clock_box(state, canvas, MfArdfClockMinutes, time.minute, 50);
    mf_ardf_draw_clock_box(state, canvas, MfArdfClockSeconds, time.second, 84);
    canvas_draw_box(canvas, 46, 31, 2, 2);
    canvas_draw_box(canvas, 46, 37, 2, 2);
    canvas_draw_box(canvas, 80, 31, 2, 2);
    canvas_draw_box(canvas, 80, 37, 2, 2);
    canvas_set_font(canvas, FontSecondary);
    elements_button_center(
        canvas,
        state->snapshot.clock_state == MfArdfClockConfirm ? "Start" :
        state->snapshot.clock_state == MfArdfClockSelect  ? "Edit" :
                                                            "Set");
}

static uint32_t mf_ardf_cycle_phase_ms(const MfArdfState* state, uint32_t now_ms) {
    uint32_t cycle_ms = mf_ardf_cycle_seconds((MfArdfMode)state->snapshot.settings.mode) * 1000U;
    uint32_t deadline = state->cycle_deadline_ms != 0U ? state->cycle_deadline_ms :
                                                         state->snapshot.next_deadline_ms;
    uint32_t target_phase = mf_ardf_target_phase(
                                (MfArdfMode)state->snapshot.settings.mode,
                                (MfArdfMessage)state->snapshot.settings.message) *
                            1000U;
    uint32_t boundary = deadline - target_phase;
    int32_t phase_ms = (int32_t)(now_ms - boundary);
    if(phase_ms < 0) phase_ms += cycle_ms;
    return (uint32_t)phase_ms % cycle_ms;
}

static void mf_ardf_draw_marker(Canvas* canvas, uint8_t x) {
    canvas_draw_box(canvas, x - 2, 52, 5, 1);
    canvas_draw_box(canvas, x - 1, 53, 3, 1);
    canvas_draw_box(canvas, x, 54, 1, 1);
}

static void mf_ardf_draw_run(const MfArdfState* state, Canvas* canvas, uint32_t now_ms) {
    char countdown[6];
    char clock[9];
    uint32_t seconds = mf_ardf_countdown_seconds(now_ms, state->snapshot.next_deadline_ms);
    uint32_t minutes = seconds / 60U;
    uint8_t fill = 0U;
    if(minutes > 99U) minutes = 99U;
    countdown[0] = (char)('0' + minutes / 10U);
    countdown[1] = (char)('0' + minutes % 10U);
    countdown[2] = ':';
    countdown[3] = (char)('0' + (seconds % 60U) / 10U);
    countdown[4] = (char)('0' + seconds % 10U);
    countdown[5] = '\0';
    clock[0] = (char)('0' + state->live_time.hour / 10U);
    clock[1] = (char)('0' + state->live_time.hour % 10U);
    clock[2] = ':';
    clock[3] = (char)('0' + state->live_time.minute / 10U);
    clock[4] = (char)('0' + state->live_time.minute % 10U);
    clock[5] = ':';
    clock[6] = (char)('0' + state->live_time.second / 10U);
    clock[7] = (char)('0' + state->live_time.second % 10U);
    clock[8] = '\0';
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(canvas, 64, 29, AlignCenter, AlignCenter, countdown);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 46, AlignCenter, AlignCenter, clock);
    canvas_draw_rframe(canvas, 3, 56, 122, 7, 2);
    if(state->snapshot.settings.mode != MfArdfModeCustom &&
       state->snapshot.settings.message < MfArdfMessageS) {
        uint32_t cycle_ms =
            mf_ardf_cycle_seconds((MfArdfMode)state->snapshot.settings.mode) * 1000U;
        uint32_t phase_ms = mf_ardf_cycle_phase_ms(state, now_ms);
        uint8_t marker = (uint8_t)((phase_ms * 120U) / cycle_ms);
        canvas_draw_box(canvas, 4 + state->snapshot.settings.message * 24U, 57, 24, 5);
        mf_ardf_draw_marker(canvas, (uint8_t)(4U + marker));
    } else {
        fill = mf_ardf_progress_width(
            now_ms, state->snapshot.segment_start_ms, state->snapshot.next_deadline_ms);
        if(fill != 0U) canvas_draw_box(canvas, 4, 57, fill, 5);
    }
}

void mf_ardf_draw(const MfArdfState* state, Canvas* canvas, uint32_t now_ms) {
    if(state == NULL || canvas == NULL || !state->entered) return;
    if(state->snapshot.view == MfArdfViewClock)
        mf_ardf_draw_clock(state, canvas);
    else if(state->snapshot.view == MfArdfViewRun)
        mf_ardf_draw_run(state, canvas, now_ms);
}
