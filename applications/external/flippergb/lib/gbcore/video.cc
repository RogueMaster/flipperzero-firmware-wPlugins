#include "video.h"

#include "gameboy.h"
#include "cpu.h"
#include "bitwise.h"

using bitwise::check_bit;

/* ---------------------------------------------------------- tile decode */

/* s_tile_lut[b] spreads the 8 bits of a tile-data byte into 8 2-bit pixel
 * fields, leftmost pixel (bit 7) in the LOWEST field -- the same LSB-first
 * packing the framebuffer uses, so whole bytes can be emitted directly.
 * X-flipped sprites bit-reverse the tile bytes first (cheaper than a
 * second 512-byte LUT: this binary is RAM on the Flipper). */
static u16 s_tile_lut[256];
static bool s_luts_ready = false;

static inline u8 bitrev8(u8 b) {
    b = (u8)((b >> 4) | (b << 4));
    b = (u8)(((b & 0xCC) >> 2) | ((b & 0x33) << 2));
    b = (u8)(((b & 0xAA) >> 1) | ((b & 0x55) << 1));
    return b;
}

Video::Video(Gameboy& inGb)
    : gb(inGb) {
    if(!s_luts_ready) {
        for(uint b = 0; b < 256; b++) {
            u16 fwd = 0;
            for(uint k = 0; k < 8; k++) {
                fwd |= (u16)(((b >> (7 - k)) & 1) << (2 * k));
            }
            s_tile_lut[b] = fwd;
        }
        s_luts_ready = true;
    }
}

/* Palette LUT: maps a packed 2bpp byte (4 pixels) through BGP in one
 * lookup. Rebuilt only when the game writes a new BGP value. */
__attribute__((optimize("Os"))) auto Video::bg_pal_lut() -> const u8* {
    u8 bgp = bg_palette.value();
    if(!bgp_lut_valid || bgp != bgp_lut_cached_for) {
        u8 shade[4] = {
            (u8)(bgp & 0x3),
            (u8)((bgp >> 2) & 0x3),
            (u8)((bgp >> 4) & 0x3),
            (u8)((bgp >> 6) & 0x3),
        };
        for(uint v = 0; v < 256; v++) {
            bgp_lut[v] = (u8)(
                shade[v & 3] | (shade[(v >> 2) & 3] << 2) | (shade[(v >> 4) & 3] << 4) |
                (shade[(v >> 6) & 3] << 6));
        }
        bgp_lut_cached_for = bgp;
        bgp_lut_valid = true;
    }
    return bgp_lut;
}

/* Video::tick is called once per emulated instruction; it lives as an
 * inline definition at the bottom of gameboy.h. Mode-transition work
 * (rendering, interrupts) stays out-of-line in this file. */

__attribute__((optimize("Os"))) void Video::mode_transition_vram_end() {
    current_mode = VideoMode::HBLANK;

    if(check_bit(lcd_status.value(), 3)) {
        gb.cpu.interrupt_flag.set_bit_to(1, true); /* hblank STAT */
    }

    lcd_status.set_bit_to(1, false);
    lcd_status.set_bit_to(0, false);
}

/* LY=LYC is evaluated when a line BEGINS (hardware: during mode 2 / OAM
 * scan). Firing it at the end of the line -- as upstream did -- made
 * games' raster effects (dmg-acid2 uses LYC IRQs to change WX/LCDC on
 * exact rows) apply one line late. */
__attribute__((optimize("Os"))) void Video::check_lyc() {
    bool eq = ly_compare.value() == line.value();
    if(eq && check_bit(lcd_status.value(), 6)) {
        gb.cpu.interrupt_flag.set_bit_to(1, true);
    }
    lcd_status.set_bit_to(2, eq);
}

