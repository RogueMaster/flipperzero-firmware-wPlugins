/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * How wide a string is on the Flipper screen, measured the way the firmware
 * measures it.
 *
 * The advances are those of the two fonts the GUI draws text in:
 * u8g2_font_helvB08_tr for FontPrimary and u8g2_font_haxrcorp4089_tr for
 * FontSecondary. They were decoded from the glyph headers in firmware 1.4.3's
 * firmware.elf, and agree glyph for glyph with the same fonts in upstream u8g2
 * (tools/font/build/single_font_files/).
 *
 * The sum of advances is what both of the firmware's line breakers compare
 * against their width: widget_element_text_scroll breaks a line once the sum
 * passes width - 4, and text_box once it would pass 120. The rendered width of
 * a string, canvas_string_width(), is the same sum with the last glyph's
 * right side bearing taken off, so the sum is also a safe upper bound on how
 * wide a centred heading draws.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

typedef enum {
    EmrtdTestFontPrimary,
    EmrtdTestFontSecondary,
} EmrtdTestFont;

/* ASCII 32 to 126; both fonts are the _tr variants, which stop there. */
static const uint8_t emrtd_test_font_primary[95] = {
    3,  4, 5, 6, 6, 8, 8, 3,  4, 4, 4, 6, 3, 5,  3, 4, /*  !"#$%&'()*+,-./ */
    6,  6, 6, 6, 6, 6, 6, 6,  6, 6, 3, 3, 5, 6,  5, 6, /* 0123456789:;<=>? */
    11, 8, 7, 8, 7, 6, 6, 8,  7, 3, 6, 7, 6, 10, 8, 8, /* @ABCDEFGHIJKLMNO */
    7,  8, 7, 7, 7, 7, 8, 11, 8, 9, 7, 4, 4, 4,  5, 6, /* PQRSTUVWXYZ[\]^_ */
    3,  6, 6, 5, 6, 6, 4, 6,  6, 3, 3, 6, 3, 9,  6, 6, /* `abcdefghijklmno */
    6,  6, 4, 6, 4, 6, 6, 8,  7, 6, 6, 5, 3, 5,  6, /* pqrstuvwxyz{|}~ */
};

static const uint8_t emrtd_test_font_secondary[95] = {
    2, 2, 4, 6, 6, 8, 7, 2, 4, 4, 6, 6, 3, 6, 2, 8, /*  !"#$%&'()*+,-./ */
    6, 3, 6, 6, 6, 6, 6, 6, 6, 6, 2, 3, 4, 6, 4, 6, /* 0123456789:;<=>? */
    9, 6, 6, 6, 6, 6, 6, 6, 6, 2, 6, 6, 6, 8, 6, 6, /* @ABCDEFGHIJKLMNO */
    6, 6, 6, 6, 6, 6, 6, 8, 6, 6, 6, 4, 8, 4, 6, 6, /* PQRSTUVWXYZ[\]^_ */
    4, 5, 5, 5, 5, 5, 3, 5, 5, 2, 3, 5, 2, 8, 5, 5, /* `abcdefghijklmno */
    5, 5, 4, 4, 4, 5, 5, 8, 5, 5, 5, 4, 2, 4, 7, /* pqrstuvwxyz{|}~ */
};

/**
 * The sum of the advances of the first @p len characters of @p str.
 *
 * A character outside the fonts counts as wider than any screen, since the
 * firmware would draw nothing for it and the text would silently lose it.
 */
static inline unsigned emrtd_test_font_advance(EmrtdTestFont font, const char* str, size_t len) {
    const uint8_t* table = font == EmrtdTestFontPrimary ? emrtd_test_font_primary :
                                                          emrtd_test_font_secondary;
    unsigned width = 0;
    for(size_t i = 0; i < len && str[i] != '\0'; i++) {
        const unsigned char c = (unsigned char)str[i];
        width += (c >= 32 && c <= 126) ? table[c - 32] : 1000;
    }
    return width;
}
