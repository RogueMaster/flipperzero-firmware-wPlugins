#pragma once

#include "tpms_bridge.h"

#include <gui/canvas.h>

/** How many list rows fit on the screen. */
#define TPMS_VIEW_ROWS 4

/** Draw the current screen. State is already locked by state_mutex. */
void tpms_view_draw(Canvas* canvas, TpmsBridgeApp* app);

/** Adjust scrolling so that the selected row stays visible. */
void tpms_view_follow_selection(TpmsBridgeApp* app);
