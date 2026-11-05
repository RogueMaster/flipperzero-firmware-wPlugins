#include "dnie_pixelart.h"

#include <stdlib.h>
#include <string.h>

bool dnie_pixelart_prepare(
    const uint8_t* gray,
    uint16_t gray_w,
    uint16_t gray_h,
    uint8_t max_w,
    uint8_t max_h,
    DniePixelSource* out) {
    memset(out, 0, sizeof(*out));
    if(!gray || !gray_w || !gray_h) return false;
    if(max_w > DNIE_PIX_MAX_W) max_w = DNIE_PIX_MAX_W;
    if(max_h > DNIE_PIX_MAX_H) max_h = DNIE_PIX_MAX_H;

    // Tamano destino conservando la proporcion
    uint32_t tw = ((uint32_t)gray_w * max_h + gray_h / 2) / gray_h;
    uint32_t th = max_h;
    if(tw > max_w) {
        tw = max_w;
        th = ((uint32_t)gray_h * max_w + gray_w / 2) / gray_w;
    }
    if(tw == 0) tw = 1;
    if(th == 0) th = 1;

    // 1. Promedio de area: cada pixel destino promedia los pixeles origen cuyo centro cae dentro
    for(uint32_t y = 0; y < th; y++) {
        uint32_t sy0 = y * gray_h / th, sy1 = (y + 1) * gray_h / th;
        if(sy1 <= sy0) sy1 = sy0 + 1;
        for(uint32_t x = 0; x < tw; x++) {
            uint32_t sx0 = x * gray_w / tw, sx1 = (x + 1) * gray_w / tw;
            if(sx1 <= sx0) sx1 = sx0 + 1;
            uint32_t sum = 0, n = 0;
            for(uint32_t sy = sy0; sy < sy1 && sy < gray_h; sy++) {
                for(uint32_t sx = sx0; sx < sx1 && sx < gray_w; sx++) {
                    sum += gray[sy * gray_w + sx];
                    n++;
                }
            }
            out->gray[y * tw + x] = (uint8_t)(n ? sum / n : 0);
        }
    }

    // 2. Contraste automatico: el 2 % mas oscuro -> negro, el 2 % mas claro -> blanco
    uint32_t hist[256] = {0};
    uint32_t total = tw * th;
    for(uint32_t i = 0; i < total; i++)
        hist[out->gray[i]]++;
    uint32_t cut = total / 50, acc = 0;
    int lo = 0, hi = 255;
    for(int v = 0; v < 256; v++) {
        acc += hist[v];
        if(acc > cut) {
            lo = v;
            break;
        }
    }
    acc = 0;
    for(int v = 255; v >= 0; v--) {
        acc += hist[v];
        if(acc > cut) {
            hi = v;
            break;
        }
    }
    if(hi - lo >= 16) {
        for(uint32_t i = 0; i < total; i++) {
            int v = (out->gray[i] - lo) * 255 / (hi - lo);
            out->gray[i] = (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v));
        }
    }

    out->width = (uint8_t)tw;
    out->height = (uint8_t)th;
    return true;
}

static void set_on(DniePixelArt* out, uint32_t x, uint32_t y) {
    out->xbm[y * dnie_pixelart_stride(out) + x / 8] |= (uint8_t)(1 << (x % 8));
}

// Difusion de error generica: offsets y pesos sobre un divisor
typedef struct {
    int8_t dx, dy;
    uint8_t w;
} Tap;

static bool
    diffuse(const DniePixelSource* src, DniePixelArt* out, const Tap* taps, int ntaps, int divisor) {
    uint32_t tw = src->width, th = src->height;
    int16_t* buf = malloc(sizeof(int16_t) * tw * th);
    if(!buf) return false;
    for(uint32_t i = 0; i < tw * th; i++)
        buf[i] = src->gray[i];
    for(uint32_t y = 0; y < th; y++) {
        for(uint32_t x = 0; x < tw; x++) {
            int old = buf[y * tw + x];
            int nv = old < 128 ? 0 : 255;
            int err = old - nv;
            if(nv == 0) set_on(out, x, y); // oscuro -> pixel encendido
            for(int k = 0; k < ntaps; k++) {
                int nx = (int)x + taps[k].dx, ny = (int)y + taps[k].dy;
                if(nx < 0 || nx >= (int)tw || ny >= (int)th) continue;
                int v = buf[ny * tw + nx] + err * taps[k].w / divisor;
                buf[ny * tw + nx] = (int16_t)(v < -512 ? -512 : (v > 767 ? 767 : v));
            }
        }
    }
    free(buf);
    return true;
}

bool dnie_pixelart_dither(const DniePixelSource* src, DnieDither mode, DniePixelArt* out) {
    memset(out, 0, sizeof(*out));
    out->width = src->width;
    out->height = src->height;

    if(mode == DnieDitherAtkinson) {
        // Reparte 6/8 del error: se pierde 1/4, de ahi su contraste
        static const Tap t[] = {{1, 0, 1}, {2, 0, 1}, {-1, 1, 1}, {0, 1, 1}, {1, 1, 1}, {0, 2, 1}};
        return diffuse(src, out, t, 6, 8);
    }
    if(mode == DnieDitherFloyd) {
        static const Tap t[] = {{1, 0, 7}, {-1, 1, 3}, {0, 1, 5}, {1, 1, 1}};
        return diffuse(src, out, t, 4, 16);
    }
    // Bayer 4x4: umbral ordenado, sin difusion
    static const uint8_t bayer[4][4] = {
        {0, 8, 2, 10},
        {12, 4, 14, 6},
        {3, 11, 1, 9},
        {15, 7, 13, 5},
    };
    for(uint32_t y = 0; y < src->height; y++) {
        for(uint32_t x = 0; x < src->width; x++) {
            int threshold = bayer[y % 4][x % 4] * 16 + 8;
            if(src->gray[y * src->width + x] < threshold) set_on(out, x, y);
        }
    }
    return true;
}

const char* dnie_dither_name(DnieDither mode) {
    switch(mode) {
    case DnieDitherAtkinson:
        return "Atkinson";
    case DnieDitherFloyd:
        return "Floyd-Steinberg";
    case DnieDitherBayer:
        return "Bayer";
    default:
        return "?";
    }
}
