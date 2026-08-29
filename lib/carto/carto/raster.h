#ifndef CARTO_RASTER_H
#define CARTO_RASTER_H

#include "carto/framebuffer.h"
#include "carto/style.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { int x, y; } carto_ipt;

uint16_t carto_rgb565(carto_rgb c);

void carto_put_px(carto_framebuffer *fb, int x, int y, carto_rgb c);
void carto_fill_polygon(carto_framebuffer *fb, const carto_ipt *pts, int n, carto_rgb c);
void carto_polyline(carto_framebuffer *fb, const carto_ipt *pts, int n, int width, carto_rgb c);
void carto_fill_rect(carto_framebuffer *fb, int x, int y, int w, int h, carto_rgb c);
void carto_draw_line(carto_framebuffer *fb, int x0, int y0, int x1, int y1,
                     int width, carto_rgb c);
void carto_fill_triangle(carto_framebuffer *fb, int x0, int y0, int x1, int y1,
                         int x2, int y2, carto_rgb c);

#ifdef __cplusplus
}
#endif

#endif
