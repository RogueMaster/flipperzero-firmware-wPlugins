#pragma once

#include <gui/gui.h>
#include "vec2.h"

// Use to draw table elements, which live on a 640 x 1280 grid
// These methods will scale and round the coordinates
// Also, they will (eventually) handle cases where the thing we're drawing
// lies outside the table bounds.

void gfx_draw_line(Canvas* canvas, float x1, float y1, float x2, float y2);
void gfx_draw_line(Canvas* canvas, const Vec2& p1, const Vec2& p2);

void gfx_draw_line_thick(Canvas* canvas, float x1, float y1, float x2, float y2, int thickness);
void gfx_draw_line_thick(Canvas* canvas, const Vec2& p1, const Vec2& p2, int thickness);

void gfx_draw_disc(Canvas* canvas, float x, float y, float r);
void gfx_draw_disc(Canvas* canvas, const Vec2& p, float r);

void gfx_draw_circle(Canvas* canvas, float x, float y, float r);
void gfx_draw_circle(Canvas* canvas, const Vec2& p, float r);

void gfx_draw_dot(Canvas* canvas, float x, float y);
void gfx_draw_dot(Canvas* canvas, const Vec2& p);

void gfx_draw_arc(Canvas* canvas, const Vec2& p, float r, float start, float end);

// Uses the micro font
void gfx_draw_str(Canvas* canvas, int x, int y, Align h, Align v, const char* str);
