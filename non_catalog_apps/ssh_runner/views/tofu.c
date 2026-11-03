/* Hollow boxes for characters the font lacks (see tofu.h). SPDX-License-Identifier: GPL-3.0-only */
#include "tofu.h"

#include "../core/textproc.h"

static uint8_t adv(Canvas* canvas, char c) {
    /* the box takes a digit's advance: as wide as most glyphs of the font */
    return canvas_glyph_width(canvas, (uint16_t)(c == TEXT_TOFU ? '0' : (unsigned char)c));
}

uint16_t tofu_width(Canvas* canvas, Font font, const char* s) {
    (void)font;
    uint16_t w = 0;
    for(; *s; s++)
        w += adv(canvas, *s);
    return w;
}

void tofu_fit(Canvas* canvas, Font font, FuriString* s, uint16_t max_px) {
    if(tofu_width(canvas, font, furi_string_get_cstr(s)) <= max_px) return;
    uint16_t dots = tofu_width(canvas, font, "...");
    size_t n = furi_string_size(s);
    while(n && tofu_width(canvas, font, furi_string_get_cstr(s)) + dots > max_px)
        furi_string_left(s, --n);
    furi_string_cat(s, "...");
}

void tofu_draw_str(Canvas* canvas, Font font, int32_t x, int32_t y, const char* s) {
    /* cap height: the box stands on the baseline as tall as a capital */
    uint8_t h = canvas_get_font_params(canvas, font)->height;
    char run[64];
    size_t n = 0;
    int32_t run_x = x;
    for(;; s++) {
        bool flush = !*s || *s == TEXT_TOFU || n + 1 == sizeof(run);
        if(flush && n) {
            run[n] = '\0';
            canvas_draw_str(canvas, run_x, y, run);
            n = 0;
        }
        if(!*s) break;
        uint8_t a = adv(canvas, *s);
        if(*s == TEXT_TOFU) {
            canvas_draw_frame(canvas, x, y - h, a - 1, h);
        } else {
            if(!n) run_x = x;
            run[n++] = *s;
        }
        x += a;
    }
}
