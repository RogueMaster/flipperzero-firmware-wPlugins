/*
 * A canvas that prints every call instead of drawing it.
 *
 * The screens are the one part of this game neither build can check
 * against the other by running tests, because a test can only say a
 * layout is in bounds, never that it is the agreed layout. A trace can:
 * dump both builds in this format and diff them.
 */
#pragma once
#include <stdio.h>
#include <string.h>
#include "beepback.h"

static Font tc_font = FontSecondary;
static uint32_t tc_dots; /* runs of dots are art, and collapse in the trace */

static void tc_flush_dots(void) {
    if(tc_dots) printf("[%lu dot calls]\n", (unsigned long)tc_dots);
    tc_dots = 0;
}

static char tc_h(Align a) {
    return a == AlignLeft ? 'L' : (a == AlignRight ? 'R' : 'C');
}
static char tc_v(Align a) {
    return a == AlignTop ? 'T' : (a == AlignBottom ? 'B' : 'M');
}

void canvas_clear(Canvas* c) {
    UNUSED(c);
    tc_flush_dots();
    printf("clear\n");
}
void canvas_set_color(Canvas* c, Color color) {
    UNUSED(c);
    tc_flush_dots();
    printf("color %s\n", color == ColorWhite ? "white" : (color == ColorXOR ? "xor" : "black"));
}
void canvas_set_font(Canvas* c, Font f) {
    UNUSED(c);
    tc_flush_dots();
    tc_font = f;
    printf("font %s\n", f == FontPrimary ? "primary" : (f == FontBigNumbers ? "bignum" : "secondary"));
}
uint16_t canvas_string_width(Canvas* c, const char* s) {
    UNUSED(c);
    /* the same table the bounds checker uses, so traces are reproducible */
    uint16_t w = 0;
    bool big = (tc_font == FontPrimary || tc_font == FontBigNumbers);
    for(; *s; s++) {
        if(tc_font == FontBigNumbers) w = (uint16_t)(w + 12);
        else if(*s == ' ') w = (uint16_t)(w + 3);
        else if(strchr("Iil1.,:;'|!", *s)) w = (uint16_t)(w + (big ? 3 : 2));
        else if(*s >= 'a' && *s <= 'z') w = (uint16_t)(w + (big ? 6 : 4));
        else w = (uint16_t)(w + (big ? 7 : 5));
    }
    return w;
}
void canvas_draw_str(Canvas* c, int32_t x, int32_t y, const char* s) {
    UNUSED(c);
    tc_flush_dots();
    printf("str %ld %ld LB \"%s\"\n", (long)x, (long)y, s);
}
void canvas_draw_str_aligned(Canvas* c, int32_t x, int32_t y, Align h, Align v, const char* s) {
    UNUSED(c);
    tc_flush_dots();
    printf("str %ld %ld %c%c \"%s\"\n", (long)x, (long)y, tc_h(h), tc_v(v), s);
}
void canvas_draw_dot(Canvas* c, int32_t x, int32_t y) {
    UNUSED(c);
    UNUSED(x);
    UNUSED(y);
    tc_dots++;
}
void canvas_draw_box(Canvas* c, int32_t x, int32_t y, size_t w, size_t h) {
    UNUSED(c);
    tc_flush_dots();
    printf("box %ld %ld %lu %lu\n", (long)x, (long)y, (unsigned long)w, (unsigned long)h);
}
void canvas_draw_rbox(Canvas* c, int32_t x, int32_t y, size_t w, size_t h, size_t r) {
    UNUSED(c);
    tc_flush_dots();
    printf("rbox %ld %ld %lu %lu %lu\n", (long)x, (long)y, (unsigned long)w, (unsigned long)h,
           (unsigned long)r);
}
void canvas_draw_frame(Canvas* c, int32_t x, int32_t y, size_t w, size_t h) {
    UNUSED(c);
    tc_flush_dots();
    printf("frame %ld %ld %lu %lu\n", (long)x, (long)y, (unsigned long)w, (unsigned long)h);
}
void canvas_draw_rframe(Canvas* c, int32_t x, int32_t y, size_t w, size_t h, size_t r) {
    UNUSED(c);
    tc_flush_dots();
    printf("rframe %ld %ld %lu %lu %lu\n", (long)x, (long)y, (unsigned long)w, (unsigned long)h,
           (unsigned long)r);
}
void canvas_draw_line(Canvas* c, int32_t a, int32_t b, int32_t x, int32_t y) {
    UNUSED(c);
    tc_flush_dots();
    printf("line %ld %ld %ld %ld\n", (long)a, (long)b, (long)x, (long)y);
}
void canvas_draw_circle(Canvas* c, int32_t x, int32_t y, size_t r) {
    UNUSED(c);
    tc_flush_dots();
    printf("circle %ld %ld %lu\n", (long)x, (long)y, (unsigned long)r);
}
void canvas_draw_disc(Canvas* c, int32_t x, int32_t y, size_t r) {
    UNUSED(c);
    tc_flush_dots();
    printf("disc %ld %ld %lu\n", (long)x, (long)y, (unsigned long)r);
}
