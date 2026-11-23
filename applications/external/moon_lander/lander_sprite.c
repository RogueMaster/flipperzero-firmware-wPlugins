#include "lander_sprite.h"
#include <math.h>
#include <stdlib.h>

/* Sprite layout — 7×7 with 2-px feet.
 *
 *        ###       y=-3   body top (3 wide)
 *       #   #      y=-2   ┐ body sides (5 wide outline,
 *       #   #      y=-1   ┘   four corner pixels intentionally cut)
 *        ###       y= 0   body bottom (3 wide)
 *       #####      y=+1   platform (5 wide, where legs attach)
 *       #   #      y=+2   legs straight down at ±2
 *      ##   ##     y=+3   2-pixel feet at each corner
 *
 * No antenna. The "taper" is the four cut corner pixels of the body outline.
 * Foot tips (outermost pixels) sit at ±LANDER_FOOT_DX = ±3, which is also
 * the collision footprint game.c uses.
 */

void lander_draw_static(Canvas* canvas, int cx, int cy) {
    /* Body outline — rounded rectangle, four corner pixels left blank */
    canvas_draw_line(canvas, cx - 1, cy - 3, cx + 1, cy - 3);   // top
    canvas_draw_line(canvas, cx - 2, cy - 2, cx - 2, cy - 1);   // left side
    canvas_draw_line(canvas, cx + 2, cy - 2, cx + 2, cy - 1);   // right side
    canvas_draw_line(canvas, cx - 1, cy,     cx + 1, cy);       // bottom
    /* Platform — 5-wide horizontal under the body */
    canvas_draw_line(canvas, cx - 2, cy + 1, cx + 2, cy + 1);
    /* Legs — vertical from platform corners straight down to foot height */
    canvas_draw_line(canvas, cx - 2, cy + 1, cx - 2, cy + 3);
    canvas_draw_line(canvas, cx + 2, cy + 1, cx + 2, cy + 3);
    /* Feet — 2 pixels each, extending one pixel outward from each leg */
    canvas_draw_line(canvas, cx - 3, cy + 3, cx - 2, cy + 3);
    canvas_draw_line(canvas, cx + 2, cy + 3, cx + 3, cy + 3);
}

/* canvas_draw_line() hands its coordinates to u8g2 as uint16_t, and u8g2
 * doesn't clip lines. So a 2-px segment from x = -1 to x = 1 is walked from
 * x = 1 all the way up to x = 65534: ~65k pixel calls (~40 ms) per segment,
 * plus a stray streak to the screen edge. The lander hits this whenever it
 * overlaps the left or top edge, and while a frame takes that long the main
 * loop starves and key events get dropped (stuck thrust/rotation).
 *
 * This is u8g2's own Bresenham walk (same pixels for on-screen lines), done
 * in signed ints, plotting only the pixels that land on the canvas. */
static void draw_line_clipped(Canvas* canvas, int x1, int y1, int x2, int y2) {
    const int w = (int)canvas_width(canvas);
    const int h = (int)canvas_height(canvas);
    /* Entirely off one side: nothing to plot. Also keeps the walk short if
     * a runaway position ever saturates both ends to INT_MAX. */
    if ((x1 < 0 && x2 < 0) || (x1 >= w && x2 >= w) ||
        (y1 < 0 && y2 < 0) || (y1 >= h && y2 >= h)) return;
    bool steep = abs(y2 - y1) > abs(x2 - x1);
    int t;
    if (steep) {
        t = x1; x1 = y1; y1 = t;
        t = x2; x2 = y2; y2 = t;
    }
    if (x1 > x2) {
        t = x1; x1 = x2; x2 = t;
        t = y1; y1 = y2; y2 = t;
    }
    int dx = x2 - x1;
    int dy = abs(y2 - y1);
    int err = dx / 2;
    int ystep = (y2 > y1) ? 1 : -1;
    for (int x = x1, y = y1; x <= x2; x++) {
        int px = steep ? y : x;
        int py = steep ? x : y;
        if (px >= 0 && px < w && py >= 0 && py < h) canvas_draw_dot(canvas, px, py);
        err -= dy;
        if (err < 0) {
            y += ystep;
            err += dx;
        }
    }
}

