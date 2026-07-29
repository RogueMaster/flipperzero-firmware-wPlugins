#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TEST_CANVAS_TEXT_CAPACITY   16U
#define TEST_CANVAS_TEXT_LENGTH     16U
#define TEST_CANVAS_BITMAP_CAPACITY 2U

typedef struct Canvas {
    uint32_t dots;
    uint32_t boxes;
    uint32_t lines;
    uint32_t strings;
    uint32_t white_strings;
    uint8_t current_color;
    bool full_height_divider;
    char text[TEST_CANVAS_TEXT_CAPACITY][TEST_CANVAS_TEXT_LENGTH];
    int32_t text_x[TEST_CANVAS_TEXT_CAPACITY];
    int32_t text_y[TEST_CANVAS_TEXT_CAPACITY];
    uint32_t bitmaps;
    uint32_t bitmap_hash[TEST_CANVAS_BITMAP_CAPACITY];
    int32_t bitmap_x[TEST_CANVAS_BITMAP_CAPACITY];
    int32_t bitmap_y[TEST_CANVAS_BITMAP_CAPACITY];
    size_t bitmap_width[TEST_CANVAS_BITMAP_CAPACITY];
    size_t bitmap_height[TEST_CANVAS_BITMAP_CAPACITY];
} Canvas;

typedef enum {
    FontPrimary,
    FontSecondary,
    FontKeyboard,
    FontBigNumbers,
} Font;

typedef enum {
    ColorWhite,
    ColorBlack,
} Color;

typedef enum {
    CanvasDirectionBottomToTop,
    CanvasDirectionTopToBottom,
} CanvasDirection;

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
void canvas_draw_bitmap(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    size_t width,
    size_t height,
    const uint8_t* compressed_bitmap_data);
void canvas_set_color(Canvas* canvas, Color color);
void canvas_draw_str(Canvas* canvas, int32_t x, int32_t y, const char* text);
uint32_t canvas_string_width(Canvas* canvas, const char* text);
void canvas_draw_frame(Canvas* canvas, int32_t x, int32_t y, int32_t width, int32_t height);
void canvas_draw_rframe(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height,
    int32_t radius);
void canvas_draw_rbox(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height,
    int32_t radius);
void canvas_draw_triangle(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    int32_t base,
    int32_t height,
    CanvasDirection direction);
