#include "game.hpp"

static constexpr uint8_t SET_MASK[8] = {
    0x01,
    0x02,
    0x04,
    0x08,
    0x10,
    0x20,
    0x40,
    0x80,
};

static constexpr uint8_t CLEAR_MASK[8] = {
    0xfe,
    0xfd,
    0xfb,
    0xf7,
    0xef,
    0xdf,
    0xbf,
    0x7f,
};

static constexpr uint8_t YMASK0[8] = {0xff, 0xfe, 0xfc, 0xf8, 0xf0, 0xe0, 0xc0, 0x80};

static constexpr uint8_t YMASK1[8] = {0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff};

static constexpr uint16_t PATTERNS[5] = {
    0x0000,
    0xaa00,
    0xaa55,
    0xff55,
    0xffff,
};

void set_pixel(uint8_t x, uint8_t y) {
    buf[(y >> 3) * FBW + x] |= SET_MASK[y & 7];
}

void inv_pixel(uint8_t x, uint8_t y) {
    buf[(y >> 3) * FBW + x] ^= SET_MASK[y & 7];
}

void clear_pixel(uint8_t x, uint8_t y) {
    buf[(y >> 3) * FBW + x] &= CLEAR_MASK[y & 7];
}

static void draw_vline(uint8_t x, int16_t y0, int16_t y1, uint16_t pat) {
    if(y0 > y1) return;
    if(y1 < 0) return;
    if(y0 >= FBH) return;
    if(x >= FBW) return;

    uint8_t ty0 = (uint8_t)tmax<int16_t>(y0, 0);
    uint8_t ty1 = (uint8_t)tmin<int16_t>(y1, FBH - 1);

    uint8_t t0 = ty0 & 0xf8;
    uint8_t t1 = ty1 & 0xf8;
    uint8_t m0 = YMASK0[ty0 & 7];
    uint8_t m1 = YMASK1[ty1 & 7];

    uint8_t pattern = (x & 1) ? uint8_t(pat) : uint8_t(pat >> 8);

    uint8_t* p = &buf[(uint16_t(t0) * (FBW / 8)) + x];

    if(t0 == t1) {
        uint8_t m = uint8_t(m0 & m1);
        uint8_t tp = *p;
        tp |= (pattern & m);
        tp &= (uint8_t)(pattern | ~m);
        *p = tp;
        return;
    }

    {
        uint8_t m = m0;
        uint8_t tp = *p;
        tp |= (pattern & m);
        tp &= (uint8_t)(pattern | ~m);
        *p = tp;
        p += FBW;
    }

    for(int8_t t = int8_t(t1 - t0 - 8); t > 0; t -= 8) {
        *p = pattern;
        p += FBW;
    }

    {
        uint8_t m = m1;
        uint8_t tp = *p;
        tp |= (pattern & m);
        tp &= (uint8_t)(pattern | ~m);
        *p = tp;
    }
}

