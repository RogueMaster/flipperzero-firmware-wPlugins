#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef CWMD_HOST_TEST
typedef enum {
    FontPrimary = 0,
    FontSecondary,
    FontKeyboard,
    FontBigNumbers,
} Font;

typedef enum {
    AlignLeft = 0,
    AlignRight,
    AlignTop,
    AlignBottom,
    AlignCenter,
} Align;

typedef enum {
    ColorWhite = 0,
    ColorBlack = 1,
} Color;

typedef struct Canvas Canvas;

void canvas_set_font(Canvas* canvas, Font font);
uint16_t canvas_string_width(Canvas* canvas, const char* str);
void canvas_draw_str(Canvas* canvas, int32_t x, int32_t y, const char* str);
void canvas_draw_str_aligned(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    Align h,
    Align v,
    const char* str);
void canvas_draw_xbm(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    uint8_t width,
    uint8_t height,
    const uint8_t* bitmap);
void canvas_draw_dot(Canvas* canvas, int32_t x, int32_t y);
void canvas_draw_box(Canvas* canvas, int32_t x, int32_t y, uint8_t width, uint8_t height);
void canvas_draw_line(Canvas* canvas, int32_t x1, int32_t y1, int32_t x2, int32_t y2);
void canvas_set_color(Canvas* canvas, Color color);
void elements_button_left(Canvas* canvas, const char* str);
void elements_button_right(Canvas* canvas, const char* str);
#else
#include <gui/canvas.h>
#include <gui/elements.h>
#endif

typedef struct {
    uint8_t id;
    uint8_t width;
    uint8_t height;
    int8_t y_offset;
    uint8_t left_bearing;
    uint8_t right_bearing;
    const uint8_t* xbm;
} CwmdIcon;

typedef enum {
    CwmdAlignJustify = 0,
    CwmdAlignLeft,
    CwmdAlignCenter,
    CwmdAlignRight,
} CwmdAlign;

typedef enum {
    CwmdChromeNone = 0,
    CwmdChromeLeft = 1U << 0,
    CwmdChromeCenter = 1U << 1,
    CwmdChromeRight = 1U << 2,
} CwmdChromeFlags;

typedef struct {
    uint8_t x;
    uint8_t y;
    uint8_t width;
    uint8_t height;
    uint8_t line_height;
    bool scrollbar;
    uint8_t chrome;
    const char* left_label;
    const char* center_label;
    const char* right_label;
    const CwmdIcon* icons;
    uint8_t icon_count;
} CwmdConfig;

typedef struct {
    int16_t scroll_px;
    int16_t target_scroll_px;
    int16_t max_scroll_px;
} CwmdState;

void cwmd_config_default(CwmdConfig* cfg, bool bottom_chrome);
void cwmd_draw(Canvas* canvas, const CwmdConfig* cfg, CwmdState* state, const char* text);
uint16_t cwmd_content_height(Canvas* canvas, const CwmdConfig* cfg, const char* text);
int16_t cwmd_max_scroll_px(Canvas* canvas, const CwmdConfig* cfg, const char* text);
void cwmd_scroll_step(CwmdState* state, int8_t dir, int16_t max_scroll_px, uint8_t step_px);
bool cwmd_scroll_tick(CwmdState* state);

#ifdef CWMD_HOST_TEST
typedef struct {
    uint16_t lines;
    uint16_t atoms;
    uint16_t icons;
    uint16_t bullets;
    uint16_t justified;
    uint16_t centered;
    uint16_t righted;
    uint16_t bold_atoms;
    uint16_t mono_atoms;
    uint16_t sanitized;
    uint16_t unknown_icons;
    int16_t last_extra_gap_px;
    int16_t last_line_width;
} CwmdTestStats;

void cwmd_test_stats(Canvas* canvas, const CwmdConfig* cfg, const char* text, CwmdTestStats* out);
#endif
