#pragma once

// Foto en escala de grises -> pixel art de 1 bit para la pantalla del Flipper.
//  1. prepare: escalado por promedio de area + contraste automatico (se guarda, ~4 KB)
//  2. dither:  tramado a 1 bit con el estilo elegido (se recalcula al cambiar de estilo)

#include <stdint.h>
#include <stdbool.h>

#define DNIE_PIX_MAX_W  64
#define DNIE_PIX_MAX_H  64
#define DNIE_PIX_STRIDE ((DNIE_PIX_MAX_W + 7) / 8)

typedef enum {
    DnieDitherAtkinson, // estilo Mac clasico, mucho contraste
    DnieDitherFloyd, // Floyd-Steinberg, mas tonos intermedios
    DnieDitherBayer, // tramado ordenado 4x4, muy "retro"
    DnieDitherCount,
} DnieDither;

typedef struct {
    uint8_t width;
    uint8_t height;
    uint8_t gray[DNIE_PIX_MAX_W * DNIE_PIX_MAX_H];
} DniePixelSource;

typedef struct {
    uint8_t width;
    uint8_t height;
    // Formato XBM (el de canvas_draw_xbm): filas de (width+7)/8 bytes, bit 0 = pixel de la
    // izquierda, 1 = pixel "encendido" (oscuro en la pantalla del Flipper)
    uint8_t xbm[DNIE_PIX_STRIDE * DNIE_PIX_MAX_H];
} DniePixelArt;

// Ajusta la imagen a max_w x max_h conservando la proporcion y estira el contraste.
bool dnie_pixelart_prepare(
    const uint8_t* gray,
    uint16_t gray_w,
    uint16_t gray_h,
    uint8_t max_w,
    uint8_t max_h,
    DniePixelSource* out);

// Tramado a 1 bit. Devuelve false si no hay memoria.
bool dnie_pixelart_dither(const DniePixelSource* src, DnieDither mode, DniePixelArt* out);

const char* dnie_dither_name(DnieDither mode);

static inline int dnie_pixelart_stride(const DniePixelArt* p) {
    return (p->width + 7) / 8;
}

static inline bool dnie_pixelart_get(const DniePixelArt* p, int x, int y) {
    return (p->xbm[y * dnie_pixelart_stride(p) + x / 8] >> (x % 8)) & 1;
}
