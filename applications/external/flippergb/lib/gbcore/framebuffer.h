#pragma once

#include "definitions.h"

/* Packed 2 bits-per-pixel framebuffer (the original stored one 4-byte enum
 * per pixel = 92 KB).
 *
 * GB_FB_ROWS storage rows are kept (default: all 144 = 5.7 KB). The Flipper
 * build compiles with GB_FB_ROWS=64: only the 64 scanlines that survive the
 * 144 -> 64 downscale are stored (2.5 KB, saving 3.2 KB of always-resident
 * RAM). row_slot[] maps screen y -> storage slot; rows without a slot are
 * write-ignored / read-as-white, and the render row mask already prevents
 * the PPU from touching them anyway. */
#ifndef GB_FB_ROWS
#define GB_FB_ROWS GAMEBOY_HEIGHT
#endif

class FrameBuffer {
public:
    FrameBuffer() {
        for(uint y = 0; y < GAMEBOY_HEIGHT; y++)
            row_slot[y] = (y < GB_FB_ROWS) ? static_cast<u8>(y) : NO_ROW;
    }

    /* Compact storage to exactly the rows enabled in mask. Storage slots
     * are assigned in ascending y order, so the frontend's n-th displayed
     * row lives in slot n. No-op when every row fits (host build). */
    void set_row_map(const u8* mask) {
        if(!mask || GB_FB_ROWS >= GAMEBOY_HEIGHT) return;
        uint slot = 0;
        for(uint y = 0; y < GAMEBOY_HEIGHT; y++) {
            if(mask[y] && slot < GB_FB_ROWS)
                row_slot[y] = static_cast<u8>(slot++);
            else
                row_slot[y] = NO_ROW;
        }
    }

    void set_pixel(uint x, uint y, Shade shade) {
        uint slot = row_slot[y];
        if(slot == NO_ROW) return;
        uint i = slot * GAMEBOY_WIDTH + x;
        uint byte = i >> 2;
        uint shift = (i & 3) * 2;
        buf[byte] = static_cast<u8>((buf[byte] & ~(0x3 << shift)) | (shade << shift));
    }

    auto get_pixel(uint x, uint y) const -> Shade {
        uint slot = row_slot[y];
        if(slot == NO_ROW) return 0;
        uint i = slot * GAMEBOY_WIDTH + x;
        return static_cast<Shade>((buf[i >> 2] >> ((i & 3) * 2)) & 0x3);
    }

    void reset() {
        for(uint i = 0; i < sizeof(buf); i++)
            buf[i] = 0;
    }

    /* Raw packed 2bpp storage (4 pixels per byte, LSB first), slot-major:
     * storage slot s starts at bit offset s * GAMEBOY_WIDTH * 2. */
    auto raw() const -> const u8* { return buf; }

    /* Direct pointer to the packed bytes of screen row y (40 bytes,
     * 4 pixels each, LSB-first) or nullptr if the row has no storage.
     * Used by the byte-oriented renderer in video.cc. */
    auto row_ptr(uint y) -> u8* {
        uint slot = row_slot[y];
        if(slot == NO_ROW) return nullptr;
        return buf + slot * (GAMEBOY_WIDTH / 4);
    }

private:
    static const u8 NO_ROW = 0xFF;

    u8 buf[GAMEBOY_WIDTH * GB_FB_ROWS / 4] = {};
    u8 row_slot[GAMEBOY_HEIGHT];
};
