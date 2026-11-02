#include "sh1106.h"
#include <string.h>

// 5x7 font, ASCII 32-126, each char is 5 bytes (columns, LSB = top row)
static const uint8_t font5x7[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, // 32 space
    0x00, 0x00, 0x5F, 0x00, 0x00, // 33 !
    0x00, 0x07, 0x00, 0x07, 0x00, // 34 "
    0x14, 0x7F, 0x14, 0x7F, 0x14, // 35 #
    0x24, 0x2A, 0x7F, 0x2A, 0x12, // 36 $
    0x23, 0x13, 0x08, 0x64, 0x62, // 37 %
    0x36, 0x49, 0x55, 0x22, 0x50, // 38 &
    0x00, 0x05, 0x03, 0x00, 0x00, // 39 '
    0x00, 0x1C, 0x22, 0x41, 0x00, // 40 (
    0x00, 0x41, 0x22, 0x1C, 0x00, // 41 )
    0x08, 0x2A, 0x1C, 0x2A, 0x08, // 42 *
    0x08, 0x08, 0x3E, 0x08, 0x08, // 43 +
    0x00, 0x50, 0x30, 0x00, 0x00, // 44 ,
    0x08, 0x08, 0x08, 0x08, 0x08, // 45 -
    0x00, 0x60, 0x60, 0x00, 0x00, // 46 .
    0x20, 0x10, 0x08, 0x04, 0x02, // 47 /
    0x3E, 0x51, 0x49, 0x45, 0x3E, // 48 0
    0x00, 0x42, 0x7F, 0x40, 0x00, // 49 1
    0x42, 0x61, 0x51, 0x49, 0x46, // 50 2
    0x21, 0x41, 0x45, 0x4B, 0x31, // 51 3
    0x18, 0x14, 0x12, 0x7F, 0x10, // 52 4
    0x27, 0x45, 0x45, 0x45, 0x39, // 53 5
    0x3C, 0x4A, 0x49, 0x49, 0x30, // 54 6
    0x01, 0x71, 0x09, 0x05, 0x03, // 55 7
    0x36, 0x49, 0x49, 0x49, 0x36, // 56 8
    0x06, 0x49, 0x49, 0x29, 0x1E, // 57 9
    0x00, 0x36, 0x36, 0x00, 0x00, // 58 :
    0x00, 0x56, 0x36, 0x00, 0x00, // 59 ;
    0x00, 0x08, 0x14, 0x22, 0x41, // 60 <
    0x14, 0x14, 0x14, 0x14, 0x14, // 61 =
    0x41, 0x22, 0x14, 0x08, 0x00, // 62 >
    0x02, 0x01, 0x51, 0x09, 0x06, // 63 ?
    0x32, 0x49, 0x79, 0x41, 0x3E, // 64 @
    0x7E, 0x11, 0x11, 0x11, 0x7E, // 65 A
    0x7F, 0x49, 0x49, 0x49, 0x36, // 66 B
    0x3E, 0x41, 0x41, 0x41, 0x22, // 67 C
    0x7F, 0x41, 0x41, 0x22, 0x1C, // 68 D
    0x7F, 0x49, 0x49, 0x49, 0x41, // 69 E
    0x7F, 0x09, 0x09, 0x01, 0x01, // 70 F
    0x3E, 0x41, 0x41, 0x51, 0x32, // 71 G
    0x7F, 0x08, 0x08, 0x08, 0x7F, // 72 H
    0x00, 0x41, 0x7F, 0x41, 0x00, // 73 I
    0x20, 0x40, 0x41, 0x3F, 0x01, // 74 J
    0x7F, 0x08, 0x14, 0x22, 0x41, // 75 K
    0x7F, 0x40, 0x40, 0x40, 0x40, // 76 L
    0x7F, 0x02, 0x04, 0x02, 0x7F, // 77 M
    0x7F, 0x04, 0x08, 0x10, 0x7F, // 78 N
    0x3E, 0x41, 0x41, 0x41, 0x3E, // 79 O
    0x7F, 0x09, 0x09, 0x09, 0x06, // 80 P
    0x3E, 0x41, 0x51, 0x21, 0x5E, // 81 Q
    0x7F, 0x09, 0x19, 0x29, 0x46, // 82 R
    0x46, 0x49, 0x49, 0x49, 0x31, // 83 S
    0x01, 0x01, 0x7F, 0x01, 0x01, // 84 T
    0x3F, 0x40, 0x40, 0x40, 0x3F, // 85 U
    0x1F, 0x20, 0x40, 0x20, 0x1F, // 86 V
    0x7F, 0x20, 0x18, 0x20, 0x7F, // 87 W
    0x63, 0x14, 0x08, 0x14, 0x63, // 88 X
    0x03, 0x04, 0x78, 0x04, 0x03, // 89 Y
    0x61, 0x51, 0x49, 0x45, 0x43, // 90 Z
    0x00, 0x00, 0x7F, 0x41, 0x41, // 91 [
    0x02, 0x04, 0x08, 0x10, 0x20, // 92 backslash
    0x41, 0x41, 0x7F, 0x00, 0x00, // 93 ]
    0x04, 0x02, 0x01, 0x02, 0x04, // 94 ^
    0x40, 0x40, 0x40, 0x40, 0x40, // 95 _
    0x00, 0x01, 0x02, 0x04, 0x00, // 96 `
    0x20, 0x54, 0x54, 0x54, 0x78, // 97 a
    0x7F, 0x48, 0x44, 0x44, 0x38, // 98 b
    0x38, 0x44, 0x44, 0x44, 0x20, // 99 c
    0x38, 0x44, 0x44, 0x48, 0x7F, // 100 d
    0x38, 0x54, 0x54, 0x54, 0x18, // 101 e
    0x08, 0x7E, 0x09, 0x01, 0x02, // 102 f
    0x08, 0x14, 0x54, 0x54, 0x3C, // 103 g
    0x7F, 0x08, 0x04, 0x04, 0x78, // 104 h
    0x00, 0x44, 0x7D, 0x40, 0x00, // 105 i
    0x20, 0x40, 0x44, 0x3D, 0x00, // 106 j
    0x00, 0x7F, 0x10, 0x28, 0x44, // 107 k
    0x00, 0x41, 0x7F, 0x40, 0x00, // 108 l
    0x7C, 0x04, 0x18, 0x04, 0x78, // 109 m
    0x7C, 0x08, 0x04, 0x04, 0x78, // 110 n
    0x38, 0x44, 0x44, 0x44, 0x38, // 111 o
    0x7C, 0x14, 0x14, 0x14, 0x08, // 112 p
    0x08, 0x14, 0x14, 0x18, 0x7C, // 113 q
    0x7C, 0x08, 0x04, 0x04, 0x08, // 114 r
    0x48, 0x54, 0x54, 0x54, 0x20, // 115 s
    0x04, 0x3F, 0x44, 0x40, 0x20, // 116 t
    0x3C, 0x40, 0x40, 0x20, 0x7C, // 117 u
    0x1C, 0x20, 0x40, 0x20, 0x1C, // 118 v
    0x3C, 0x40, 0x30, 0x40, 0x3C, // 119 w
    0x44, 0x28, 0x10, 0x28, 0x44, // 120 x
    0x0C, 0x50, 0x50, 0x50, 0x3C, // 121 y
    0x44, 0x64, 0x54, 0x4C, 0x44, // 122 z
    0x00, 0x08, 0x36, 0x41, 0x00, // 123 {
    0x00, 0x00, 0x7F, 0x00, 0x00, // 124 |
    0x00, 0x41, 0x36, 0x08, 0x00, // 125 }
    0x08, 0x04, 0x08, 0x10, 0x08, // 126 ~
};

