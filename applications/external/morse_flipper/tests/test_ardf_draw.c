#include <stdio.h>
#include <string.h>

#include <gui/canvas.h>
#include "plugins/ardf/mf_ardf_draw.h"

static unsigned checks;
#define CHECK(x)                                                           \
    do {                                                                   \
        checks++;                                                          \
        if(!(x)) {                                                         \
            fprintf(stderr, "failed %s:%d: %s\n", __FILE__, __LINE__, #x); \
            return 1;                                                      \
        }                                                                  \
    } while(0)

void canvas_set_font(Canvas* c, Font f) {
    c->current_font = f;
}
void canvas_set_color(Canvas* c, Color color) {
    c->current_color = color;
}
void canvas_draw_str_aligned(Canvas* c, int32_t x, int32_t y, Align h, Align v, const char* s) {
    (void)h;
    (void)v;
    size_t i = c->strings++;
    strncpy(c->text[i], s, TEST_CANVAS_TEXT_LENGTH - 1);
    c->text_x[i] = x;
    c->text_y[i] = y;
    c->text_font[i] = c->current_font;
    if(c->current_color == ColorWhite) c->white_strings++;
}
void canvas_draw_str(Canvas* c, int32_t x, int32_t y, const char* s) {
    canvas_draw_str_aligned(c, x, y, AlignLeft, AlignTop, s);
}
uint32_t canvas_string_width(Canvas* c, const char* s) {
    (void)c;
    return strlen(s) * 6U;
}
static void box(Canvas* c, int32_t x, int32_t y, int32_t w, int32_t h) {
    size_t i = c->boxes++;
    c->box_x[i] = x;
    c->box_y[i] = y;
    c->box_width[i] = w;
    c->box_height[i] = h;
    c->box_color[i] = c->current_color;
}
void canvas_draw_box(Canvas* c, int32_t x, int32_t y, int32_t w, int32_t h) {
    box(c, x, y, w, h);
    c->box_kind[c->boxes - 1U] = 0U;
}
void canvas_draw_frame(Canvas* c, int32_t x, int32_t y, int32_t w, int32_t h) {
    box(c, x, y, w, h);
    c->box_kind[c->boxes - 1U] = 1U;
}
void canvas_draw_rframe(Canvas* c, int32_t x, int32_t y, int32_t w, int32_t h, int32_t r) {
    box(c, x, y, w, h);
    c->box_kind[c->boxes - 1U] = 2U;
    c->box_radius[c->boxes - 1U] = r;
}
void canvas_draw_rbox(Canvas* c, int32_t x, int32_t y, int32_t w, int32_t h, int32_t r) {
    box(c, x, y, w, h);
    c->box_kind[c->boxes - 1U] = 3U;
    c->box_radius[c->boxes - 1U] = r;
}
void canvas_draw_dot(Canvas* c, int32_t x, int32_t y) {
    (void)x;
    (void)y;
    c->dots++;
}
void canvas_draw_line(Canvas* c, int32_t a, int32_t b, int32_t d, int32_t e) {
    (void)a;
    (void)b;
    (void)d;
    (void)e;
    c->lines++;
}
void canvas_draw_bitmap(Canvas* c, int32_t x, int32_t y, size_t w, size_t h, const uint8_t* d) {
    (void)c;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)d;
}
void canvas_draw_triangle(Canvas* c, int32_t x, int32_t y, int32_t b, int32_t h, CanvasDirection d) {
    (void)c;
    (void)x;
    (void)y;
    (void)b;
    (void)h;
    (void)d;
}

