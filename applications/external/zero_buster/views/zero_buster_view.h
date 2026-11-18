#pragma once

#include <gui/gui.h>
#include <input/input.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration */
typedef struct ZeroBusterApp ZeroBusterApp;

/* View draw callback for ViewPort */
void zero_buster_view_draw_callback(Canvas* canvas, void* context);

/* View input callback for ViewPort */
void zero_buster_view_input_callback(InputEvent* input_event, void* context);

#ifdef __cplusplus
}
#endif
