/**
  @file   render.cpp
  @author apfxtech
  @brief  1bpp framebuffer drawing on top of the Flipper canvas

  Mystic Balloon, Flipper Zero port

  apfxtech, 2026, license: MIT (see LICENSE)

  SPDX-License-Identifier: MIT
*/

#include "render.h"

#include <furi.h>
#include <string.h>

static uint8_t gfx_fb[GFX_SIZE];

static inline void gfx_apply(int16_t x, int16_t y, uint8_t value, uint8_t mask) {
    if(mask == 0 || (uint16_t)x >= GFX_WIDTH) return;

    const int16_t row = (int16_t)(y >> 3);
    const uint8_t shift = (uint8_t)(y & 7);

    if(shift == 0) {
        if((uint16_t)row < GFX_PAGES) {
            uint8_t* dst = &gfx_fb[row * GFX_WIDTH + x];
            *dst = (uint8_t)((*dst & (uint8_t)~mask) | (value & mask));
        }
        return;
    }

    const uint8_t low_mask = (uint8_t)(mask << shift);
    if(low_mask && (uint16_t)row < GFX_PAGES) {
        const uint8_t low_value = (uint8_t)(value << shift);
        uint8_t* dst = &gfx_fb[row * GFX_WIDTH + x];
        *dst = (uint8_t)((*dst & (uint8_t)~low_mask) | (low_value & low_mask));
    }

    const uint8_t high_mask = (uint8_t)(mask >> (8 - shift));
    const int16_t next_row = (int16_t)(row + 1);
    if(high_mask && (uint16_t)next_row < GFX_PAGES) {
        const uint8_t high_value = (uint8_t)(value >> (8 - shift));
        uint8_t* dst = &gfx_fb[next_row * GFX_WIDTH + x];
        *dst = (uint8_t)((*dst & (uint8_t)~high_mask) | (high_value & high_mask));
    }
}

void gfx_clear(GfxColor color) {
    memset(gfx_fb, color == GfxDark ? 0xFF : 0x00, GFX_SIZE);
}

void gfx_present(Canvas* canvas) {
    uint8_t* dst = canvas_get_buffer(canvas);
    const size_t size = canvas_get_buffer_size(canvas);
    memcpy(dst, gfx_fb, size < GFX_SIZE ? size : GFX_SIZE);
}

static inline uint8_t gfx_page_mask(int16_t page, int16_t pages, int16_t h) {
    if(page + 1 < pages) return 0xFF;
    const uint8_t rest = (uint8_t)(h & 7);
    return rest ? (uint8_t)((1u << rest) - 1) : 0xFF;
}

typedef enum {
    GfxBlitOverwrite,
    GfxBlitSelfMasked,
    GfxBlitErase,
} GfxBlitMode;

static void gfx_blit(int16_t x, int16_t y, const uint8_t* sheet, uint8_t frame, GfxBlitMode mode) {
    if(!sheet) return;

    const int16_t w = (int16_t)sheet[0];
    const int16_t h = (int16_t)sheet[1];
    if(w <= 0 || h <= 0) return;
    if(x + w <= 0 || x >= GFX_WIDTH || y + h <= 0 || y >= GFX_HEIGHT) return;

    const int16_t pages = (int16_t)((h + 7) >> 3);
    const uint8_t* data = sheet + 2 + (int32_t)frame * w * pages;

    for(int16_t page = 0; page < pages; page++) {
        const uint8_t page_mask = gfx_page_mask(page, pages, h);
        const uint8_t* column = data + (int32_t)page * w;
        const int16_t page_y = (int16_t)(y + page * 8);

        for(int16_t c = 0; c < w; c++) {
            const uint8_t bits = column[c];

            switch(mode) {
            case GfxBlitOverwrite:
                gfx_apply((int16_t)(x + c), page_y, (uint8_t)~bits, page_mask);
                break;
            case GfxBlitSelfMasked:
                gfx_apply((int16_t)(x + c), page_y, 0x00, (uint8_t)(bits & page_mask));
                break;
            case GfxBlitErase:
                gfx_apply((int16_t)(x + c), page_y, 0xFF, (uint8_t)(bits & page_mask));
                break;
            }
        }
    }
}

void gfx_sprite_overwrite(int16_t x, int16_t y, const uint8_t* sheet, uint8_t frame) {
    gfx_blit(x, y, sheet, frame, GfxBlitOverwrite);
}

void gfx_sprite_self_masked(int16_t x, int16_t y, const uint8_t* sheet, uint8_t frame) {
    gfx_blit(x, y, sheet, frame, GfxBlitSelfMasked);
}

void gfx_sprite_erase(int16_t x, int16_t y, const uint8_t* sheet, uint8_t frame) {
    gfx_blit(x, y, sheet, frame, GfxBlitErase);
}

void gfx_sprite_plus_mask(int16_t x, int16_t y, const uint8_t* sheet, uint8_t frame) {
    if(!sheet) return;

    const int16_t w = (int16_t)sheet[0];
    const int16_t h = (int16_t)sheet[1];
    if(w <= 0 || h <= 0) return;
    if(x + w <= 0 || x >= GFX_WIDTH || y + h <= 0 || y >= GFX_HEIGHT) return;

    const int16_t pages = (int16_t)((h + 7) >> 3);
    const uint8_t* data = sheet + 2 + (int32_t)frame * w * pages * 2;

    for(int16_t page = 0; page < pages; page++) {
        const uint8_t page_mask = gfx_page_mask(page, pages, h);
        const uint8_t* column = data + (int32_t)page * w * 2;
        const int16_t page_y = (int16_t)(y + page * 8);

        for(int16_t c = 0; c < w; c++) {
            const uint8_t image = column[c * 2];
            const uint8_t mask = column[c * 2 + 1];
            gfx_apply((int16_t)(x + c), page_y, (uint8_t)~image, (uint8_t)(mask & page_mask));
        }
    }
}
