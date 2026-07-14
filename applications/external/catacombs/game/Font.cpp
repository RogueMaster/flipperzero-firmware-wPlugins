#include <stdint.h>
#include "game/Defines.h"
#include "game/Font.h"
#include "game/Platform.h"
#include "game/Generated/SpriteTypes.h"

static inline uint8_t v3(uint8_t m) {
    return m | (m << 1) | (m >> 1);
}

static inline void
    apply4(uint8_t* dst, uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t xorMask) {
    if(xorMask) {
        dst[0] |= a;
        dst[1] |= b;
        dst[2] |= c;
        dst[3] |= d;
    } else {
        dst[0] &= ~a;
        dst[1] &= ~b;
        dst[2] &= ~c;
        dst[3] &= ~d;
    }
}

static inline void apply1(uint8_t* dst, uint8_t m, uint8_t xorMask) {
    if(xorMask)
        *dst |= m;
    else
        *dst &= ~m;
}

void Font::PrintString(const char* str, uint8_t line, uint8_t x, uint8_t colour) {
    uint8_t* p = Platform::GetScreenBuffer() + DISPLAY_WIDTH * line + x;
    uint8_t xorMask = (colour == COLOUR_BLACK) ? 0 : 0xff;

    for(;;) {
        char c = *str++;
        if(!c) break;
        DrawChar(p, c, xorMask);
        p += glyphWidth;
    }
}

void Font::PrintInt(uint16_t val, uint8_t line, uint8_t x, uint8_t colour) {
    uint8_t* p = Platform::GetScreenBuffer() + DISPLAY_WIDTH * line + x;
    uint8_t xorMask = (colour == COLOUR_BLACK) ? 0 : 0xff;

    if(val == 0) {
        DrawChar(p, '0', xorMask);
        return;
    }

    char buf[5];
    int n = 0;

    while(val && n < 5) {
        buf[n++] = (char)('0' + (val % 10));
        val /= 10;
    }

    while(n--) {
        DrawChar(p, buf[n], xorMask);
        p += glyphWidth;
    }
}

void Font::DrawChar(uint8_t* p, char c, uint8_t xorMask) {
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

    apply1(p - 1, v3(i0), outlineMask);
    apply4(p, r0, r1, r2, r3, outlineMask);
    apply1(p + 4, v3(i3), outlineMask);

    apply4(p, i0, i1, i2, i3, xorMask);
}
