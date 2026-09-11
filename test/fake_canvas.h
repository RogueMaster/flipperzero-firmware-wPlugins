/*
 * A canvas that draws nothing and checks everything.
 *
 * The browser build shipped two bugs where an arrow was drawn past the
 * bottom edge and simply vanished, which no amount of looking at the
 * screen finds. So the host build implements the canvas API as a
 * bookkeeper: every call records the rectangle it would have touched,
 * and anything reaching outside 128x64 is counted.
 *
 * Text widths come from a table close to the two Flipper fonts and
 * rounded up rather than down, so a string this says fits really does.
 */
#pragma once
#include <string.h>
#include "beepback.h"

typedef struct {
    uint32_t ops;
    uint32_t discs; /* filled circles, which is what a wheel car is */
    uint32_t rboxes; /* rounded fills, which is what a selected row is */
    uint32_t whites; /* switches to white ink, which is what inverting is */
    uint32_t out_of_bounds;
    uint32_t too_wide;
    char worst[64]; /* the widest string that did not fit */
    int16_t worst_w;
    char text[48][40]; /* everything the frame said, for the content checks */
    uint8_t texts;
    Font font;
    Color color;
} FakeCanvas;

static FakeCanvas fake;

/* Ascent above and descent below the baseline, and the full line box. */
#define FC_ASCENT 7
#define FC_DESCENT 2
#define FC_LINE 9

static uint8_t fc_char_w(char ch, Font font) {
    bool big = (font == FontPrimary || font == FontBigNumbers);
    if(font == FontBigNumbers) return 12;
    if(ch == ' ') return 3;
    if(strchr("Iil1.,:;'|!", ch)) return big ? 3 : 2;
    if(ch >= 'a' && ch <= 'z') return big ? 6 : 4;
    return big ? 7 : 5; /* capitals, digits and the rest */
}

static int16_t fc_str_w(const char* s, Font font) {
    int16_t w = 0;
    for(; *s; s++) w = (int16_t)(w + fc_char_w(*s, font));
    return w;
}

static void fc_rect(int32_t x, int32_t y, int32_t w, int32_t h) {
    fake.ops++;
    if(x < 0 || y < 0 || x + w > BB_W || y + h > BB_H) fake.out_of_bounds++;
}

static void fc_text(int32_t x, int32_t y, const char* s) {
    int16_t w = fc_str_w(s, fake.font);
    if(fake.texts < 48) {
        strncpy(fake.text[fake.texts], s, sizeof(fake.text[0]) - 1);
        fake.text[fake.texts][sizeof(fake.text[0]) - 1] = 0;
        fake.texts++;
    }
    if(w > BB_W) {
        fake.too_wide++;
        if(w > fake.worst_w) {
            fake.worst_w = w;
            strncpy(fake.worst, s, sizeof(fake.worst) - 1);
            fake.worst[sizeof(fake.worst) - 1] = 0;
        }
    }
    fc_rect(x, y, w, FC_LINE);
}

/* did this frame print something containing that? */
static bool fc_saw(const char* needle) {
    for(uint8_t i = 0; i < fake.texts; i++)
        if(strstr(fake.text[i], needle)) return true;
    return false;
}

static void fake_canvas_reset(void) {
    memset(&fake, 0, sizeof(fake));
    fake.font = FontSecondary;
}

/* ---- the Flipper canvas API, as bookkeeping ---- */
void canvas_clear(Canvas* c) {
    UNUSED(c);
    fake.ops++;
}
void canvas_set_color(Canvas* c, Color color) {
    UNUSED(c);
    if(color == ColorWhite) fake.whites++;
    fake.color = color;
}
void canvas_set_font(Canvas* c, Font font) {
    UNUSED(c);
    fake.font = font;
}
uint16_t canvas_string_width(Canvas* c, const char* str) {
    UNUSED(c);
    return (uint16_t)fc_str_w(str, fake.font);
}
void canvas_draw_str(Canvas* c, int32_t x, int32_t y, const char* str) {
    UNUSED(c);
    fc_text(x, y - FC_ASCENT, str);
}
void canvas_draw_str_aligned(Canvas* c, int32_t x, int32_t y, Align h, Align v, const char* str) {
    UNUSED(c);
    int16_t w = fc_str_w(str, fake.font);
    int32_t x0 = x;
    int32_t y0 = y;
    if(h == AlignCenter) x0 = x - w / 2;
    if(h == AlignRight) x0 = x - w;
    if(v == AlignCenter) y0 = y - FC_LINE / 2;
    if(v == AlignBottom) y0 = y - FC_LINE;
    fc_text(x0, y0, str);
}
void canvas_draw_dot(Canvas* c, int32_t x, int32_t y) {
    UNUSED(c);
    fc_rect(x, y, 1, 1);
}
void canvas_draw_box(Canvas* c, int32_t x, int32_t y, size_t w, size_t h) {
    UNUSED(c);
    fc_rect(x, y, (int32_t)w, (int32_t)h);
}
void canvas_draw_rbox(Canvas* c, int32_t x, int32_t y, size_t w, size_t h, size_t r) {
    UNUSED(c);
    UNUSED(r);
    fake.rboxes++;
    fc_rect(x, y, (int32_t)w, (int32_t)h);
}
void canvas_draw_frame(Canvas* c, int32_t x, int32_t y, size_t w, size_t h) {
    UNUSED(c);
    fc_rect(x, y, (int32_t)w, (int32_t)h);
}
void canvas_draw_rframe(Canvas* c, int32_t x, int32_t y, size_t w, size_t h, size_t r) {
    UNUSED(c);
    UNUSED(r);
    fc_rect(x, y, (int32_t)w, (int32_t)h);
}
void canvas_draw_line(Canvas* c, int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    UNUSED(c);
    int32_t x = x1 < x2 ? x1 : x2, y = y1 < y2 ? y1 : y2;
    fc_rect(x, y, (x1 > x2 ? x1 - x2 : x2 - x1) + 1, (y1 > y2 ? y1 - y2 : y2 - y1) + 1);
}
void canvas_draw_circle(Canvas* c, int32_t x, int32_t y, size_t r) {
    UNUSED(c);
    fc_rect(x - (int32_t)r, y - (int32_t)r, (int32_t)r * 2 + 1, (int32_t)r * 2 + 1);
}
void canvas_draw_disc(Canvas* c, int32_t x, int32_t y, size_t r) {
    UNUSED(c);
    fake.discs++;
    fc_rect(x - (int32_t)r, y - (int32_t)r, (int32_t)r * 2 + 1, (int32_t)r * 2 + 1);
}
