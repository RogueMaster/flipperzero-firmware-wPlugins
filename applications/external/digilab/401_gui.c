/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2025 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */
#include <401_gui.h>

uint8_t l401Gui_draw_btn(Canvas* canvas, uint8_t x, uint8_t y, uint8_t w, bool state, char* text) {
    canvas_set_custom_u8g2_font(canvas, CUSTOM_FONT_TINY_REGULAR);
    furi_assert(text);
    size_t font_height = 5;
    uint8_t maxheight = font_height + 3;

    Color bgcolor = ColorBlack;
    Color fgcolor = ColorWhite;

    if(w == 0) // Auto find width
        w = canvas_string_width(canvas, text) + 2;
    // State = true (=selected)
    if(state) {
        canvas_set_color(canvas, bgcolor);
        canvas_draw_rbox(canvas, x - 1, y - 1, w + 1, maxheight, 1);
    }
    // State = false (=unselected)
    else {
        fgcolor = ColorBlack;
        canvas_set_color(canvas, bgcolor);
        canvas_draw_rframe(canvas, x - 1, y - 1, w + 1, maxheight, 1);
    }

    canvas_set_color(canvas, fgcolor);
    canvas_draw_str_aligned(canvas, x + 1, y + 1, AlignLeft, AlignTop, text);
    return w;
}
