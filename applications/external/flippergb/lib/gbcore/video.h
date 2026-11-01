#pragma once

#include "framebuffer.h"
#include "address.h"
#include "register.h"
#include "definitions.h"

class Gameboy;

using vblank_callback_t = void (*)(void* ctx);

enum class VideoMode {
    ACCESS_OAM,
    ACCESS_VRAM,
    HBLANK,
    VBLANK,
};

class Video {
public:
    Video(Gameboy& inGb);

    void tick(Cycles cycles);

    void register_vblank_callback(vblank_callback_t cb, void* ctx) {
        vblank_callback = cb;
        vblank_ctx = ctx;
    }

    auto vram_read(u16 offset) const -> u8 { return video_ram[offset]; }
    auto vram_ptr() -> u8* { return video_ram; } /* save states */
    void vram_write(u16 offset, u8 value) { video_ram[offset] = value; }

    /* When true, scanline/sprite rendering work is skipped (frame skip);
     * timing, interrupts and register behaviour are unaffected. */
    bool skip_render = false;

    /* Optional per-scanline render mask (GAMEBOY_HEIGHT entries, 0 = the
     * frontend never displays this line so its pixels are not rendered).
     * Purely a display optimization: timing/interrupts are unaffected.
     * nullptr (default) renders every line. On the Flipper only 64 of the
     * 144 lines survive the downscale, so ~55% of PPU work is skipped. */
    const u8* row_mask = nullptr;

    void set_row_mask(const u8* mask) {
        row_mask = mask;
        buffer.set_row_map(mask); /* compact storage to the visible rows */
    }

    auto get_framebuffer() const -> const FrameBuffer& { return buffer; }

    u8 control_byte = 0;

    ByteRegister lcd_control;
    ByteRegister lcd_status;

    ByteRegister scroll_y;
    ByteRegister scroll_x;

    ByteRegister line; /* LY */
    ByteRegister ly_compare;

    ByteRegister window_y;
    ByteRegister window_x; /* Note: x - 7 */

    ByteRegister bg_palette;
    ByteRegister sprite_palette_0; /* OBP0 */
    ByteRegister sprite_palette_1; /* OBP1 */

    ByteRegister dma_transfer; /* DMA */

private:
    void write_scanline(u8 current_line);
    void draw_sprites_line(uint current_line);
    void draw();
    void mode_transition_vram_end();
    void check_lyc();
    void mode_transition_hblank_end();
    void mode_transition_vblank_line();
    void draw_bg_line(uint current_line);
    void draw_window_line(uint current_line);
    void render_strip(
        u8* dst_row,
        uint dst_x,
        uint count,
        uint map_row_base,
        uint src_px,
        uint tile_line_off,
        bool use_tile_set_zero,
        const u8* pal_lut);
    auto bg_pal_lut() -> const u8*;

    static auto is_on_screen(int x, int y) -> bool {
        return x >= 0 && y >= 0 && x < static_cast<int>(GAMEBOY_WIDTH) &&
               y < static_cast<int>(GAMEBOY_HEIGHT);
    }

    auto display_enabled() const -> bool;
    auto window_tile_map() const -> bool;
    auto window_enabled() const -> bool;
    auto bg_window_tile_data() const -> bool;
    auto bg_tile_map_display() const -> bool;
    auto sprite_size() const -> bool;
    auto sprites_enabled() const -> bool;
    auto bg_enabled() const -> bool;

    Gameboy& gb;

    FrameBuffer buffer;

    /* packed-byte -> palette-mapped-byte LUT for the current BGP value */
    u8 bgp_lut[256];
    u8 bgp_lut_cached_for = 0;
    bool bgp_lut_valid = false;

    /* window internal line counter (per frame, advances only on lines
     * where the window is actually shown) */
    uint window_line = 0;

public:
    /* LY as the CPU sees it (line 153 reads 0 for most of its duration) */
    auto ly_read() const -> u8 {
        u8 v = line.value();
        return v == 153 ? 0 : v;
    }
    /* save-state accessors */
    auto get_mode() const -> u8 { return (u8)current_mode; }
    auto get_cycle_counter() const -> uint { return cycle_counter; }
    void set_state(u8 ly, u8 mode, uint counter) {
        line.set(ly);
        current_mode = (VideoMode)(mode & 3);
        cycle_counter = counter;
    }

private:
    u8 video_ram[0x2000] = {}; /* DMG: 8 KB (was 16 KB upstream) */

    VideoMode current_mode = VideoMode::ACCESS_OAM;
    uint cycle_counter = 0;

    vblank_callback_t vblank_callback = nullptr;
    void* vblank_ctx = nullptr;
};

const uint TILES_PER_LINE = 32;
const uint TILE_HEIGHT_PX = 8;
const uint TILE_WIDTH_PX = 8;
const uint TILE_BYTES = 2 * 8;
const uint SPRITE_BYTES = 4;
const uint BG_MAP_SIZE = 256;

const u16 TILE_SET_ZERO_ADDRESS = 0x8000;
const u16 TILE_SET_ONE_ADDRESS = 0x8800;
const u16 TILE_MAP_ZERO_ADDRESS = 0x9800;
const u16 TILE_MAP_ONE_ADDRESS = 0x9C00;

/* All in machine cycles (M-cycles), matching the CPU cycle tables.
 *
 * IMPORTANT: upstream had these in T-cycles (204/80/172, 456 per scanline)
 * while the opcode tables count M-cycles (NOP = 1). The PPU counted M-cycles
 * against T-cycle constants, so every emulated frame burned 70224 M-cycles
 * of CPU emulation instead of the hardware-correct 17556: literally 4x the
 * work per frame (and 4x the timer interrupts per frame). A desktop CPU
 * hides that; on a 64 MHz Cortex-M4 it was the difference between slideshow
 * and playable. Real hardware: scanline = 456 T-cycles = 114 M-cycles. */
const uint CLOCKS_PER_HBLANK = 51; /* Mode 0: 204 T-cycles */
const uint CLOCKS_PER_SCANLINE_OAM = 20; /* Mode 2: 80 T-cycles */
const uint CLOCKS_PER_SCANLINE_VRAM = 43; /* Mode 3: 172 T-cycles */
const uint CLOCKS_PER_SCANLINE =
    (CLOCKS_PER_SCANLINE_OAM + CLOCKS_PER_SCANLINE_VRAM + CLOCKS_PER_HBLANK);

const uint CLOCKS_PER_VBLANK = 1140; /* Mode 1: 4560 T-cycles */
const uint SCANLINES_PER_FRAME = 144;
const uint CLOCKS_PER_FRAME = (CLOCKS_PER_SCANLINE * SCANLINES_PER_FRAME) + CLOCKS_PER_VBLANK;
