#pragma once
#include "game.h"

#define FB_SIZE (SCR_W * SCR_H / 8)

// Draws the world, HUD, title, map and death screens into fb (XBM layout: 16 bytes per row,
// bit 0 = leftmost pixel, 1 = black). Menus and dialogue are drawn on top by the device with
// Flipper fonts.
void render(const Game* g, uint8_t* fb);

// 3x5 font helpers, also used by the device for a few labels.
void fb_text(uint8_t* fb, int x, int y, const char* s, int scale, bool outline);
int fb_text_w(const char* s, int scale);
