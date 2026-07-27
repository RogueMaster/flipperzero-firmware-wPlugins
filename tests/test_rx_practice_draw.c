#include <assert.h>
#include <stdio.h>

#include "mf_rx_practice_core.h"
#include "mf_rx_practice_draw.h"

static unsigned checks;
static int32_t last_line_x2;
static bool show_left_hint;

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
    (void)text;
    (void)target;
    (void)length;
    (void)y;
    (void)mark_errors;
}

static bool left_exit_hint(void* context) {
    CHECK(context == &show_left_hint);
    return show_left_hint;
}

int main(void) {
    Canvas canvas = {0};
    MfRxPracticeState state = {
        .phase = MfRxPracticePhaseAnswer,
        .target_len = 4U,
        .draw_services = {.context = &show_left_hint, .left_exit_hint = left_exit_hint},
    };

    show_left_hint = true;
    mf_rx_practice_draw(&state, &canvas);
    CHECK(last_line_x2 == 119);
    state.phase = MfRxPracticePhaseResult;
    mf_rx_practice_draw(&state, &canvas);
    CHECK(last_line_x2 == 127);

    printf("test_rx_practice_draw: %u checks passed\n", checks);
    return 0;
}
