#pragma once
#include "Platform.h"
#include "Config.h"

// 128x64 1-bit frame buffer in the display's own page layout: eight
// pages of 128 bytes, each byte a column of eight pixels with the top
// pixel in bit 0. That is the layout of the Flipper's canvas buffer, so a
// finished frame reaches the screen with one 1 KB memcpy. A set bit is a
// drawn (dark) pixel.

namespace Fb {

constexpr int W = Config::ScreenW;
constexpr int H = Config::ScreenH;
constexpr int Pages = H / 8;

inline uint8_t buf[W * Pages];

struct Clip {
    int x0, y0, x1, y1;
};
inline Clip clip = {0, 0, W - 1, H - 1};

inline void setClip(int x0, int y0, int x1, int y1) {
    clip = {x0, y0, x1, y1};
}
inline void fullClip() {
    clip = {0, 0, W - 1, H - 1};
}
inline void clear() {
    memset(buf, 0, sizeof(buf));
}

inline int iround(float v) {
    return (int)floorf(v + 0.5f);
}

inline void px(int x, int y, bool on = true) {
    if(x < clip.x0 || x > clip.x1 || y < clip.y0 || y > clip.y1) return;
    uint8_t& b = buf[(y >> 3) * W + x];
    uint8_t m = (uint8_t)(1u << (y & 7));
    if(on)
        b |= m;
    else
        b &= (uint8_t)~m;
}

inline void hline(int x0, int x1, int y, bool on = true) {
    if(x0 > x1) {
        int t = x0;
        x0 = x1;
        x1 = t;
    }
    if(y < clip.y0 || y > clip.y1) return;
    if(x0 < clip.x0) x0 = clip.x0;
    if(x1 > clip.x1) x1 = clip.x1;
    uint8_t* row = buf + (y >> 3) * W;
    uint8_t m = (uint8_t)(1u << (y & 7));
    if(on)
        for(int x = x0; x <= x1; x++)
            row[x] |= m;
    else
        for(int x = x0; x <= x1; x++)
            row[x] &= (uint8_t)~m;
}

inline void vline(int x, int y0, int y1, bool on = true) {
    if(y0 > y1) {
        int t = y0;
        y0 = y1;
        y1 = t;
    }
    for(int y = y0; y <= y1; y++)
        px(x, y, on);
}

inline void rect(int x, int y, int w, int h, bool on = true) {
    if(w <= 0 || h <= 0) return;
    hline(x, x + w - 1, y, on);
    hline(x, x + w - 1, y + h - 1, on);
    vline(x, y, y + h - 1, on);
    vline(x + w - 1, y, y + h - 1, on);
}

inline void fill(int x, int y, int w, int h, bool on = true) {
    for(int j = 0; j < h; j++)
        hline(x, x + w - 1, y + j, on);
}

// Bresenham. Callers clip first, so the coordinates stay small.
inline void lineI(int x0, int y0, int x1, int y1, bool on = true) {
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for(int guard = 0; guard < 512; guard++) {
        px(x0, y0, on);
        if(x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if(e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if(e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

// Liang-Barsky against the clip rectangle. False if the segment misses.
inline bool clipSeg(float& x0, float& y0, float& x1, float& y1) {
    const float X0 = (float)clip.x0, Y0 = (float)clip.y0;
    const float X1 = (float)clip.x1, Y1 = (float)clip.y1;
    float dx = x1 - x0, dy = y1 - y0;
    float t0 = 0.0f, t1 = 1.0f;
    auto edge = [&](float p, float q) -> bool {
        if(p == 0.0f) return q >= 0.0f;
        float r = q / p;
        if(p < 0.0f) {
            if(r > t1) return false;
            if(r > t0) t0 = r;
        } else {
            if(r < t0) return false;
            if(r < t1) t1 = r;
        }
        return true;
    };
    if(!edge(-dx, x0 - X0) || !edge(dx, X1 - x0) || !edge(-dy, y0 - Y0) || !edge(dy, Y1 - y0))
        return false;
    float ax = x0 + t0 * dx, ay = y0 + t0 * dy;
    x1 = x0 + t1 * dx;
    y1 = y0 + t1 * dy;
    x0 = ax;
    y0 = ay;
    return true;
}

inline void line(float x0, float y0, float x1, float y1, bool on = true) {
    if(!clipSeg(x0, y0, x1, y1)) return;
    lineI(iround(x0), iround(y0), iround(x1), iround(y1), on);
}

// Circle outline, scanned by rows and by columns over the clip rectangle
// only, so a planet a thousand pixels wide costs the same as a small one.
inline void circle(float cx, float cy, float r, bool on = true) {
    if(r < 0.75f) {
        px(iround(cx), iround(cy), on);
        return;
    }
    if(cx + r < (float)clip.x0 || cx - r > (float)clip.x1 || cy + r < (float)clip.y0 ||
       cy - r > (float)clip.y1)
        return;
    int ya = (int)ceilf(cy - r), yb = (int)floorf(cy + r);
    if(ya < clip.y0) ya = clip.y0;
    if(yb > clip.y1) yb = clip.y1;
    for(int y = ya; y <= yb; y++) {
        float d = (float)y - cy;
        float h = (r - d) * (r + d);
        float w = h > 0.0f ? sqrtf(h) : 0.0f;
        px(iround(cx - w), y, on);
        px(iround(cx + w), y, on);
    }
    int xa = (int)ceilf(cx - r), xb = (int)floorf(cx + r);
    if(xa < clip.x0) xa = clip.x0;
    if(xb > clip.x1) xb = clip.x1;
    for(int x = xa; x <= xb; x++) {
        float d = (float)x - cx;
        float h = (r - d) * (r + d);
        float w = h > 0.0f ? sqrtf(h) : 0.0f;
        px(x, iround(cy - w), on);
        px(x, iround(cy + w), on);
    }
}

// ---------- 3x5 font ----------
// Five rows per glyph, one octal digit per row, top row first; 4 is the
// left column. Covers ' ' to '_'; lower case prints as upper case.
inline const uint16_t Font3x5[64] = {
    000000, 022202, 055000, 057575, 036736, 051245, 025253, 022000, //  !"#$%&'
    024442, 042224, 005250, 002720, 000024, 000700, 000002, 011244, // ()*+,-./
    075557, 026227, 071747, 071717, 055711, 074717, 074757, 071122, // 01234567
    075757, 075717, 002020, 002024, 012421, 007070, 042124, 061202, // 89:;<=>?
    025543, 025755, 065656, 034443, 065556, 074647, 074644, 034553, // @ABCDEFG
    055755, 072227, 011152, 055655, 044447, 057555, 065555, 025552, // HIJKLMNO
    065644, 025563, 065655, 034216, 072222, 055557, 055552, 055775, // PQRSTUVW
    055255, 055222, 071247, 064446, 044211, 032223, 025000, 000007, // XYZ[\]^_
};

inline void glyph(int x, int y, char c, bool on = true) {
    if(c >= 'a' && c <= 'z') c = (char)(c - 32);
    if(c < 32 || c > 95) c = '?';
    uint16_t g = Font3x5[c - 32];
    for(int row = 0; row < 5; row++) {
        int bits = (g >> (3 * (4 - row))) & 7;
        if(bits & 4) px(x, y + row, on);
        if(bits & 2) px(x + 1, y + row, on);
        if(bits & 1) px(x + 2, y + row, on);
    }
}

inline int textWidth(const char* s) {
    int n = (int)strlen(s);
    return n > 0 ? n * 4 - 1 : 0;
}

inline int text(int x, int y, const char* s, bool on = true) {
    for(; *s; s++, x += 4)
        glyph(x, y, *s, on);
    return x;
}

inline void textCenter(int cx, int y, const char* s, bool on = true) {
    text(cx - textWidth(s) / 2, y, s, on);
}

inline void textRight(int x1, int y, const char* s, bool on = true) {
    text(x1 - textWidth(s) + 1, y, s, on);
}

// Text on a cleared box so it stays readable over wireframe.
inline void label(int x, int y, const char* s) {
    fill(x - 1, y - 1, textWidth(s) + 2, 7, false);
    text(x, y, s);
}

inline void labelCenter(int cx, int y, const char* s) {
    label(cx - textWidth(s) / 2, y, s);
}

} // namespace Fb