__attribute__((optimize("Os"))) void Video::mode_transition_hblank_end() {
    if(!skip_render) write_scanline(line.value());
    line.increment();
    check_lyc(); /* new line starts here */

    /* Line 145 (index 144) is the first line of VBLANK */
    if(line == 144) {
        current_mode = VideoMode::VBLANK;
        lcd_status.set_bit_to(1, false);
        lcd_status.set_bit_to(0, true);
        gb.cpu.interrupt_flag.set_bit_to(0, true);
    } else {
        lcd_status.set_bit_to(1, true);
        lcd_status.set_bit_to(0, false);
        current_mode = VideoMode::ACCESS_OAM;
    }
}

__attribute__((optimize("Os"))) void Video::mode_transition_vblank_line() {
    line.increment();

    /* LY=LYC also fires for vblank lines 145-153 (games wait on it).
     * Line 154 is transient (LY wraps to 0) and never matches. */
    if(line.value() < 154) check_lyc();

    /* Line 155 (index 154) is the last line */
    if(line == 154) {
        if(!skip_render) {
            draw();
            buffer.reset();
        } else {
            draw(); /* still notify the frontend for pacing */
        }
        line.reset();
        window_line = 0; /* the window's internal line counter is per-frame */
        check_lyc(); /* line 0 begins */
        current_mode = VideoMode::ACCESS_OAM;
        lcd_status.set_bit_to(1, true);
        lcd_status.set_bit_to(0, false);
    }
}

auto Video::display_enabled() const -> bool {
    return check_bit(control_byte, 7);
}
auto Video::window_tile_map() const -> bool {
    return check_bit(control_byte, 6);
}
auto Video::window_enabled() const -> bool {
    return check_bit(control_byte, 5);
}
auto Video::bg_window_tile_data() const -> bool {
    return check_bit(control_byte, 4);
}
auto Video::bg_tile_map_display() const -> bool {
    return check_bit(control_byte, 3);
}
auto Video::sprite_size() const -> bool {
    return check_bit(control_byte, 2);
}
auto Video::sprites_enabled() const -> bool {
    return check_bit(control_byte, 1);
}
auto Video::bg_enabled() const -> bool {
    return check_bit(control_byte, 0);
}

void Video::write_scanline(u8 current_line) {
    if(!display_enabled()) {
        return;
    }

    /* Lines the frontend never displays are not worth rendering */
    if(row_mask && current_line < GAMEBOY_HEIGHT && !row_mask[current_line]) {
        return;
    }

    if(bg_enabled()) {
        draw_bg_line(current_line);
    }

    if(window_enabled()) {
        draw_window_line(current_line);
    }

    /* Sprites are composited per scanline like hardware: mid-frame raster
     * changes to OBP/LCDC (object size/enable) apply to the correct rows
     * (dmg-acid2 exercises this), and only displayed lines pay the cost. */
    if(sprites_enabled()) {
        draw_sprites_line(current_line);
    }
}

/* Byte-oriented tile-strip renderer: emits `count` pixels of one tile-map
 * row into the packed framebuffer row `dst_row`, starting at screen pixel
 * `dst_x`, sourcing map pixels from `src_px` (wraps at 256). Pixels flow
 * through a small bit-queue so the bulk of the line is written as whole
 * palette-mapped bytes (4 px per store) instead of per-pixel RMW packing:
 * ~7x fewer operations per line than the old per-pixel path. */
