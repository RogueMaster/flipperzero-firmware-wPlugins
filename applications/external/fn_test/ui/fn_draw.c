#include "fn_draw.h"

#include "fn_font.h"

#include <string.h>

#define FN_DRAW_LINE_BUF 65

static size_t fn_draw_utf8_len(const char* s) {
    const unsigned char c = (unsigned char)*s;
    if(c < 0x80) return 1;
    if((c & 0xE0) == 0xC0) return 2;
    if((c & 0xF0) == 0xE0) return 3;
    if((c & 0xF8) == 0xF0) return 4;
    return 1;
}

static int32_t fn_draw_baseline(int32_t y_top, uint8_t line_h) {
    return y_top + line_h - ((line_h >= 8) ? 2 : 1);
}

static uint16_t fn_draw_text(
    Canvas* canvas,
    int32_t x,
    int32_t y_top,
    const char* text,
    int32_t max_width,
    uint8_t line_h,
    uint8_t max_lines,
    bool draw) {
    if(!text || !*text) return 0;

    const char* p = text;
    uint8_t line = 0;
    uint16_t used = 0;

    while(*p && line < max_lines) {
        char buf[FN_DRAW_LINE_BUF];
        size_t n = 0;
        size_t last_space = 0;
        bool has_space = false;

        while(*p) {
            const size_t cl = fn_draw_utf8_len(p);
            if(n + cl >= sizeof(buf)) break;
            memcpy(&buf[n], p, cl);
            n += cl;
            buf[n] = '\0';
            if(*p == ' ') {
                last_space = n - 1;
                has_space = true;
            }
            if(canvas_string_width(canvas, buf) > max_width) {
                if(has_space && last_space > 0) {
                    const size_t keep = last_space;
                    p -= (n - keep - 1);
                    buf[keep] = '\0';
                    n = keep;
                } else {
                    p -= cl;
                    buf[n - cl] = '\0';
                    n -= cl;
                }
                break;
            }
            p += cl;
        }

        while(n > 0 && buf[n - 1] == ' ') {
            buf[--n] = '\0';
        }
        while(*p == ' ')
            p++;
        if(n == 0) break;

        if(draw) {
            canvas_draw_str(canvas, x, fn_draw_baseline(y_top + line * line_h, line_h), buf);
        }
        line++;
        used = (uint16_t)(line * line_h);
    }

    return used;
}

void fn_draw_primary(Canvas* canvas) {
    canvas_set_custom_u8g2_font(canvas, fn_font_primary());
}

void fn_draw_small(Canvas* canvas) {
    canvas_set_custom_u8g2_font(canvas, fn_font_small());
}

uint16_t fn_draw_wrapped(
    Canvas* canvas,
    int32_t x,
    int32_t y_top,
    const char* text,
    int32_t max_width,
    uint8_t line_h,
    uint8_t max_lines) {
    return fn_draw_text(canvas, x, y_top, text, max_width, line_h, max_lines, true);
}

uint16_t fn_draw_measure(
    Canvas* canvas,
    const char* text,
    int32_t max_width,
    uint8_t line_h,
    uint8_t max_lines) {
    return fn_draw_text(canvas, 0, 0, text, max_width, line_h, max_lines, false);
}
