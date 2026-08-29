#include "carto/framebuffer.h"
#include <string.h>

int carto_fb_init(carto_framebuffer *fb, int w, int h, carto_pixfmt fmt,
                  uint8_t *pixels) {
    if (!fb || !pixels || w <= 0 || h <= 0) return -1;
    fb->width = w;
    fb->height = h;
    fb->format = fmt;
    fb->stride = (int)carto_fb_row_bytes(w, fmt);
    fb->pixels = pixels;
    fb->cell_color = NULL;
    fb->cell_cols = 0;
    fb->cell_rows = 0;
    return 0;
}

void carto_fb_clear(carto_framebuffer *fb, uint32_t value) {
    if (!fb || !fb->pixels) return;

    switch (fb->format) {
        case CARTO_FMT_MONO1:
            memset(fb->pixels, value ? 0xFF : 0x00, carto_fb_size(fb));
            return;
        case CARTO_FMT_INDEXED8:
            memset(fb->pixels, (int)(value & 0xFF), carto_fb_size(fb));
            return;
        case CARTO_FMT_RGB565: {
            uint16_t v = (uint16_t)value;
            for (int y = 0; y < fb->height; ++y) {
                uint16_t *row = (uint16_t *)(fb->pixels + (size_t)y * fb->stride);
                for (int x = 0; x < fb->width; ++x) row[x] = v;
            }
            return;
        }
        case CARTO_FMT_RGBA8888: {
            for (int y = 0; y < fb->height; ++y) {
                uint32_t *row = (uint32_t *)(fb->pixels + (size_t)y * fb->stride);
                for (int x = 0; x < fb->width; ++x) row[x] = value;
            }
            return;
        }
    }
}
