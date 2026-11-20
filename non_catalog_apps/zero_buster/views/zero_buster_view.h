#pragma once

#include <gui/gui.h>
#include <input/input.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ZeroBusterApp ZeroBusterApp;

void zero_buster_view_draw_callback(Canvas* canvas, void* context);

void zero_buster_view_input_callback(InputEvent* input_event, void* context);

#ifdef __cplusplus
}
#endif