void lander_draw_rotated(
    Canvas* canvas, float cx, float cy, float angle, float thrust, float y_scale) {
    float s = sinf(angle);
    float c = cosf(angle);

/* floorf, not a plain (int) cast: truncation sends -0.5 to 0, which squashes
 * the sprite by a pixel as it crosses the left/top edge. Same result for
 * every on-screen (non-negative) coordinate. */
#define RX(lx, ly) ((int)floorf(cx + (lx) * c - (ly) * s))
#define RY(lx, ly) ((int)floorf(cy + ((lx) * s + (ly) * c) * y_scale))

    /* Body outline — four segments forming the rounded rectangle */
    draw_line_clipped(canvas, RX(-1, -3), RY(-1, -3), RX(+1, -3), RY(+1, -3));   // top
    draw_line_clipped(canvas, RX(-2, -2), RY(-2, -2), RX(-2, -1), RY(-2, -1));   // L side
    draw_line_clipped(canvas, RX(+2, -2), RY(+2, -2), RX(+2, -1), RY(+2, -1));   // R side
    draw_line_clipped(canvas, RX(-1,  0), RY(-1,  0), RX(+1,  0), RY(+1,  0));   // bottom

    /* Platform */
    draw_line_clipped(canvas,
        RX(-LANDER_HALF_W, LANDER_BODY_BOT), RY(-LANDER_HALF_W, LANDER_BODY_BOT),
        RX( LANDER_HALF_W, LANDER_BODY_BOT), RY( LANDER_HALF_W, LANDER_BODY_BOT));

    /* Vertical legs — platform corners straight down to foot height */
    draw_line_clipped(canvas,
        RX(-LANDER_HALF_W, LANDER_BODY_BOT), RY(-LANDER_HALF_W, LANDER_BODY_BOT),
        RX(-LANDER_HALF_W, LANDER_FOOT_DY),  RY(-LANDER_HALF_W, LANDER_FOOT_DY));
    draw_line_clipped(canvas,
        RX( LANDER_HALF_W, LANDER_BODY_BOT), RY( LANDER_HALF_W, LANDER_BODY_BOT),
        RX( LANDER_HALF_W, LANDER_FOOT_DY),  RY( LANDER_HALF_W, LANDER_FOOT_DY));

    /* 2-px feet — from leg base (HALF_W) outward to foot tip (FOOT_DX) */
    draw_line_clipped(canvas,
        RX(-LANDER_FOOT_DX, LANDER_FOOT_DY), RY(-LANDER_FOOT_DX, LANDER_FOOT_DY),
        RX(-LANDER_HALF_W,  LANDER_FOOT_DY), RY(-LANDER_HALF_W,  LANDER_FOOT_DY));
    draw_line_clipped(canvas,
        RX( LANDER_HALF_W,  LANDER_FOOT_DY), RY( LANDER_HALF_W,  LANDER_FOOT_DY),
        RX( LANDER_FOOT_DX, LANDER_FOOT_DY), RY( LANDER_FOOT_DX, LANDER_FOOT_DY));

    /* Thrust flame — inverted triangle from platform corners to a point
     * below. Attached at LANDER_BODY_BOT (= the platform). */
    if (thrust > 0.05f) {
        float flame_len = 2.0f + 4.0f * thrust;
        draw_line_clipped(canvas,
            RX(-1.5f, LANDER_BODY_BOT), RY(-1.5f, LANDER_BODY_BOT),
            RX(0, LANDER_BODY_BOT + flame_len), RY(0, LANDER_BODY_BOT + flame_len));
        draw_line_clipped(canvas,
            RX( 1.5f, LANDER_BODY_BOT), RY( 1.5f, LANDER_BODY_BOT),
            RX(0, LANDER_BODY_BOT + flame_len), RY(0, LANDER_BODY_BOT + flame_len));
    }

#undef RX
#undef RY
}
