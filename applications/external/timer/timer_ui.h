#ifndef TIMER_UI_H
#define TIMER_UI_H

#include "timer_app.h"

void draw_thick_line(Canvas* canvas, int x0, int y0, int x1, int y1, int thickness);
void draw_small_analog_clock(Canvas* canvas, time_t now);
void draw_callback(Canvas* canvas, void* ctx);
void input_callback(InputEvent* event, void* ctx);

#endif // TIMER_UI_H
