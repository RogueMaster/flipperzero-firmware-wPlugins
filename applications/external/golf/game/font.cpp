#include "game.hpp"

#if ARDUGOLF_FX

static constexpr uint8_t SPACE_WIDTH = 2;

static constexpr uint16_t fd(uint8_t a = 0, uint8_t b = 0, uint8_t c = 0) {
    return (uint16_t(a & 0x1f) << 0) | (uint16_t(b & 0x1f) << 5) | (uint16_t(c & 0x1f) << 10);
}

static uint16_t const FONT_DATA[] PROGMEM = {

    fd(0x17), /* ! */
    fd(0x03, 0x00, 0x03), /* " */
    fd(0x0e, 0x0e, 0x0e),
    /* # */ /* used to indicate the item is used */
    fd(), /* $ */
    fd(0x19, 0x04, 0x13), /* % */
    fd(), /* & */
    fd(0x03), /* ' */
    fd(0x0e, 0x11), /* ( */
    fd(0x11, 0x0e), /* ) */
    fd(), /* * */
    fd(0x04, 0x0e, 0x04), /* + */
    fd(0x10, 0x08), /* , */
    fd(0x04, 0x04), /* - */
    fd(0x10), /* . */
    fd(0x18, 0x04, 0x03), /* / */

    fd(0x0e, 0x11, 0x0e), /* 0 */
    fd(0x12, 0x1f, 0x10), /* 1 */
    fd(0x19, 0x15, 0x12), /* 2 */
    fd(0x11, 0x15, 0x0a), /* 3 */
    fd(0x07, 0x04, 0x1f), /* 4 */
    fd(0x17, 0x15, 0x09), /* 5 */
    fd(0x0e, 0x15, 0x08), /* 6 */
    fd(0x01, 0x19, 0x07), /* 7 */
    fd(0x0a, 0x15, 0x0a), /* 8 */
    fd(0x02, 0x15, 0x0e), /* 9 */

    fd(0x0a), /* : */
    fd(0x10, 0x0a), /* ; */
    fd(0x04, 0x04, 0x0a), /* < */
    fd(0x0a, 0x0a, 0x0a), /* = */
    fd(0x0a, 0x04, 0x04), /* > */
    fd(0x01, 0x15, 0x02), /* ? */
    fd(), /* @ */

    fd(0x1e, 0x05, 0x1e), /* A */
    fd(0x1f, 0x15, 0x0a), /* B */
    fd(0x0e, 0x11, 0x11), /* C */
    fd(0x1f, 0x11, 0x0e), /* D */
    fd(0x1f, 0x15, 0x11), /* E */
    fd(0x1f, 0x05, 0x01), /* F */
    fd(0x0e, 0x11, 0x19), /* G */
    fd(0x1f, 0x04, 0x1f), /* H */
    fd(0x11, 0x1f, 0x11), /* I */
    fd(0x11, 0x0f), /* J */
    fd(0x1f, 0x04, 0x1b), /* K */
    fd(0x1f, 0x10, 0x10), /* L */
    fd(0x1f, 0x02, 0x1f), /* M */
    fd(0x1f, 0x01, 0x1e), /* N */
    fd(0x1f, 0x11, 0x1f), /* O */
    fd(0x1f, 0x05, 0x02), /* P */
    fd(0x0e, 0x11, 0x1e), /* Q */
    fd(0x1f, 0x05, 0x1a), /* R */
    fd(0x12, 0x15, 0x09), /* S */
    fd(0x01, 0x1f, 0x01), /* T */
    fd(0x1f, 0x10, 0x1f), /* U */
    fd(0x07, 0x18, 0x07), /* V */
    fd(0x1f, 0x08, 0x1f), /* W */
    fd(0x1b, 0x04, 0x1b), /* X */
    fd(0x03, 0x14, 0x0f), /* Y */
    fd(0x19, 0x15, 0x13), /* Z */

    fd(0x1f, 0x11), /* [ */
    fd(0x03, 0x04, 0x18), /* \ */
    fd(0x11, 0x1f), /* ] */
    fd(0x02, 0x01, 0x02), /* ^ */
    fd(0x10, 0x10, 0x10), /* _ */
    fd(0x01, 0x02), /* ` */

    fd(0x0c, 0x12, 0x1e), /* a */
    fd(0x1f, 0x12, 0x0c), /* b */
    fd(0x0c, 0x12, 0x12), /* c */
    fd(0x0c, 0x12, 0x1f), /* d */
    fd(0x0c, 0x1a, 0x14), /* e */
    fd(0x1e, 0x05), /* f */
    fd(0x16, 0x1e), /* g */
    fd(0x1f, 0x02, 0x1c), /* h */
    fd(0x1d), /* i */
    fd(0x10, 0x0d), /* j */
    fd(0x1f, 0x04, 0x1a), /* k */
    fd(0x1f), /* l */
    fd(0x1e, 0x04, 0x1e), /* m */
    fd(0x1e, 0x02, 0x1c), /* n */
    fd(0x0c, 0x12, 0x0c), /* o */
    fd(0x1e, 0x0a, 0x04), /* p */
    fd(0x04, 0x0a, 0x1e), /* q */
    fd(0x1e, 0x04, 0x02), /* r */
    fd(0x16, 0x1a), /* s */
    fd(0x0f, 0x12), /* t */
    fd(0x0e, 0x10, 0x1e), /* u */
    fd(0x06, 0x18, 0x06), /* v */
    fd(0x1e, 0x08, 0x1e), /* w */
    fd(0x12, 0x0c, 0x12), /* x */
    fd(0x02, 0x14, 0x0e), /* y */
    fd(0x1a, 0x16), /* z */
    fd(0x04, 0x0e, 0x1f), /* { (left arrow) */
    fd(), /* | */
    fd(0x1f, 0x0e, 0x04), /* } (right arrow) */

};