void Video::render_strip(
    u8* dst_row,
    uint dst_x,
    uint count,
    uint map_row_base, /* VRAM offset of the tile map row (32 entries) */
    uint src_px,
    uint tile_line_off, /* 2 * (row within the tile) */
    bool use_tile_set_zero,
    const u8* pal_lut) {
    uint tile_x = (src_px >> 3) & 31;
    uint fine = src_px & 7;
    uint tset_base = use_tile_set_zero ? 0x0000u : 0x0800u;

    /* bit-queue of pending 2bpp pixels, LSB = next pixel to emit */
    u32 acc = 0;
    uint nbits = 0;

#define FETCH_TILE_ROW() \
    do { \
        u8 tid = video_ram[map_row_base + tile_x]; \
        tile_x = (tile_x + 1) & 31; \
        uint toff = use_tile_set_zero ? \
                        (uint)tid * TILE_BYTES : \
                        (uint)((s8)tid + 128) * TILE_BYTES; \
        uint la = tset_base + toff + tile_line_off; \
        u16 v = (u16)(s_tile_lut[video_ram[la]] | (s_tile_lut[video_ram[la + 1]] << 1)); \
        acc |= (u32)v << nbits; \
        nbits += 16; \
    } while(0)

    /* prime the queue, discarding the fine-scroll pixels */
    FETCH_TILE_ROW();
    acc >>= fine * 2;
    nbits -= fine * 2;

    /* head: single pixels until the destination is byte-aligned */
    while(count && (dst_x & 3)) {
        if(nbits < 2) FETCH_TILE_ROW();
        uint sh = (dst_x & 3) * 2;
        u8* p = dst_row + (dst_x >> 2);
        *p = (u8)((*p & ~(3u << sh)) | ((pal_lut[acc & 3] & 3u) << sh));
        acc >>= 2;
        nbits -= 2;
        dst_x++;
        count--;
    }

    /* body: whole bytes, palette applied 4 pixels at a time */
    u8* out = dst_row + (dst_x >> 2);
    while(count >= 4) {
        if(nbits < 8) FETCH_TILE_ROW();
        *out++ = pal_lut[acc & 0xFF];
        acc >>= 8;
        nbits -= 8;
        count -= 4;
        dst_x += 4;
    }

    /* tail */
    while(count) {
        if(nbits < 2) FETCH_TILE_ROW();
        uint sh = (dst_x & 3) * 2;
        u8* p = dst_row + (dst_x >> 2);
        *p = (u8)((*p & ~(3u << sh)) | ((pal_lut[acc & 3] & 3u) << sh));
        acc >>= 2;
        nbits -= 2;
        dst_x++;
        count--;
    }
#undef FETCH_TILE_ROW
}

void Video::draw_bg_line(uint current_line) {
    /* Note: tileset two uses signed numbering to share half the tiles with
     * tileset one */
    bool use_tile_set_zero = bg_window_tile_data();
    bool use_tile_map_zero = !bg_tile_map_display();

    u8* dst = buffer.row_ptr(current_line);
    if(!dst) return;

    uint scrolled_y = (current_line + scroll_y.value()) % BG_MAP_SIZE;
    uint map_row_base = (use_tile_map_zero ? (TILE_MAP_ZERO_ADDRESS - 0x8000) :
                                             (TILE_MAP_ONE_ADDRESS - 0x8000)) +
                        (scrolled_y / TILE_HEIGHT_PX) * TILES_PER_LINE;

    render_strip(
        dst,
        0,
        GAMEBOY_WIDTH,
        map_row_base,
        scroll_x.value(),
        (scrolled_y % TILE_HEIGHT_PX) * 2,
        use_tile_set_zero,
        bg_pal_lut());
}

void Video::draw_window_line(uint current_line) {
    bool use_tile_set_zero = bg_window_tile_data();
    bool use_tile_map_zero = !window_tile_map();

    /* the window only starts once LY reaches WY */
    if(current_line < window_y.value()) return;

    /* WX off-screen hides the window for this line WITHOUT advancing its
     * internal line counter: when it reappears, drawing resumes from the
     * next window row, not from LY-WY (dmg-acid2 "chin" behaviour) */
    uint wx = window_x.value();
    if(wx > 166) return;
    uint start_x = wx >= 7 ? wx - 7 : 0;
    if(start_x >= GAMEBOY_WIDTH) return;

    uint src_row = window_line; /* internal per-frame counter */
    if(src_row >= 256) return;
    window_line++; /* the counter advances on every line the window shows */

    u8* dst = buffer.row_ptr(current_line);
    if(!dst) return; /* row not displayed: counter still advanced above */

    uint map_row_base = (use_tile_map_zero ? (TILE_MAP_ZERO_ADDRESS - 0x8000) :
                                             (TILE_MAP_ONE_ADDRESS - 0x8000)) +
                        ((src_row / TILE_HEIGHT_PX) & 31) * TILES_PER_LINE;

    render_strip(
        dst,
        start_x,
        GAMEBOY_WIDTH - start_x,
        map_row_base,
        wx >= 7 ? 0 : 7 - wx,
        (src_row % TILE_HEIGHT_PX) * 2,
        use_tile_set_zero,
        bg_pal_lut());
}

