#pragma once

#include <stdint.h>
#include <furi.h>
#include <gui/canvas.h>

#include <gui/elements.h>

#include "../font_provider/c_font.h"
#include "../utils/unicode_utils.h"

void elements_draw_str_utf8(Canvas* canvas, uint8_t x, uint8_t y, const char* str);
void elements_draw_str_aligned_utf16(
    Canvas* canvas,
    uint8_t x,
    uint8_t y,
    Align horizontal,
    Align vertical,
    const Unicode* uni);
void elements_draw_str_aligned_utf8(
    Canvas* canvas,
    uint8_t x,
    uint8_t y,
    Align horizontal,
    Align vertical,
    const char* str);

void elements_multiline_text_aligned_utf8(
    Canvas* canvas,
    uint8_t x,
    uint8_t y,
    Align horizontal,
    Align vertical,
    const char* text);

void elements_button_left_utf8(Canvas* canvas, const char* str);
void elements_button_right_utf8(Canvas* canvas, const char* str);
void elements_button_center_utf8(Canvas* canvas, const char* str);
