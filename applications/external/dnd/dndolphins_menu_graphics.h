#pragma once

#include <gui/gui.h>
#include <stdint.h>

/* Draws the native 25x25 icon for one DNDolphins home entry. Every Home
   entry owns a stable per-entry asset filename so artwork can be replaced
   independently without changing menu code. */
void dndolphins_menu_graphics_draw_icon(Canvas* canvas, uint16_t index, int32_t x, int32_t y);
