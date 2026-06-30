#ifndef BACKGROUND_H
#define BACKGROUND_H

#include <gui/canvas.h>

#include "settings.h"

static double grid_offset = 0.0;

void background_init() {
}

void background_clear() {
}

void background_update(double dt) {
    grid_offset += settings.grid_speed * dt;
    if(grid_offset >= settings.grid_spacing) grid_offset -= settings.grid_spacing;
}

void background_draw(Canvas* canvas) {
    size_t w = canvas_width(canvas);
    size_t h = canvas_height(canvas);

    int goff = (int)grid_offset;

    for(int gx = -goff; gx < (int)w; gx += settings.grid_spacing) {
        canvas_draw_line(canvas, gx, 0, gx, (int)h);
    }

    for(int gy = -goff; gy < (int)h; gy += settings.grid_spacing) {
        canvas_draw_line(canvas, 0, gy, (int)w, gy);
    }
}

#endif