static bool i2c_send(SH1106* d, const uint8_t* data, size_t len) {
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
    bool ok = furi_hal_i2c_tx(&furi_hal_i2c_handle_external, d->i2c_addr, data, len, 50);
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);
    return ok;
}

bool sh1106_cmd(SH1106* d, uint8_t cmd) {
    uint8_t buf[2] = {0x00, cmd};
    return i2c_send(d, buf, 2);
}

bool sh1106_cmd2(SH1106* d, uint8_t cmd, uint8_t arg) {
    uint8_t buf[3] = {0x00, cmd, arg};
    return i2c_send(d, buf, 3);
}

bool sh1106_detect(uint8_t addr) {
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
    bool found = furi_hal_i2c_is_device_ready(&furi_hal_i2c_handle_external, addr, 10);
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);
    return found;
}

bool sh1106_init(SH1106* d, uint8_t addr) {
    memset(d, 0, sizeof(SH1106));
    d->i2c_addr = addr;

    if(!sh1106_detect(addr)) return false;

    // SH1106 128x64 initialisation
    sh1106_cmd(d, 0xAE); // Display off
    sh1106_cmd2(d, 0xD5, 0x80); // Clock frequency
    sh1106_cmd2(d, 0xA8, 0x3F); // Multiplex 64
    sh1106_cmd2(d, 0xD3, 0x00); // Display offset
    sh1106_cmd(d, 0x40); // Start line 0

    // SH1106 DC-DC converter
    sh1106_cmd2(d, 0xAD, 0x8B);

    sh1106_cmd(d, 0xA1); // Segment remap
    sh1106_cmd(d, 0xC8); // COM scan direction
    sh1106_cmd2(d, 0xDA, 0x12); // COM configuration
    sh1106_cmd2(d, 0x81, 0x7F); // Contrast
    sh1106_cmd2(d, 0xD9, 0x22); // Precharge
    sh1106_cmd2(d, 0xDB, 0x35); // VCOM level
    sh1106_cmd(d, 0xA4); // Display RAM contents
    sh1106_cmd(d, 0xA6); // Normal display

    sh1106_clear(d);
    sh1106_flush(d);

    sh1106_cmd(d, 0xAF); // Display on
    d->initialized = true;
    return true;
}

