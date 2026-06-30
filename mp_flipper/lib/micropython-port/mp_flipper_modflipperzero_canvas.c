#include <gui/gui.h>
#include <gui/elements.h>

#include <mp_flipper_modflipperzero.h>
#include <mp_flipper_runtime.h>

#include "mp_flipper_context.h"

static Align text_align_x = AlignLeft;
static Align text_align_y = AlignTop;

inline uint8_t mp_flipper_canvas_width() {
    mp_flipper_context_t* ctx = mp_flipper_context;

    return canvas_width(ctx->canvas);
}

inline uint8_t mp_flipper_canvas_height() {
    mp_flipper_context_t* ctx = mp_flipper_context;

    return canvas_height(ctx->canvas);
}

inline uint8_t mp_flipper_canvas_text_width(const char* text) {
    mp_flipper_context_t* ctx = mp_flipper_context;

    return canvas_string_width(ctx->canvas, text);
}

inline uint8_t mp_flipper_canvas_text_height() {
    mp_flipper_context_t* ctx = mp_flipper_context;

    return canvas_current_font_height(ctx->canvas);
}

inline void mp_flipper_canvas_draw_dot(uint8_t x, uint8_t y) {
    mp_flipper_context_t* ctx = mp_flipper_context;

    canvas_draw_dot(ctx->canvas, x, y);
}

inline void mp_flipper_canvas_draw_box(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t r) {
    mp_flipper_context_t* ctx = mp_flipper_context;

    canvas_draw_rbox(ctx->canvas, x, y, w, h, r);
}

inline void mp_flipper_canvas_draw_frame(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t r) {
    mp_flipper_context_t* ctx = mp_flipper_context;

    canvas_draw_rframe(ctx->canvas, x, y, w, h, r);
}

inline void mp_flipper_canvas_draw_line(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
    mp_flipper_context_t* ctx = mp_flipper_context;

    canvas_draw_line(ctx->canvas, x0, y0, x1, y1);
}

inline void mp_flipper_canvas_draw_circle(uint8_t x, uint8_t y, uint8_t r) {
    mp_flipper_context_t* ctx = mp_flipper_context;

    canvas_draw_circle(ctx->canvas, x, y, r);
}

inline void mp_flipper_canvas_draw_disc(uint8_t x, uint8_t y, uint8_t r) {
    mp_flipper_context_t* ctx = mp_flipper_context;

    canvas_draw_disc(ctx->canvas, x, y, r);
}

inline void mp_flipper_canvas_set_font(uint8_t font) {
    mp_flipper_context_t* ctx = mp_flipper_context;

    canvas_set_font(ctx->canvas, font == MP_FLIPPER_FONT_PRIMARY ? FontPrimary : FontSecondary);
}

inline void mp_flipper_canvas_set_color(uint8_t color) {
    mp_flipper_context_t* ctx = mp_flipper_context;

    canvas_set_color(ctx->canvas, color == MP_FLIPPER_COLOR_BLACK ? ColorBlack : ColorWhite);
}

inline void mp_flipper_canvas_set_text(uint8_t x, uint8_t y, const char* text) {
    mp_flipper_context_t* ctx = mp_flipper_context;

    canvas_draw_str_aligned(ctx->canvas, x, y, text_align_x, text_align_y, text);
}

inline void mp_flipper_canvas_set_text_align(uint8_t x, uint8_t y) {
    Align align_x = x == MP_FLIPPER_ALIGN_BEGIN ? AlignLeft : AlignRight;
    Align align_y = y == MP_FLIPPER_ALIGN_BEGIN ? AlignTop : AlignBottom;

    text_align_x = x == MP_FLIPPER_ALIGN_CENTER ? AlignCenter : align_x;
    text_align_y = y == MP_FLIPPER_ALIGN_CENTER ? AlignCenter : align_y;
}

inline void mp_flipper_canvas_update() {
    mp_flipper_context_t* ctx = mp_flipper_context;

    canvas_commit(ctx->canvas);
}

inline void mp_flipper_canvas_clear() {
    mp_flipper_context_t* ctx = mp_flipper_context;

    canvas_clear(ctx->canvas);
}