static void draw_tri_segment(
    int16_t ax,
    int16_t ay0,
    int16_t ay1,
    int16_t bx,
    int16_t by0,
    int16_t by1,
    uint16_t pat) {
    if(ax >= bx) return;

    if(ay0 > ay1 || by0 > by1) {
        tswap(ay0, ay1);
        tswap(by0, by1);
    }

    int16_t dx = bx - ax;
    int16_t dy0 = tabs(by0 - ay0);
    int16_t dy1 = tabs(by1 - ay1);

    int16_t e0, e1;
    {
        uint8_t ay0mask = uint8_t(ay0 & FB_FRAC_MASK);
        uint8_t ay1mask = uint8_t(ay1 & FB_FRAC_MASK);
        if(ay0 > by0) ay0mask = (FB_FRAC_COEF - ay0mask) & FB_FRAC_MASK;
        if(ay1 > by1) ay1mask = (FB_FRAC_COEF - ay1mask) & FB_FRAC_MASK;
        int8_t tax = int8_t((ax & FB_FRAC_MASK) - (FB_FRAC_COEF / 2));
        e0 = div_frac_s(int16_t(dx * (int16_t(ay0mask) - FB_FRAC_COEF) - dy0 * tax));
        e1 = div_frac_s(int16_t(dx * (int16_t(ay1mask) - FB_FRAC_COEF) - dy1 * tax));
    }

    int8_t sy0 = (ay0 < by0 ? 1 : -1);
    int8_t sy1 = (ay1 < by1 ? 1 : -1);
    if(ay0 > by0) ay0 -= 1;
    if(ay1 > by1) ay1 -= 1;

    int16_t pxa = div_frac_s(ax);
    int16_t pxb = div_frac_s(int16_t(bx + (FB_FRAC_COEF / 2 - 1)));
    int16_t py0 = div_frac_s(ay0);
    int16_t py1 = div_frac_s(ay1);

    while(pxa != pxb) {
        while(e0 > 0) {
            py0 += sy0;
            e0 -= dx;
        }
        while(e1 > 0) {
            py1 += sy1;
            e1 -= dx;
        }
        draw_vline((uint8_t)pxa, py0, py1, pat);
        pxa += 1;
        e0 += dy0;
        e1 += dy1;
    }
}

static void draw_circle_bresenham_segment_filled(
    int16_t cx,
    int16_t cy,
    int16_t x,
    int16_t y,
    uint16_t pat) {
    draw_vline(uint8_t(cx + x), cy - y, cy + y, pat);
    draw_vline(uint8_t(cx - x), cy - y, cy + y, pat);
    draw_vline(uint8_t(cx + y), cy - x, cy + x, pat);
    draw_vline(uint8_t(cx - y), cy - x, cy + x, pat);
}

static void draw_circle_set_pixel(uint8_t x, int16_t y) {
    if(x >= FBW) return;
    if(y < 0 || y >= FBH) return;
    uint8_t* p = &buf[((uint16_t(y & 0x38) << 4) + x)];
    *p &= (uint8_t) ~(1u << (y & 0x7));
}

static void draw_circle_bresenham_segment_outline(int16_t cx, int16_t cy, int16_t x, int16_t y) {
    draw_circle_set_pixel(uint8_t(cx + x), cy - y);
    draw_circle_set_pixel(uint8_t(cx + x), cy + y);
    draw_circle_set_pixel(uint8_t(cx - x), cy - y);
    draw_circle_set_pixel(uint8_t(cx - x), cy + y);
    draw_circle_set_pixel(uint8_t(cx + y), cy - x);
    draw_circle_set_pixel(uint8_t(cx + y), cy + x);
    draw_circle_set_pixel(uint8_t(cx - y), cy - x);
    draw_circle_set_pixel(uint8_t(cx - y), cy + x);
}

void draw_ball_outline(dvec2 c, uint16_t r) {
    if(c.x < -(int16_t)r || c.y < -(int16_t)r) return;
    if(c.x > FBW * FB_FRAC_COEF + r || c.y > FBW * FB_FRAC_COEF + r) return;

    c.x = div_frac_s(c.x);
    c.y = div_frac_s(c.y);
    r >>= FB_FRAC_BITS;

    int16_t dx = 12;
    int16_t dy = int16_t(8 - int16_t(r) * 8);
    int16_t e = int16_t(5 - int16_t(r) * 4);
    int16_t y = (int16_t)r;

    for(int16_t x = 0; x <= y; ++x) {
        draw_circle_bresenham_segment_outline(c.x, c.y, x, y);
        if(e >= 0) {
            e += dy;
            dy += 8;
            y -= 1;
        }
        e += dx;
        dx += 8;
    }
}