void sh1106_power(SH1106* d, bool on) {
    sh1106_cmd(d, on ? 0xAF : 0xAE);
}

void sh1106_flush(SH1106* d) {
    // Typical SH1106 visible-area offset
    const uint8_t column_offset = 2;

    uint8_t page_buf[SH1106_WIDTH + 1];
    page_buf[0] = 0x40; // I2C data control byte

    for(uint8_t page = 0; page < SH1106_PAGES; page++) {
        // Select SH1106 page
        sh1106_cmd(d, 0xB0 | page);

        // Set column start position
        sh1106_cmd(d, 0x00 | (column_offset & 0x0F));
        sh1106_cmd(d, 0x10 | (column_offset >> 4));

        // Copy one page from framebuffer

        if(d->flip_h) {
            // Reverse the pixel columns within this page.
            for(uint16_t x = 0; x < SH1106_WIDTH; x++) {
                page_buf[x + 1] = d->buffer[page * SH1106_WIDTH + (SH1106_WIDTH - 1 - x)];
            }
        } else {
            memcpy(&page_buf[1], &d->buffer[page * SH1106_WIDTH], SH1106_WIDTH);
        }

        // Transfer 128 display bytes
        i2c_send(d, page_buf, sizeof(page_buf));
    }
}

void sh1106_set_contrast(SH1106* d, uint8_t val) {
    sh1106_cmd2(d, 0x81, val);
}

void sh1106_set_invert(SH1106* d, bool invert) {
    sh1106_cmd(d, invert ? 0xA7 : 0xA6);
}

void sh1106_set_all_on(SH1106* d, bool force) {
    sh1106_cmd(d, force ? 0xA5 : 0xA4);
}

void sh1106_set_flip_h(SH1106* d, bool flip) {
    // Software horizontal mirror.
    // Keep the controller's segment mapping unchanged.
    d->flip_h = flip;

    // Refresh immediately using the new orientation.
    sh1106_flush(d);
}

void sh1106_set_flip_v(SH1106* d, bool flip) {
    // C0 = normal, C8 = remapped
    // default init is C8, so "flip" means C0
    sh1106_cmd(d, flip ? 0xC0 : 0xC8);
}

void sh1106_set_offset(SH1106* d, uint8_t rows) {
    sh1106_cmd2(d, 0xD3, rows & 0x3F);
}

void sh1106_set_start_line(SH1106* d, uint8_t line) {
    sh1106_cmd(d, 0x40 | (line & 0x3F));
}

void sh1106_set_clock(SH1106* d, uint8_t divide, uint8_t freq) {
    sh1106_cmd2(d, 0xD5, ((freq & 0x0F) << 4) | (divide & 0x0F));
}

