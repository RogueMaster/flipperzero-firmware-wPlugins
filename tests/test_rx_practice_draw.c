#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "mf_rx_practice_core.h"
#include "mf_rx_practice_draw.h"

static unsigned checks;
static int32_t last_line_x2;
static char last_text[MF_CALLSIGN_MAX_LEN + 1U];

#define CHECK(value) \
    do { \
        assert(value); \
        checks++; \
    } while(0)

void canvas_set_font(Canvas* canvas, Font font) {
    (void)canvas;
    (void)font;
}

void canvas_draw_str_aligned(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    Align horizontal,
    Align vertical,
    const char* text) {
    (void)canvas;
    (void)x;
    (void)y;
    (void)horizontal;
    (void)vertical;
    (void)text;
}

void canvas_draw_dot(Canvas* canvas, int32_t x, int32_t y) {
    (void)canvas;
    (void)x;
    (void)y;
}

void canvas_draw_box(Canvas* canvas, int32_t x, int32_t y, int32_t width, int32_t height) {
    (void)canvas;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
}

void canvas_draw_line(Canvas* canvas, int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    (void)canvas;
    (void)x1;
    (void)y1;
    (void)y2;
    last_line_x2 = x2;
}

void mf_big_callsign_draw_text(
    Canvas* canvas,
    const char* text,
    const char* target,
    uint8_t length,
    int32_t y,
    bool mark_errors) {
    (void)canvas;
    (void)target;
    (void)length;
    (void)y;
    (void)mark_errors;
    snprintf(last_text, sizeof(last_text), "%s", text);
}

int main(void) {
    Canvas canvas = {0};
    MfRxPracticeState state = {
        .phase = MfRxPracticePhaseAnswer,
        .target_len = 4U,
        .button_paddle = true,
        .answer_preview = 'E',
    };

    mf_rx_practice_draw(&state, &canvas);
    CHECK(last_line_x2 == 119);
    CHECK(strcmp(last_text, "E") == 0);
    state.button_paddle = false;
    mf_rx_practice_draw(&state, &canvas);
    CHECK(last_line_x2 == 127);
    state.phase = MfRxPracticePhaseResult;
    mf_rx_practice_draw(&state, &canvas);
    CHECK(last_line_x2 == 127);

    printf("test_rx_practice_draw: %u checks passed\n", checks);
    return 0;
}