void draw_ball_filled(dvec2 c, uint16_t r, uint16_t pat) {
    if(c.x < -(int16_t)r || c.y < -(int16_t)r) return;
    if(c.x > FBW * FB_FRAC_COEF + r || c.y > FBW * FB_FRAC_COEF + r) return;

    c.x = div_frac_s(c.x);
    c.y = div_frac_s(c.y);
    r >>= FB_FRAC_BITS;

    int16_t dx = 12;
    int16_t dy = int16_t(8 - int16_t(r) * 8);
    int16_t e = int16_t(5 - int16_t(r) * 4);
    int16_t y = (int16_t)r;

    for(int16_t x = 0; x <= y; ++x) {
        draw_circle_bresenham_segment_filled(c.x, c.y, x, y, pat);
        if(e >= 0) {
            e += dy;
            dy += 8;
            y -= 1;
        }
        e += dx;
        dx += 8;
    }
}

int16_t interp(int16_t a, int16_t b, int16_t c, int16_t x, int16_t z) {
    if(a == c) return x;

    uint16_t xz = (x < z ? uint16_t(z - x) : uint16_t(x - z));
    uint32_t p = uint32_t(xz) * uint16_t(b - a);
    uint16_t ac = uint16_t(c - a);
    int16_t t;

    if(ac >= 256) {
        uint16_t i = inv16(ac);
        t = int16_t(((p >> 8) * i) >> 16);
    } else {
        uint16_t i = inv8((uint8_t)ac);
        t = int16_t((p * i) >> 16);
    }

    if(x > z) t = -t;
    return x + t;
}

static inline int8_t to8(int16_t x) {
    return int8_t(uint16_t(x) >> 8);
}

void draw_tri(dvec2 v0, dvec2 v1, dvec2 v2, uint8_t pati) {
    if(tmax(v0.y, v1.y, v2.y) < 0) return;
    if(tmin(v0.y, v1.y, v2.y) > FBH * FB_FRAC_COEF) return;

    if(v0.x > v1.x) tswap(v0, v1);
    if(v1.x > v2.x) tswap(v1, v2);
    if(v0.x > v1.x) tswap(v0, v1);

    if(v2.x < 0 || v0.x >= FBW * FB_FRAC_COEF || v0.x == v2.x) return;

    int16_t ty = interp(v0.x, v1.x, v2.x, v0.y, v2.y);

    uint16_t pat = PATTERNS[tmin<uint8_t>(pati, 4)];

    {
        int16_t ax = v0.x;
        int16_t bx = v1.x;
        int16_t ay0 = v0.y;
        int16_t ay1 = v0.y;
        int16_t by0 = v1.y;
        int16_t by1 = ty;

        if(ax < 0) {
            ay0 = interp(ax, 0, bx, ay0, by0);
            ay1 = interp(ax, 0, bx, ay1, by1);
            ax = 0;
        }
        if(bx > FBW * FB_FRAC_COEF) {
            by0 = interp(ax, FBW * FB_FRAC_COEF, bx, ay0, by0);
            by1 = interp(ax, FBW * FB_FRAC_COEF, bx, ay1, by1);
            bx = FBW * FB_FRAC_COEF;
        }
        draw_tri_segment(ax, ay0, ay1, bx, by0, by1, pat);
    }

    {
        int16_t ax = v1.x;
        int16_t bx = v2.x;
        int16_t ay0 = v1.y;
        int16_t ay1 = ty;
        int16_t by0 = v2.y;
        int16_t by1 = v2.y;

        if(ax < 0) {
            ay0 = interp(ax, 0, bx, ay0, by0);
            ay1 = interp(ax, 0, bx, ay1, by1);
            ax = 0;
        }
        if(bx > FBW * FB_FRAC_COEF) {
            by0 = interp(ax, FBW * FB_FRAC_COEF, bx, ay0, by0);
            by1 = interp(ax, FBW * FB_FRAC_COEF, bx, ay1, by1);
            bx = FBW * FB_FRAC_COEF;
        }
        draw_tri_segment(ax, ay0, ay1, bx, by0, by1, pat);
    }
}
