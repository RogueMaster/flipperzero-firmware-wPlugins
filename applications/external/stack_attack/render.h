// Draws the game into a 128x64 1-bit framebuffer in XBM layout (LSB = leftmost pixel),
// ready for canvas_draw_xbm. No Flipper headers, so the host preview can use it too.
#pragma once

#include "game.h"

#include <stdint.h>

#define FB_W      128
#define FB_H      64
#define FB_STRIDE (FB_W / 8)
#define FB_SIZE   (FB_STRIDE * FB_H)

void fb_clear(uint8_t* fb);
void fb_pixel(uint8_t* fb, int x, int y, bool black);
void fb_text(uint8_t* fb, int x, int y, const char* s);
void fb_rect(uint8_t* fb, int x, int y, int w, int h, bool black);

void render_game(uint8_t* fb, const Game* g, uint32_t hiscore);
void render_title(uint8_t* fb);
