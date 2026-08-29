#ifndef CARTO_FRAMEBUFFER_H
#define CARTO_FRAMEBUFFER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CARTO_FMT_MONO1    = 0,
    CARTO_FMT_INDEXED8 = 1,
    CARTO_FMT_RGB565   = 2,
    CARTO_FMT_RGBA8888 = 3
} carto_pixfmt;

typedef struct {
    int          width;
    int          height;
    carto_pixfmt format;
    int          stride;
    uint8_t     *pixels;
    uint16_t    *cell_color;
    int          cell_cols;
    int          cell_rows;
} carto_framebuffer;

static inline size_t carto_fb_row_bytes(int w, carto_pixfmt fmt) {
    switch (fmt) {
        case CARTO_FMT_MONO1:    return (size_t)((w + 7) / 8);
        case CARTO_FMT_INDEXED8: return (size_t)w;
        case CARTO_FMT_RGB565:   return (size_t)w * 2;
        case CARTO_FMT_RGBA8888: return (size_t)w * 4;
    }
    return 0;
}

static inline size_t carto_fb_size(const carto_framebuffer *fb) {
    return (size_t)fb->stride * (size_t)fb->height;
}

int  carto_fb_init(carto_framebuffer *fb, int w, int h, carto_pixfmt fmt,
                   uint8_t *pixels);
void carto_fb_clear(carto_framebuffer *fb, uint32_t value);

#ifdef __cplusplus
}
#endif

#endif