void sh1106_set_precharge(SH1106* d, uint8_t phase1, uint8_t phase2) {
    sh1106_cmd2(d, 0xD9, ((phase2 & 0x0F) << 4) | (phase1 & 0x0F));
}

void sh1106_set_vcomh(SH1106* d, uint8_t level) {
    sh1106_cmd2(d, 0xDB, level);
}

void sh1106_set_charge_pump(SH1106* d, bool enable) {
    // SH1106 DC-DC converter control
    sh1106_cmd2(d, 0xAD, enable ? 0x8B : 0x8A);
}

void sh1106_scroll_step(SH1106* d, bool left) {
    // Rotate each 128-byte page by one pixel column.
    // No unsupported controller commands are sent.

    for(uint8_t page = 0; page < SH1106_PAGES; page++) {
        uint8_t* row = &d->buffer[page * SH1106_WIDTH];
        uint8_t edge;

        if(left) {
            edge = row[0];
            memmove(row, row + 1, SH1106_WIDTH - 1);
            row[SH1106_WIDTH - 1] = edge;
        } else {
            edge = row[SH1106_WIDTH - 1];
            memmove(row + 1, row, SH1106_WIDTH - 1);
            row[0] = edge;
        }
    }

    sh1106_flush(d);
}

// Scrolling

void sh1106_scroll_h(SH1106* d, bool left, uint8_t start_page, uint8_t end_page, uint8_t speed) {
    UNUSED(d);
    UNUSED(left);
    UNUSED(start_page);
    UNUSED(end_page);
    UNUSED(speed);
}

void sh1106_scroll_hv(
    SH1106* d,
    bool left,
    uint8_t start_page,
    uint8_t end_page,
    uint8_t speed,
    uint8_t v_offset) {
    UNUSED(d);
    UNUSED(left);
    UNUSED(start_page);
    UNUSED(end_page);
    UNUSED(speed);
    UNUSED(v_offset);
}

void sh1106_scroll_stop(SH1106* d) {
    UNUSED(d);
}
// Fade / Blink (command 0x23)
// mode: 0=off, 1=fade slow, 2=fade fast, 3=blink slow, 4=blink fast
// Byte format: bits[7:6]=mode(2=fade,3=blink), bits[5:4]=interval, bits[3:0]=step
void sh1106_set_fade_blink(SH1106* d, uint8_t mode) {
    UNUSED(d);
    UNUSED(mode);
}

// Drawing

void sh1106_clear(SH1106* d) {
    memset(d->buffer, 0, SH1106_BUF_SIZE);
}

void sh1106_fill(SH1106* d, uint8_t pattern) {
    memset(d->buffer, pattern, SH1106_BUF_SIZE);
}

void sh1106_pixel(SH1106* d, int16_t x, int16_t y, bool on) {
    if(x < 0 || x >= SH1106_WIDTH || y < 0 || y >= SH1106_HEIGHT) return;
    uint16_t idx = (uint16_t)(x + (y / 8) * SH1106_WIDTH);
    if(on)
        d->buffer[idx] |= (1 << (y & 7));
    else
        d->buffer[idx] &= ~(1 << (y & 7));
}

