// SPDX-License-Identifier: GPL-3.0
// Original hand-drawn 5x7 uppercase Cyrillic glyphs, with Ukrainian additions.
// This avoids relying on non-ASCII coverage in the firmware's default fonts.
#include "pc_font.h"
#include <stdint.h>
typedef struct {
    uint16_t code;
    uint8_t rows[7];
} Glyph;
static const Glyph glyphs[] = {
    {0x410, {14, 17, 17, 31, 17, 17, 17}}, {0x411, {31, 16, 16, 30, 17, 17, 30}},
    {0x412, {30, 17, 17, 30, 17, 17, 30}}, {0x413, {31, 16, 16, 16, 16, 16, 16}},
    {0x414, {7, 9, 9, 9, 9, 31, 17}},      {0x415, {31, 16, 16, 30, 16, 16, 31}},
    {0x416, {21, 21, 14, 4, 14, 21, 21}},  {0x417, {14, 17, 1, 6, 1, 17, 14}},
    {0x418, {17, 17, 19, 21, 25, 17, 17}}, {0x419, {10, 4, 17, 19, 21, 25, 17}},
    {0x41A, {17, 18, 20, 24, 20, 18, 17}}, {0x41B, {7, 9, 9, 9, 9, 9, 17}},
    {0x41C, {17, 27, 21, 21, 17, 17, 17}}, {0x41D, {17, 17, 17, 31, 17, 17, 17}},
    {0x41E, {14, 17, 17, 17, 17, 17, 14}}, {0x41F, {31, 17, 17, 17, 17, 17, 17}},
    {0x420, {30, 17, 17, 30, 16, 16, 16}}, {0x421, {14, 17, 16, 16, 16, 17, 14}},
    {0x422, {31, 4, 4, 4, 4, 4, 4}},       {0x423, {17, 17, 17, 15, 1, 17, 14}},
    {0x424, {4, 14, 21, 21, 21, 14, 4}},   {0x425, {17, 17, 10, 4, 10, 17, 17}},
    {0x426, {18, 18, 18, 18, 18, 31, 1}},  {0x427, {17, 17, 17, 15, 1, 1, 1}},
    {0x428, {21, 21, 21, 21, 21, 21, 31}}, {0x429, {21, 21, 21, 21, 21, 31, 1}},
    {0x42C, {16, 16, 16, 30, 17, 17, 30}}, {0x42E, {18, 21, 21, 29, 21, 21, 18}},
    {0x42F, {15, 17, 17, 15, 5, 9, 17}},   {0x404, {14, 17, 16, 28, 16, 17, 14}},
    {0x406, {14, 4, 4, 4, 4, 4, 14}},      {0x407, {10, 0, 14, 4, 4, 4, 14}},
    {0x490, {1, 31, 16, 16, 16, 16, 16}}};
void pc_draw_text(Canvas* c, int x, int baseline, const char* text) {
    const uint8_t* p = (const uint8_t*)text;
    while(*p && x < 127) {
        uint16_t cp = *p++;
        if((cp & 0xE0) == 0xC0 && (*p & 0xC0) == 0x80) {
            cp = ((cp & 31) << 6) | (*p++ & 63);
        } else if(cp >= 128)
            cp = '?';
        if(cp >= 0x430 && cp <= 0x44F) cp -= 32;
        const Glyph* found = NULL;
        for(unsigned i = 0; i < sizeof(glyphs) / sizeof(glyphs[0]); i++)
            if(glyphs[i].code == cp) {
                found = &glyphs[i];
                break;
            }
        if(found) {
            if(x + 5 > 127) break;
            for(int row = 0; row < 7; row++)
                for(int col = 0; col < 5; col++)
                    if(found->rows[row] & (1 << (4 - col)))
                        canvas_draw_dot(c, x + col, baseline - 7 + row);
            x += 6;
        } else {
            if(cp >= 128) cp = '?';
            int width = (int)canvas_glyph_width(c, cp);
            if(x + width > 127) break;
            canvas_draw_glyph(c, x, baseline, cp);
            x += width;
        }
    }
}