void Video::draw_sprites_line(uint current_line) {
    uint sprite_height = sprite_size() ? 16 : 8;

    /* hardware OAM scan: the first 10 sprites (in OAM order) covering
     * this line are selected */
    u8 sel[10];
    uint nsel = 0;
    for(uint n = 0; n < 40 && nsel < 10; n++) {
        int sy = (int)gb.mmu.oam_ram[n * 4] - 16;
        if((int)current_line >= sy && (int)current_line < sy + (int)sprite_height) {
            sel[nsel++] = (u8)n;
        }
    }
    if(nsel == 0) return;

    u8* row = buffer.row_ptr(current_line);
    if(!row) return;

    /* draw in reverse priority order so the winner is painted last:
     * lower X wins overlaps, ties broken by lower OAM index */
    for(uint i = 1; i < nsel; i++) { /* insertion sort by X descending */
        u8 v = sel[i];
        u8 vx = gb.mmu.oam_ram[v * 4 + 1];
        uint j = i;
        while(j > 0 && gb.mmu.oam_ram[sel[j - 1] * 4 + 1] < vx) {
            sel[j] = sel[j - 1];
            j--;
        }
        sel[j] = v;
    }

    for(uint i = 0; i < nsel; i++) {
        uint n = sel[i];
        u16 oam = (u16)(n * 4);
        u8 sprite_y = gb.mmu.oam_ram[oam];
        u8 sprite_x = gb.mmu.oam_ram[oam + 1];
        if(sprite_x == 0 || sprite_x >= 168) continue; /* off-screen */

        u8 pattern_n = gb.mmu.oam_ram[oam + 2];
        u8 attrs = gb.mmu.oam_ram[oam + 3];
        bool use_palette_1 = check_bit(attrs, 4);
        bool flip_x = check_bit(attrs, 5);
        bool flip_y = check_bit(attrs, 6);
        bool obj_behind_bg = check_bit(attrs, 7);

        /* in 8x16 mode the hardware ignores bit 0 of the tile id */
        if(sprite_height == 16) pattern_n &= 0xFE;

        u8 obp = use_palette_1 ? sprite_palette_1.value() : sprite_palette_0.value();
        u8 shade[4] = {
            (u8)(obp & 0x3),
            (u8)((obp >> 2) & 0x3),
            (u8)((obp >> 4) & 0x3),
            (u8)((obp >> 6) & 0x3),
        };

        uint y = current_line - (uint)(sprite_y - 16);
        uint src_y = flip_y ? sprite_height - 1 - y : y;
        uint line_addr = pattern_n * TILE_BYTES + src_y * 2;

        u8 b1 = video_ram[line_addr];
        u8 b2 = video_ram[line_addr + 1];
        if(flip_x) {
            b1 = bitrev8(b1);
            b2 = bitrev8(b2);
        }
        u16 v = (u16)(s_tile_lut[b1] | (s_tile_lut[b2] << 1));
        if(v == 0) continue; /* fully transparent row */

        int start_x = (int)sprite_x - 8;
        for(uint x = 0; x < TILE_WIDTH_PX; x++, v >>= 2) {
            uint color = v & 3;
            if(color == 0) continue; /* transparent */

            int screen_x = start_x + (int)x;
            if(screen_x < 0 || screen_x >= (int)GAMEBOY_WIDTH) continue;

            uint sh = ((uint)screen_x & 3) * 2;
            u8* p = row + ((uint)screen_x >> 2);

            /* same as upstream: priority compares the final shade */
            if(obj_behind_bg && ((*p >> sh) & 3) != SHADE_WHITE) continue;

            *p = (u8)((*p & ~(3u << sh)) | (shade[color] << sh));
        }
    }
}

void Video::draw() {
    if(vblank_callback) vblank_callback(vblank_ctx);
}
