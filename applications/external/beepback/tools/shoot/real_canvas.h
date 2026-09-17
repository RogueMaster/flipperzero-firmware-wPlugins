/*
 * The Flipper's canvas, on a host machine.
 *
 * Not a model of it: this is u8g2 - the same library the firmware draws
 * with - set up with the same two fonts canvas_set_font() picks, driving
 * a 128x64 buffer, and every canvas_* call below does what the same call
 * does in applications/services/gui/canvas.c. The alignment arithmetic
 * is copied from there rather than reasoned about, because that is the
 * part a screenshot would get subtly wrong.
 *
 * So what comes out is what the device draws, pixel for pixel, without a
 * device. The fake canvas in test/ measures; this one renders.
 */
#pragma once
#include <string.h>
#include <u8g2.h>
/* Canvas, Color, Font and Align come from the stub gui header, which is
   diffed against the device's own by tools/check_stubs.py. Declaring
   them again here would be a second opinion about the API. */
#include "beepback.h"

static u8g2_t rc_u8g2;
static uint8_t rc_buf[BB_W / 8 * BB_H];

/* a 128x64 display that exists only as a buffer */
static const u8x8_display_info_t rc_info = {
    .chip_enable_level = 0,
    .chip_disable_level = 1,
    .post_chip_enable_wait_ns = 0,
    .pre_chip_disable_wait_ns = 0,
    .reset_pulse_width_ms = 0,
    .post_reset_wait_ms = 0,
    .sda_setup_time_ns = 0,
    .sck_pulse_width_ns = 0,
    .sck_clock_hz = 4000000UL,
    .spi_mode = 0,
    .i2c_bus_clock_100kHz = 4,
    .data_setup_time_ns = 0,
    .write_pulse_width_ns = 0,
    .tile_width = BB_W / 8,
    .tile_height = BB_H / 8,
    .default_x_offset = 0,
    .flipmode_x_offset = 0,
    .pixel_width = BB_W,
    .pixel_height = BB_H,
};

static uint8_t rc_display_cb(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr) {
    (void)arg_int;
    (void)arg_ptr;
    if(msg == U8X8_MSG_DISPLAY_SETUP_MEMORY) u8x8_d_helper_display_setup_memory(u8x8, &rc_info);
    return 1;
}

static void rc_init(void) {
    u8g2_SetupDisplay(&rc_u8g2, rc_display_cb, u8x8_cad_empty, u8x8_dummy_cb, u8x8_dummy_cb);
    u8g2_SetupBuffer(&rc_u8g2, rc_buf, BB_H / 8, u8g2_ll_hvline_vertical_top_lsb, U8G2_R0);
    u8g2_SetFontMode(&rc_u8g2, 1);
    u8g2_SetFontPosBaseline(&rc_u8g2);
    u8g2_SetDrawColor(&rc_u8g2, 1);
    u8g2_SetFont(&rc_u8g2, u8g2_font_haxrcorp4089_tr);
    u8g2_ClearBuffer(&rc_u8g2);
}

static bool rc_pixel(int32_t x, int32_t y) {
    if(x < 0 || y < 0 || x >= BB_W || y >= BB_H) return false;
    return (rc_buf[(y / 8) * BB_W + x] >> (y % 8)) & 1u;
}

/* ---- the canvas API, as canvas.c implements it ---- */

void canvas_clear(Canvas* c) {
    (void)c;
    u8g2_ClearBuffer(&rc_u8g2);
}
void canvas_set_color(Canvas* c, Color color) {
    (void)c;
    u8g2_SetDrawColor(&rc_u8g2, color);
}
void canvas_set_font(Canvas* c, Font font) {
    (void)c;
    u8g2_SetFontMode(&rc_u8g2, 1);
    if(font == FontPrimary) {
        u8g2_SetFont(&rc_u8g2, u8g2_font_helvB08_tr);
    } else if(font == FontSecondary) {
        u8g2_SetFont(&rc_u8g2, u8g2_font_haxrcorp4089_tr);
    } else if(font == FontKeyboard) {
        u8g2_SetFont(&rc_u8g2, u8g2_font_profont11_mr);
    } else {
        u8g2_SetFont(&rc_u8g2, u8g2_font_profont22_tn);
    }
}
size_t canvas_current_font_height(const Canvas* c) {
    (void)c;
    size_t h = u8g2_GetMaxCharHeight(&rc_u8g2);
    if(rc_u8g2.font == u8g2_font_haxrcorp4089_tr) h += 1;
    return h;
}
uint16_t canvas_string_width(Canvas* c, const char* str) {
    (void)c;
    if(!str) return 0;
    return u8g2_GetUTF8Width(&rc_u8g2, str);
}
void canvas_draw_str(Canvas* c, int32_t x, int32_t y, const char* str) {
    (void)c;
    if(str) u8g2_DrawUTF8(&rc_u8g2, x, y, str);
}
void canvas_draw_str_aligned(Canvas* c, int32_t x, int32_t y, Align h, Align v, const char* str) {
    (void)c;
    if(!str) return;
    if(h == AlignRight) {
        x -= u8g2_GetUTF8Width(&rc_u8g2, str);
    } else if(h == AlignCenter) {
        x -= u8g2_GetUTF8Width(&rc_u8g2, str) / 2;
    }
    if(v == AlignTop) {
        y += u8g2_GetAscent(&rc_u8g2);
    } else if(v == AlignCenter) {
        y += u8g2_GetAscent(&rc_u8g2) / 2;
    }
    u8g2_DrawUTF8(&rc_u8g2, x, y, str);
}
void canvas_draw_dot(Canvas* c, int32_t x, int32_t y) {
    (void)c;
    u8g2_DrawPixel(&rc_u8g2, x, y);
}
void canvas_draw_box(Canvas* c, int32_t x, int32_t y, size_t w, size_t h) {
    (void)c;
    u8g2_DrawBox(&rc_u8g2, x, y, w, h);
}
void canvas_draw_frame(Canvas* c, int32_t x, int32_t y, size_t w, size_t h) {
    (void)c;
    u8g2_DrawFrame(&rc_u8g2, x, y, w, h);
}
void canvas_draw_rbox(Canvas* c, int32_t x, int32_t y, size_t w, size_t h, size_t r) {
    (void)c;
    u8g2_DrawRBox(&rc_u8g2, x, y, w, h, r);
}
void canvas_draw_rframe(Canvas* c, int32_t x, int32_t y, size_t w, size_t h, size_t r) {
    (void)c;
    u8g2_DrawRFrame(&rc_u8g2, x, y, w, h, r);
}
void canvas_draw_line(Canvas* c, int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    (void)c;
    u8g2_DrawLine(&rc_u8g2, x1, y1, x2, y2);
}
void canvas_draw_circle(Canvas* c, int32_t x, int32_t y, size_t r) {
    (void)c;
    u8g2_DrawCircle(&rc_u8g2, x, y, r, U8G2_DRAW_ALL);
}
void canvas_draw_disc(Canvas* c, int32_t x, int32_t y, size_t r) {
    (void)c;
    u8g2_DrawDisc(&rc_u8g2, x, y, r, U8G2_DRAW_ALL);
}