void sh1106_line(SH1106* d, int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
    int16_t dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int16_t dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = dx - dy;
    for(;;) {
        sh1106_pixel(d, x0, y0, true);
        if(x0 == x1 && y0 == y1) break;
        int16_t e2 = err * 2;
        if(e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if(e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void sh1106_rect(SH1106* d, int16_t x, int16_t y, int16_t w, int16_t h) {
    sh1106_line(d, x, y, x + w - 1, y);
    sh1106_line(d, x, y + h - 1, x + w - 1, y + h - 1);
    sh1106_line(d, x, y, x, y + h - 1);
    sh1106_line(d, x + w - 1, y, x + w - 1, y + h - 1);
}

void sh1106_fill_rect(SH1106* d, int16_t x, int16_t y, int16_t w, int16_t h) {
    for(int16_t j = y; j < y + h; j++)
        for(int16_t i = x; i < x + w; i++)
            sh1106_pixel(d, i, j, true);
}

void sh1106_circle(SH1106* d, int16_t cx, int16_t cy, int16_t r) {
    int16_t x = r, y = 0, err = 1 - r;
    while(x >= y) {
        sh1106_pixel(d, cx + x, cy + y, true);
        sh1106_pixel(d, cx - x, cy + y, true);
        sh1106_pixel(d, cx + x, cy - y, true);
        sh1106_pixel(d, cx - x, cy - y, true);
        sh1106_pixel(d, cx + y, cy + x, true);
        sh1106_pixel(d, cx - y, cy + x, true);
        sh1106_pixel(d, cx + y, cy - x, true);
        sh1106_pixel(d, cx - y, cy - x, true);
        y++;
        if(err < 0) {
            err += 2 * y + 1;
        } else {
            x--;
            err += 2 * (y - x) + 1;
        }
    }
}

void sh1106_char(SH1106* d, int16_t x, int16_t y, char c) {
    if(c < 32 || c > 126) c = '?';
    const uint8_t* glyph = &font5x7[(c - 32) * 5];
    for(int8_t col = 0; col < 5; col++) {
        uint8_t bits = glyph[col];
        for(int8_t row = 0; row < 7; row++) {
            if(bits & (1 << row)) sh1106_pixel(d, x + col, y + row, true);
        }
    }
}

void sh1106_string(SH1106* d, int16_t x, int16_t y, const char* s) {
    while(*s) {
        sh1106_char(d, x, y, *s);
        x += 6; // 5px char + 1px gap
        s++;
    }
}

// 7-segment bitmasks: bits = gfedcba
static const uint8_t seg7_map[10] = {
    0x3F, // 0: abcdef
    0x06, // 1: bc
    0x5B, // 2: abdeg
    0x4F, // 3: abcdg
    0x66, // 4: bcfg
    0x6D, // 5: acdfg
    0x7D, // 6: acdefg
    0x07, // 7: abc
    0x7F, // 8: abcdefg
    0x6F, // 9: abcdfg
};

// Segment positions within a 20x28 digit cell
// Each segment: {x_offset, y_offset, width, height}
static const int8_t seg_pos[7][4] = {
    {3, 0, 14, 3}, // a: top horizontal
    {17, 2, 3, 12}, // b: top-right vertical
    {17, 15, 3, 12}, // c: bottom-right vertical
    {3, 25, 14, 3}, // d: bottom horizontal
    {0, 15, 3, 12}, // e: bottom-left vertical
    {0, 2, 3, 12}, // f: top-left vertical
    {3, 13, 14, 3}, // g: middle horizontal
};

void sh1106_big_digit(SH1106* d, int16_t x, int16_t y, uint8_t digit) {
    if(digit > 9) return;
    uint8_t segs = seg7_map[digit];
    for(int s = 0; s < 7; s++) {
        if(segs & (1 << s)) {
            sh1106_fill_rect(
                d, x + seg_pos[s][0], y + seg_pos[s][1], seg_pos[s][2], seg_pos[s][3]);
        }
    }
}

void sh1106_big_colon(SH1106* d, int16_t x, int16_t y, bool on) {
    if(!on) return;
    sh1106_fill_rect(d, x + 1, y + 7, 3, 3);
    sh1106_fill_rect(d, x + 1, y + 18, 3, 3);
}

void sh1106_battery_icon(SH1106* d, int16_t x, int16_t y, uint8_t pct, bool charging) {
    // 16x9 battery outline with nub on right
    sh1106_rect(d, x, y + 1, 13, 7); // body
    sh1106_fill_rect(d, x + 13, y + 3, 2, 3); // positive terminal nub
    // fill level (up to 9px wide inside the body)
    int fill = (pct * 9) / 100;
    if(fill > 0) sh1106_fill_rect(d, x + 2, y + 3, fill, 3);
    // charging bolt indicator
    if(charging) {
        sh1106_pixel(d, x + 7, y, true);
        sh1106_pixel(d, x + 6, y + 1, true);
        sh1106_pixel(d, x + 6, y + 2, true);
        sh1106_pixel(d, x + 8, y + 6, true);
        sh1106_pixel(d, x + 8, y + 7, true);
        sh1106_pixel(d, x + 9, y + 8, true);
    }
}
