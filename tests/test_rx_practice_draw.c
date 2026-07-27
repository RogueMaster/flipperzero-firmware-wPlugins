#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "mf_rx_practice_core.h"
#include "mf_rx_practice_draw.h"

static unsigned checks;
static int32_t last_line_x1;
static int32_t last_line_y1;
static int32_t last_line_x2;
static int32_t last_line_y2;
static unsigned left_hint_boxes;
static char last_text[MF_CALLSIGN_MAX_LEN + 1U];
static const int32_t hint_geometry[4][4] = {
    {124, 33, 1, 1},
    {125, 32, 1, 3},
    {126, 31, 1, 5},
    {127, 30, 1, 7},
};

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
    if(x >= 124 && x <= 127) {
        unsigned index = (unsigned)(x - 124);
        assert(index == left_hint_boxes);
        assert(x == hint_geometry[index][0]);
        assert(y == hint_geometry[index][1]);
        assert(width == hint_geometry[index][2]);
        assert(height == hint_geometry[index][3]);
        left_hint_boxes++;
    }
}

void canvas_draw_line(Canvas* canvas, int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    (void)canvas;
    last_line_x1 = x1;
    last_line_y1 = y1;
    last_line_x2 = x2;
    last_line_y2 = y2;
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

static void reset_draw_capture(void) {
    last_line_x1 = -1;
    last_line_y1 = -1;
    last_line_x2 = -1;
    last_line_y2 = -1;
    left_hint_boxes = 0U;
    last_text[0] = '\0';
}

static void check_phase_chrome(
    MfRxPracticeState* state,
    Canvas* canvas,
    MfRxPracticePhase phase,
    bool button_paddle) {
    reset_draw_capture();
    state->phase = phase;
    state->button_paddle = button_paddle;
    mf_rx_practice_draw(state, canvas);
    if(phase == MfRxPracticePhaseIdle) {
        CHECK(last_line_x1 == -1);
        CHECK(last_line_y1 == -1);
        CHECK(last_line_x2 == -1);
        CHECK(last_line_y2 == -1);
        CHECK(left_hint_boxes == 0U);
    } else {
        CHECK(last_line_x1 == 0);
        CHECK(last_line_y1 == 33);
        CHECK(last_line_x2 == (button_paddle ? 119 : 127));
        CHECK(last_line_y2 == 33);
        CHECK(left_hint_boxes == (button_paddle ? 4U : 0U));
    }
}

int main(void) {
    Canvas canvas = {0};
    MfRxPracticeDrawSnapshot snapshot = {
        .answer_preview = 'E',
    };
    MfRxPracticeState state = {
        .phase = MfRxPracticePhaseAnswer,
        .target_len = 4U,
        .button_paddle = true,
        .draw_snapshot = &snapshot,
    };

    memcpy(state.target, "EIAA", 5U);
    for(MfRxPracticePhase phase = MfRxPracticePhaseIdle;
        phase <= MfRxPracticePhaseFinal;
        phase++) {
        check_phase_chrome(&state, &canvas, phase, true);
        check_phase_chrome(&state, &canvas, phase, false);
    }

    reset_draw_capture();
    state.phase = MfRxPracticePhaseAnswer;
    state.button_paddle = true;
    mf_rx_practice_draw(&state, &canvas);
    CHECK(strcmp(last_text, "E") == 0);

    snapshot.answer_preview = '\0';
    MfRxPracticeResult result = mf_rx_practice_feed_text(&state, "E", 1U, 100U);
    CHECK(result.handled && result.redraw);
    CHECK(snapshot.answer_preview == '\0');
    reset_draw_capture();
    mf_rx_practice_draw(&state, &canvas);
    CHECK(strcmp(last_text, "E") == 0);

    snapshot.answer_preview = 'I';
    reset_draw_capture();
    mf_rx_practice_draw(&state, &canvas);
    CHECK(strcmp(last_text, "EI") == 0);

    state.answer_len = 0U;
    state.answer[0] = '\0';
    for(const char* preview = "EISH5"; *preview != '\0'; preview++) {
        snapshot.answer_preview = *preview;
        reset_draw_capture();
        mf_rx_practice_draw(&state, &canvas);
        CHECK(last_text[0] == *preview && last_text[1] == '\0');
    }

    printf("test_rx_practice_draw: %u checks passed\n", checks);
    return 0;
}
