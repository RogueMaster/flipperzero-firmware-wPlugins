#include "mf_rx_practice_draw.h"

#include <stdio.h>

static void mf_draw_slots(
    Canvas* canvas,
    const char* text,
    const char* target,
    uint8_t length,
    uint8_t top,
    bool reveal,
    bool mark_errors) {
    const uint8_t width = (uint8_t)(length * 15U + (length - 1U) * 3U);
    const uint8_t left = (uint8_t)((128U - width) / 2U);
    for(uint8_t i = 0U; i < length; i++) {
        char slot[2] = {reveal && text[i] != '\0' ? text[i] : '_', '\0'};
        uint8_t x = (uint8_t)(left + i * 18U);
        bool mismatch = mark_errors && (text[i] == '\0' || text[i] != target[i]);
        if(mismatch) {
            canvas_draw_box(canvas, x, top, 15U, 13U);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_draw_frame(canvas, x, top, 15U, 13U);
        }
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, x + 7U, top + 11U, AlignCenter, AlignBottom, slot);
        if(mismatch) canvas_set_color(canvas, ColorBlack);
    }
}

void mf_rx_practice_draw(const MfRxPracticeState* state, Canvas* canvas) {
    char score[24];
    if(state == NULL || canvas == NULL) return;
    canvas_set_font(canvas, FontSecondary);
    if(state->phase == MfRxPracticePhaseIdle) {
        canvas_draw_str_aligned(canvas, 64, 12, AlignCenter, AlignBottom, "Callsigns");
        canvas_draw_str_aligned(canvas, 64, 33, AlignCenter, AlignBottom, "Press OK to start");
        if(state->physical_key_can_start)
            canvas_draw_str_aligned(canvas, 64, 49, AlignCenter, AlignBottom, "Press your key to start");
        return;
    }
    if(state->phase == MfRxPracticePhaseFinal) {
        unsigned pct = state->session_total == 0U ? 0U :
                       (unsigned)(((uint32_t)100U * state->session_passed +
                                   state->session_total / 2U) /
                                  state->session_total);
        canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignBottom, "Final score");
        canvas_draw_str_aligned(
            canvas,
            64,
            23,
            AlignCenter,
            AlignBottom,
            state->internal_error ? "Practice error" : "Callsigns");
        snprintf(score, sizeof(score), "%u/%u  %u%%", (unsigned)state->session_passed,
                 (unsigned)state->session_total, pct);
        canvas_draw_str_aligned(canvas, 64, 48, AlignCenter, AlignBottom, score);
        canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "Back / OK");
        return;
    }
    canvas_draw_str_aligned(canvas, 64, 9, AlignCenter, AlignBottom,
                            state->phase == MfRxPracticePhaseResult ?
                                (state->last_passed ? "PASS" : "FAIL") :
                                "Callsigns");
    mf_draw_slots(
        canvas,
        state->target,
        state->target,
        state->target_len,
        13U,
        state->phase == MfRxPracticePhaseResult,
        false);
    mf_draw_slots(
        canvas,
        state->answer,
        state->target,
        state->target_len,
        30U,
        true,
        state->phase == MfRxPracticePhaseResult);
    canvas_draw_line(canvas, 4U, 48U, 123U, 48U);
    unsigned pct = state->session_total == 0U ? 0U :
                   (unsigned)(((uint32_t)100U * state->session_passed +
                               state->session_total / 2U) /
                              state->session_total);
    snprintf(
        score,
        sizeof(score),
        "%u/%u %u%%",
        (unsigned)state->session_passed,
        (unsigned)state->session_total,
        pct);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 121U, 61U, AlignRight, AlignBottom, score);
    if(state->phase == MfRxPracticePhaseResult) {
        snprintf(score, sizeof(score), "%u", (unsigned)state->countdown_draw_s);
        canvas_draw_str_aligned(canvas, 7U, 61U, AlignLeft, AlignBottom, score);
    }
}
