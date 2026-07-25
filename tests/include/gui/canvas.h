#pragma once

#include <stdint.h>

typedef struct Canvas {
    uint32_t dots;
    uint32_t boxes;
} Canvas;

typedef enum {
    FontPrimary,
    FontSecondary,
} Font;

typedef enum {
    AlignLeft,
    AlignCenter,
    AlignRight,
    AlignTop,
    AlignBottom,
} Align;

void canvas_set_font(Canvas* canvas, Font font);
void canvas_draw_str_aligned(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    Align horizontal,
    Align vertical,
    const char* text);
void canvas_draw_dot(Canvas* canvas, int32_t x, int32_t y);
void canvas_draw_box(Canvas* canvas, int32_t x, int32_t y, int32_t width, int32_t height);
void canvas_draw_line(Canvas* canvas, int32_t x1, int32_t y1, int32_t x2, int32_t y2);