uint8_t draw_char(uint8_t x, uint8_t y, char c) {
    uint16_t p = pgm_read_word(FONT_DATA + c - 33);
    uint8_t d[3], n = 3;
    d[0] = uint8_t(p) & 0x1f;
    d[1] = uint8_t(p >> 5) & 0x1f;
    d[2] = uint8_t(p >> 10) & 0x1f;
    if(d[2] == 0) n = (d[1] == 0 ? 1 : 2);
    for(uint8_t i = 0; i < n; ++i) {
        uint8_t t = d[i];
        for(uint8_t j = y; t; ++j, t >>= 1)
            if(t & 1) inv_pixel(x + i, j);
    }
    return n;
}

static uint8_t draw_text_ex(uint8_t x, uint8_t y, char const* t, bool prog) {
    for(;;) {
        char c = prog ? (char)pgm_read_byte(t) : *t;
        ++t;
        if(c == '\0') return x;
        if(c == ' ') {
            x += SPACE_WIDTH + 1;
            continue;
        }
        x += draw_char(x, y, c) + 1;
    }
    return x;
}

void draw_text(uint8_t x, uint8_t y, char const* p) {
    draw_text_ex(x, y, p, true);
}

void draw_text_nonprog(uint8_t x, uint8_t y, char const* p) {
    draw_text_ex(x, y, p, false);
}

uint8_t char_width(char c) {
    if(c == ' ') return SPACE_WIDTH;
    uint16_t p = pgm_read_word(FONT_DATA + c - 33);
    uint8_t a, b, n = 3;
    a = uint8_t(p >> 5) & 0x1f;
    b = uint8_t(p >> 10) & 0x1f;
    if(b == 0) n = (a == 0 ? 1 : 2);
    return n;
}

static uint8_t text_width_ex(char const* t, bool prog) {
    uint8_t w = 0;
    for(;;) {
        char c = prog ? (char)pgm_read_byte(t) : *t;
        ++t;
        if(c == '\0') break;
        if(c == ' ') {
            w += SPACE_WIDTH + 1;
            continue;
        }
        w += char_width(c) + 1;
    }
    return w == 0 ? 0 : w - 1;
}

uint8_t text_width(char const* p) {
    return text_width_ex(p, true);
}

uint8_t text_width_nonprog(char const* p) {
    return text_width_ex(p, false);
}

#endif
