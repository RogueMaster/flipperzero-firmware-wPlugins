/* Lays a UiFrame out on the 128x64 canvas after the design
 * (docs/design/source/flipper-ui.js): header with the transport icon,
 * bottom button chips, lists with a scrollbar, the colour swatch, the spool
 * with waves and the weight digits. Flipper only; it runs on the GUI
 * thread and reads nothing but the frame.
 */
#pragma once

#include <gui/canvas.h>

#include "ui_frame.h"

void ui_draw(Canvas* canvas, const UiFrame* frame);
