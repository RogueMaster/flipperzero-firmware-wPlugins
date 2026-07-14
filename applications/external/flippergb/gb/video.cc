#include "video.h"

#include "gameboy.h"
#include "cpu.h"
#include "bitwise.h"

using bitwise::check_bit;

Video::Video(Gameboy& inGb)
    : gb(inGb) {
}

void Video::tick(Cycles cycles) {
    cycle_counter += cycles.cycles;

    switch(current_mode) {
    case VideoMode::ACCESS_OAM:
        if(cycle_counter >= CLOCKS_PER_SCANLINE_OAM) {
            cycle_counter = cycle_counter % CLOCKS_PER_SCANLINE_OAM;
            lcd_status.set_bit_to(1, true);
            lcd_status.set_bit_to(0, true);
            current_mode = VideoMode::ACCESS_VRAM;
        }
        break;
    case VideoMode::ACCESS_VRAM:
        if(cycle_counter >= CLOCKS_PER_SCANLINE_VRAM) {
            cycle_counter = cycle_counter % CLOCKS_PER_SCANLINE_VRAM;
            current_mode = VideoMode::HBLANK;

            bool hblank_interrupt = check_bit(lcd_status.value(), 3);

            if(hblank_interrupt) {
                gb.cpu.interrupt_flag.set_bit_to(1, true);
            }

            bool ly_coincidence_interrupt = check_bit(lcd_status.value(), 6);
            bool ly_coincidence = ly_compare.value() == line.value();
            if(ly_coincidence_interrupt && ly_coincidence) {
                gb.cpu.interrupt_flag.set_bit_to(1, true);
            }
            lcd_status.set_bit_to(2, ly_coincidence);

            lcd_status.set_bit_to(1, false);
            lcd_status.set_bit_to(0, false);
        }
        break;
    case VideoMode::HBLANK:
        if(cycle_counter >= CLOCKS_PER_HBLANK) {
            if(!skip_render) write_scanline(line.value());
            line.increment();

            cycle_counter = cycle_counter % CLOCKS_PER_HBLANK;

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
        break;
    case VideoMode::VBLANK:
        if(cycle_counter >= CLOCKS_PER_SCANLINE) {
            line.increment();

            cycle_counter = cycle_counter % CLOCKS_PER_SCANLINE;

            /* Line 155 (index 154) is the last line */
            if(line == 154) {
                if(!skip_render) {
                    write_sprites();
                    draw();
                    buffer.reset();
                } else {
                    draw(); /* still notify the frontend for pacing */
                }
                line.reset();
                current_mode = VideoMode::ACCESS_OAM;
                lcd_status.set_bit_to(1, true);
                lcd_status.set_bit_to(0, false);
            };
        }
        break;
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
}

void Video::write_sprites() {
    if(!sprites_enabled()) {
        return;
    }

    for(uint sprite_n = 0; sprite_n < 40; sprite_n++) {
        draw_sprite(sprite_n);
    }
}

void Video::draw_bg_line(uint current_line) {
    /* Note: tileset two uses signed numbering to share half the tiles with
     * tileset one */
    bool use_tile_set_zero = bg_window_tile_data();
    bool use_tile_map_zero = !bg_tile_map_display();

    Palette palette = load_palette(bg_palette);

    u16 tile_set_address = use_tile_set_zero ? TILE_SET_ZERO_ADDRESS : TILE_SET_ONE_ADDRESS;
    u16 tile_map_address = use_tile_map_zero ? TILE_MAP_ZERO_ADDRESS : TILE_MAP_ONE_ADDRESS;

    uint screen_y = current_line;
    uint scrolled_y = (screen_y + scroll_y.value()) % BG_MAP_SIZE;
    uint tile_y = scrolled_y / TILE_HEIGHT_PX;
    uint tile_pixel_y = scrolled_y % TILE_HEIGHT_PX;
    uint tile_data_line_offset = tile_pixel_y * 2;

    /* Render tile-by-tile instead of refetching the tile data for every
     * pixel like upstream did */
    uint scroll_x_val = scroll_x.value();

    uint screen_x = 0;
    while(screen_x < GAMEBOY_WIDTH) {
        uint scrolled_x = (screen_x + scroll_x_val) % BG_MAP_SIZE;
        uint tile_x = scrolled_x / TILE_WIDTH_PX;
        uint tile_pixel_x = scrolled_x % TILE_WIDTH_PX;

        uint tile_index = tile_y * TILES_PER_LINE + tile_x;
        u8 tile_id = video_ram[tile_map_address - 0x8000 + tile_index];

        uint tile_data_mem_offset = use_tile_set_zero ?
                                        tile_id * TILE_BYTES :
                                        static_cast<uint>(
                                            (static_cast<s8>(tile_id) + 128)) *
                                            TILE_BYTES;

        uint line_addr = (tile_set_address - 0x8000) + tile_data_mem_offset +
                         tile_data_line_offset;

        u8 pixels_1 = video_ram[line_addr];
        u8 pixels_2 = video_ram[line_addr + 1];

        /* Draw the remainder of this tile's row */
        for(uint px = tile_pixel_x; px < TILE_WIDTH_PX && screen_x < GAMEBOY_WIDTH;
            px++, screen_x++) {
            u8 pixel_color = get_pixel_from_line(pixels_1, pixels_2, static_cast<u8>(px));
            buffer.set_pixel(screen_x, screen_y, get_shade_from_palette(pixel_color, palette));
        }
    }
}

void Video::draw_window_line(uint current_line) {
    bool use_tile_set_zero = bg_window_tile_data();
    bool use_tile_map_zero = !window_tile_map();

    Palette palette = load_palette(bg_palette);

    u16 tile_set_address = use_tile_set_zero ? TILE_SET_ZERO_ADDRESS : TILE_SET_ONE_ADDRESS;
    u16 tile_map_address = use_tile_map_zero ? TILE_MAP_ZERO_ADDRESS : TILE_MAP_ONE_ADDRESS;

    uint screen_y = current_line;
    uint scrolled_y = screen_y - window_y.value();

    if(scrolled_y >= GAMEBOY_HEIGHT) {
        return;
    }

    uint tile_y = scrolled_y / TILE_HEIGHT_PX;
    uint tile_pixel_y = scrolled_y % TILE_HEIGHT_PX;
    uint tile_data_line_offset = tile_pixel_y * 2;

    for(uint screen_x = 0; screen_x < GAMEBOY_WIDTH; screen_x++) {
        uint scrolled_x = screen_x + window_x.value() - 7;

        uint tile_x = scrolled_x / TILE_WIDTH_PX;
        uint tile_pixel_x = scrolled_x % TILE_WIDTH_PX;

        uint tile_index = tile_y * TILES_PER_LINE + tile_x;
        if(tile_index >= 32 * 32) continue;

        u8 tile_id = video_ram[tile_map_address - 0x8000 + tile_index];

        uint tile_data_mem_offset = use_tile_set_zero ?
                                        tile_id * TILE_BYTES :
                                        static_cast<uint>(
                                            (static_cast<s8>(tile_id) + 128)) *
                                            TILE_BYTES;

        uint line_addr = (tile_set_address - 0x8000) + tile_data_mem_offset +
                         tile_data_line_offset;

        u8 pixels_1 = video_ram[line_addr];
        u8 pixels_2 = video_ram[line_addr + 1];

        u8 pixel_color = get_pixel_from_line(pixels_1, pixels_2, static_cast<u8>(tile_pixel_x));
        buffer.set_pixel(screen_x, screen_y, get_shade_from_palette(pixel_color, palette));
    }
}

void Video::draw_sprite(const uint sprite_n) {
    /* Each sprite is represented by 4 bytes */
    u16 oam_start = static_cast<u16>(sprite_n * SPRITE_BYTES);

    u8 sprite_y = gb.mmu.oam_ram[oam_start];
    u8 sprite_x = gb.mmu.oam_ram[oam_start + 1];

    /* Offscreen sprites are not drawn */
    if(sprite_y == 0 || sprite_y >= 160) {
        return;
    }
    if(sprite_x == 0 || sprite_x >= 168) {
        return;
    }

    uint sprite_height = sprite_size() ? 16 : 8;

    u8 pattern_n = gb.mmu.oam_ram[oam_start + 2];
    u8 sprite_attrs = gb.mmu.oam_ram[oam_start + 3];

    /* Bits 0-3 are used only for CGB */
    bool use_palette_1 = check_bit(sprite_attrs, 4);
    bool flip_x = check_bit(sprite_attrs, 5);
    bool flip_y = check_bit(sprite_attrs, 6);
    bool obj_behind_bg = check_bit(sprite_attrs, 7);

    Palette palette = use_palette_1 ? load_palette(sprite_palette_1) :
                                      load_palette(sprite_palette_0);

    uint tile_offset = pattern_n * TILE_BYTES;

    int start_y = sprite_y - 16;
    int start_x = sprite_x - 8;

    for(uint y = 0; y < sprite_height; y++) {
        int screen_y = start_y + static_cast<int>(y);
        if(screen_y < 0 || screen_y >= static_cast<int>(GAMEBOY_HEIGHT)) continue;
        if(row_mask && !row_mask[screen_y]) continue;

        uint src_y = !flip_y ? y : sprite_height - y - 1;

        uint line_addr = tile_offset + src_y * 2; /* relative to tile set zero */
        u8 pixels_1 = video_ram[line_addr];
        u8 pixels_2 = video_ram[line_addr + 1];

        for(uint x = 0; x < TILE_WIDTH_PX; x++) {
            int screen_x = start_x + static_cast<int>(x);
            if(screen_x < 0 || screen_x >= static_cast<int>(GAMEBOY_WIDTH)) continue;

            uint src_x = !flip_x ? x : TILE_WIDTH_PX - x - 1;

            u8 gb_color = get_pixel_from_line(pixels_1, pixels_2, static_cast<u8>(src_x));

            /* Color 0 is transparent */
            if(gb_color == 0) {
                continue;
            }

            Shade existing_pixel = buffer.get_pixel(
                static_cast<uint>(screen_x), static_cast<uint>(screen_y));

            /* Note: same behaviour as upstream - compares the final shade
             * rather than the logical color 0 */
            if(obj_behind_bg && existing_pixel != SHADE_WHITE) {
                continue;
            }

            buffer.set_pixel(
                static_cast<uint>(screen_x),
                static_cast<uint>(screen_y),
                get_shade_from_palette(gb_color, palette));
        }
    }
}

auto Video::get_pixel_from_line(u8 byte1, u8 byte2, u8 pixel_index) -> u8 {
    using bitwise::bit_value;

    return static_cast<u8>(
        (bit_value(byte2, 7 - pixel_index) << 1) | bit_value(byte1, 7 - pixel_index));
}

auto Video::load_palette(const ByteRegister& palette_register) -> Palette {
    u8 v = palette_register.value();

    Palette palette;
    palette.color0 = static_cast<Shade>(v & 0x3);
    palette.color1 = static_cast<Shade>((v >> 2) & 0x3);
    palette.color2 = static_cast<Shade>((v >> 4) & 0x3);
    palette.color3 = static_cast<Shade>((v >> 6) & 0x3);
    return palette;
}

auto Video::get_shade_from_palette(u8 color, const Palette& palette) -> Shade {
    switch(color) {
    case 0:
        return palette.color0;
    case 1:
        return palette.color1;
    case 2:
        return palette.color2;
    default:
        return palette.color3;
    }
}

void Video::draw() {
    if(vblank_callback) vblank_callback(vblank_ctx);
}
