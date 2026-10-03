#include <assert.h>
#include <stdio.h>

#include "mf_big_callsign_font.h"

static unsigned boxes;
static unsigned color_changes;

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
    assert(width > 0);
    assert(height > 0);
    boxes++;
}

void canvas_draw_line(Canvas* canvas, int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    (void)canvas;
    (void)x1;
    (void)y1;
    (void)x2;
    (void)y2;
}

void canvas_set_color(Canvas* canvas, Color color) {
    (void)canvas;
    (void)color;
    color_changes++;
}

void canvas_draw_str(Canvas* canvas, int32_t x, int32_t y, const char* text) {
    (void)canvas;
    (void)x;
    (void)y;
    (void)text;
}

uint32_t canvas_string_width(Canvas* canvas, const char* text) {
    (void)canvas;
    (void)text;
    return 0U;
}

void canvas_draw_frame(Canvas* canvas, int32_t x, int32_t y, int32_t width, int32_t height) {
    (void)canvas;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
}

void canvas_draw_rframe(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height,
    int32_t radius) {
    (void)canvas;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    (void)radius;
}

void canvas_draw_rbox(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height,
    int32_t radius) {
    (void)canvas;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    (void)radius;
}

void canvas_draw_triangle(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    int32_t base,
    int32_t height,
    CanvasDirection direction) {
    (void)canvas;
    (void)x;
    (void)y;
    (void)base;
    (void)height;
    (void)direction;
}

static unsigned draw_count(Canvas* canvas, char ch) {
    boxes = 0U;
    color_changes = 0U;
    mf_big_callsign_draw_char(canvas, 4, 5, ch, false);
    assert(color_changes == 0U);
    return boxes;
}

int main(void) {
    Canvas canvas = {0};

    assert(draw_count(&canvas, '.') > 0U);
    assert(draw_count(&canvas, ',') > 0U);
    assert(draw_count(&canvas, '/') > 0U);
    assert(draw_count(&canvas, '?') > 0U);
    assert(draw_count(&canvas, '=') == 10U);
    assert(draw_count(&canvas, '#') == 0U);

    boxes = 0U;
    color_changes = 0U;
    mf_big_callsign_draw_char(&canvas, 4, 5, '=', true);
    assert(boxes == 11U);
    assert(color_changes == 2U);

    puts("test_big_callsign_font: passed");
    return 0;
}