int main(void) {
    MfArdfState state = {0};
    Canvas canvas = {.current_color = ColorBlack};
    state.entered = true;
    state.snapshot.view = MfArdfViewClock;
    state.live_time = (MfArdfClockTime){12, 34, 56};
    mf_ardf_draw(&state, &canvas, 0U);
    CHECK(canvas.strings == 5U);
    CHECK(strcmp(canvas.text[0], "Confirm or set time") == 0);
    CHECK(canvas.box_x[0] == 16 && canvas.box_y[0] == 24 && canvas.box_width[0] == 28);
    CHECK(canvas.box_x[1] == 50 && canvas.box_x[2] == 84);
    CHECK(canvas.box_kind[0] == 2U && canvas.box_radius[0] == 1);
    CHECK(canvas.box_kind[1] == 2U && canvas.box_radius[1] == 1);
    CHECK(canvas.box_kind[2] == 2U && canvas.box_radius[2] == 1);
    CHECK(canvas.text_font[1] == FontBigNumbers && canvas.text_font[3] == FontBigNumbers);
    CHECK(strcmp(canvas.text[4], "Start") == 0 && canvas.text_font[4] == FontSecondary);
    CHECK(canvas.white_strings == 0U);
    CHECK(canvas.boxes == 7U);
    CHECK(canvas.center_buttons == 1U && strcmp(canvas.center_button_text, "Start") == 0);
    CHECK(canvas.center_button_font == FontSecondary);
    memset(&canvas, 0, sizeof(canvas));
    canvas.current_color = ColorBlack;
    state.snapshot.clock_state = MfArdfClockSelect;
    state.snapshot.clock_field = MfArdfClockSeconds;
    mf_ardf_draw(&state, &canvas, 0U);
    CHECK(canvas.box_kind[0] == 2U && canvas.box_kind[1] == 2U && canvas.box_kind[2] == 3U);
    CHECK(canvas.box_radius[2] == 1 && canvas.white_strings == 1U);
    CHECK(canvas.center_buttons == 1U && strcmp(canvas.center_button_text, "Edit") == 0);
    CHECK(strcmp(canvas.text[4], "Edit") == 0 && canvas.text_font[4] == FontSecondary);
    memset(&canvas, 0, sizeof(canvas));
    canvas.current_color = ColorBlack;
    state.snapshot.clock_state = MfArdfClockConfirm;
    mf_ardf_draw(&state, &canvas, 0U);
    CHECK(canvas.box_kind[0] == 2U && canvas.box_kind[1] == 2U && canvas.box_kind[2] == 2U);
    CHECK(canvas.white_strings == 0U);
    CHECK(canvas.center_buttons == 1U && strcmp(canvas.center_button_text, "Start") == 0);
    CHECK(strcmp(canvas.text[4], "Start") == 0 && canvas.text_font[4] == FontSecondary);
    memset(&canvas, 0, sizeof(canvas));
    canvas.current_color = ColorBlack;
    state.snapshot.clock_state = MfArdfClockEdit;
    state.snapshot.clock_field = MfArdfClockMinutes;
    state.draft_time = state.live_time;
    mf_ardf_draw(&state, &canvas, 0U);
    CHECK(canvas.boxes == 13U);
    CHECK(canvas.box_x[1] == 50 && canvas.box_y[1] == 24);
    CHECK(canvas.box_kind[0] == 2U && canvas.box_kind[1] == 3U && canvas.box_radius[1] == 1);
    CHECK(canvas.box_x[2] == 64 && canvas.box_y[2] == 20 && canvas.box_width[2] == 1);
    CHECK(canvas.white_strings == 1U);
    CHECK(canvas.center_buttons == 1U && strcmp(canvas.center_button_text, "Set") == 0);
    CHECK(strcmp(canvas.text[4], "Set") == 0 && canvas.text_font[4] == FontSecondary);
    memset(&canvas, 0, sizeof(canvas));
    canvas.current_color = ColorBlack;
    state.snapshot.view = MfArdfViewRun;
    state.snapshot.settings.mode = MfArdfModeStandard;
    state.snapshot.settings.message = MfArdfMessage3;
    state.snapshot.next_deadline_ms = 300000U;
    state.live_time = (MfArdfClockTime){12U, 34U, 56U};
    mf_ardf_draw(&state, &canvas, 0U);
    CHECK(canvas.strings == 2U && strcmp(canvas.text[0], "05:00") == 0);
    CHECK(strcmp(canvas.text[1], "12:34:56") == 0);
    CHECK(canvas.text_x[1] == 64 && canvas.text_y[1] == 46);
    CHECK(canvas.text_font[1] == FontSecondary);
    CHECK(canvas.boxes == 5U);
    CHECK(canvas.box_x[1] == 52 && canvas.box_y[1] == 57 && canvas.box_width[1] == 24);
    memset(&canvas, 0, sizeof(canvas));
    canvas.current_color = ColorBlack;
    state.snapshot.next_deadline_ms = 420000U;
    state.live_time = (MfArdfClockTime){23U, 59U, 59U};
    mf_ardf_draw(&state, &canvas, 120000U);
    CHECK(strcmp(canvas.text[1], "23:59:59") == 0);
    CHECK(canvas.box_x[2] == 50 && canvas.box_y[2] == 52 && canvas.box_width[2] == 5);
    puts("test_ardf_draw: passed");
    return 0;
}
