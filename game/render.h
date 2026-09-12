/**
  @file   render.h
  @author apfxtech
  @brief  1bpp framebuffer drawing on top of the Flipper canvas

  Mystic Balloon, Flipper Zero port

  apfxtech, 2026, license: MIT (see LICENSE)

  SPDX-License-Identifier: MIT
*/

#pragma once

#include <gui/gui.h>
#include <stdint.h>

#define GFX_WIDTH  128
#define GFX_HEIGHT 64
#define GFX_PAGES  (GFX_HEIGHT / 8)
#define GFX_SIZE   (GFX_WIDTH * GFX_PAGES)

typedef enum {
    GfxLight = 0,
    GfxDark = 1,
} GfxColor;

#ifdef __cplusplus
extern "C" {
#endif

void gfx_clear(GfxColor color);
void gfx_present(Canvas* canvas);

void gfx_sprite_overwrite(int16_t x, int16_t y, const uint8_t* sheet, uint8_t frame);
void gfx_sprite_self_masked(int16_t x, int16_t y, const uint8_t* sheet, uint8_t frame);
void gfx_sprite_erase(int16_t x, int16_t y, const uint8_t* sheet, uint8_t frame);
void gfx_sprite_plus_mask(int16_t x, int16_t y, const uint8_t* sheet, uint8_t frame);

#ifdef __cplusplus
}
#endif
