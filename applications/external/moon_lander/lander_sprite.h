#pragma once

#include <gui/gui.h>

/* Lander geometry — the source of truth for the lander's visual *and*
 * collision footprint. Game.c's collision math keys off LANDER_FOOT_DX /
 * LANDER_FOOT_DY only; the other constants are internal to the sprite. */

#define LANDER_HALF_W       2.0f    // platform half-width (also leg x position)
#define LANDER_BODY_TOP    -3.0f    // y of body top edge
#define LANDER_BODY_BOT     1.0f    // y of platform; legs and flame attach here
#define LANDER_FOOT_DX      3.0f    // foot tip (outermost) x offset from center
#define LANDER_FOOT_DY      3.0f    // foot y

/* Draw the lander upright (no rotation) centered at integer pixel (cx, cy).
 * Fast path used by the menu's title bar. */
void lander_draw_static(Canvas* canvas, int cx, int cy);

/* Draw the lander rotated by `angle` (radians, 0 = upright, +ve = clockwise)
 * with the body center at float (cx, cy). If `thrust` > 0.05, a flame is
 * drawn from the platform, scaled by thrust intensity in [0, 1]. `y_scale`
 * squishes the sprite vertically for TV mode (1.0 = normal). */
void lander_draw_rotated(
    Canvas* canvas, float cx, float cy, float angle, float thrust, float y_scale);
