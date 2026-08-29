#include <stdint.h>
#include "game/Defines.h"
#include "game/Font.h"
#include "game/Platform.h"
#include "game/Generated/SpriteTypes.h"

namespace {

constexpr int16_t kPageCount = DISPLAY_HEIGHT / 8;

inline uint8_t v3(uint8_t m) {
    return m | (m << 1) | (m >> 1);
}

// A glyph paints its outline one column either side of its 4 column cell, so a
// character covers columns col-1 .. col+4. This clip is the whole point of the
// function: without it a string printed at x = 0 wrote framebuffer[-1], which
// is the last byte of the heap block header of the allocation the framebuffer
// lives in. That corrupts xBlockSize, and the damage only surfaces when the
// block is freed on app exit - taking the allocator, and the firmware, with it.
inline void blend(uint8_t* base, int16_t line, int16_t col, uint8_t m, uint8_t xorMask) {
    if(col < 0 || col >= DISPLAY_WIDTH) return;

    uint8_t* dst = base + (int32_t)line * DISPLAY_WIDTH + col;
    if(xorMask)
        *dst |= m;
    else
        *dst &= (uint8_t)~m;
}

} // namespace

void Font::DrawChar(uint8_t* base, int16_t line, int16_t col, char c, uint8_t xorMask) {
    uint8_t uc = (uint8_t)c;
    if(uc < firstGlyphIndex) return;

    const uint8_t* f = fontPageData + glyphWidth * (uc - firstGlyphIndex);

    uint8_t i0 = ~f[0];
    uint8_t i1 = ~f[1];
    uint8_t i2 = ~f[2];
    uint8_t i3 = ~f[3];

    uint8_t t0 = v3(i0 | i1);
    uint8_t t1 = v3(i0 | i1 | i2);
    uint8_t t2 = v3(i1 | i2 | i3);
    uint8_t t3 = v3(i2 | i3);

    uint8_t r0 = t0 & ~i0;
    uint8_t r1 = t1 & ~i1;
    uint8_t r2 = t2 & ~i2;
    uint8_t r3 = t3 & ~i3;

    uint8_t outlineMask = xorMask ^ 0xff;

    blend(base, line, (int16_t)(col - 1), v3(i0), outlineMask);
    blend(base, line, (int16_t)(col + 0), r0, outlineMask);
    blend(base, line, (int16_t)(col + 1), r1, outlineMask);
    blend(base, line, (int16_t)(col + 2), r2, outlineMask);
    blend(base, line, (int16_t)(col + 3), r3, outlineMask);
    blend(base, line, (int16_t)(col + 4), v3(i3), outlineMask);

    blend(base, line, (int16_t)(col + 0), i0, xorMask);
    blend(base, line, (int16_t)(col + 1), i1, xorMask);
    blend(base, line, (int16_t)(col + 2), i2, xorMask);
    blend(base, line, (int16_t)(col + 3), i3, xorMask);
}

void Font::PrintString(const char* str, uint8_t line, uint8_t x, uint8_t colour) {
    uint8_t* base = Platform::GetScreenBuffer();
    if(!base || !str || line >= kPageCount) return;

    uint8_t xorMask = (colour == COLOUR_BLACK) ? 0 : 0xff;
    int16_t col = (int16_t)x;

    for(;;) {
        char c = *str++;
        if(!c) break;
        if(col >= DISPLAY_WIDTH) break;
        DrawChar(base, (int16_t)line, col, c, xorMask);
        col += glyphWidth;
    }
}

void Font::PrintInt(uint16_t val, uint8_t line, uint8_t x, uint8_t colour) {
    uint8_t* base = Platform::GetScreenBuffer();
    if(!base || line >= kPageCount) return;

    uint8_t xorMask = (colour == COLOUR_BLACK) ? 0 : 0xff;
    int16_t col = (int16_t)x;

    if(val == 0) {
        DrawChar(base, (int16_t)line, col, '0', xorMask);
        return;
    }

    char buf[5];
    int n = 0;

    while(val && n < 5) {
        buf[n++] = (char)('0' + (val % 10));
        val /= 10;
    }

    while(n--) {
        if(col >= DISPLAY_WIDTH) break;
        DrawChar(base, (int16_t)line, col, buf[n], xorMask);
        col += glyphWidth;
    }
}
