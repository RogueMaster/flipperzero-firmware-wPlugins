#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <stdint.h>
#include <stdbool.h>

#define SH1106_WIDTH    128
#define SH1106_HEIGHT   64
#define SH1106_PAGES    (SH1106_HEIGHT / 8)
#define SH1106_BUF_SIZE (SH1106_WIDTH * SH1106_PAGES)

#define SH1106_ADDR_3C (0x3C << 1)
#define SH1106_ADDR_3D (0x3D << 1)

typedef struct {
    uint8_t buffer[SH1106_BUF_SIZE];
    uint8_t i2c_addr;
    bool initialized;
    bool flip_h;
} SH1106;

// Lifecycle
bool sh1106_detect(uint8_t addr);
bool sh1106_init(SH1106* d, uint8_t addr);
void sh1106_power(SH1106* d, bool on);

// Low-level
bool sh1106_cmd(SH1106* d, uint8_t cmd);
bool sh1106_cmd2(SH1106* d, uint8_t cmd, uint8_t arg);
void sh1106_flush(SH1106* d);

// Hardware commands
void sh1106_set_contrast(SH1106* d, uint8_t val);
void sh1106_set_invert(SH1106* d, bool invert);
void sh1106_set_all_on(SH1106* d, bool force);
void sh1106_set_flip_h(SH1106* d, bool flip);
void sh1106_set_flip_v(SH1106* d, bool flip);
void sh1106_set_offset(SH1106* d, uint8_t rows);
void sh1106_set_start_line(SH1106* d, uint8_t line);
void sh1106_set_clock(SH1106* d, uint8_t divide, uint8_t freq);
void sh1106_set_precharge(SH1106* d, uint8_t phase1, uint8_t phase2);
void sh1106_set_vcomh(SH1106* d, uint8_t level);
void sh1106_set_charge_pump(SH1106* d, bool enable);

// Scrolling
void sh1106_scroll_h(SH1106* d, bool left, uint8_t start_page, uint8_t end_page, uint8_t speed);
void sh1106_scroll_hv(
    SH1106* d,
    bool left,
    uint8_t start_page,
    uint8_t end_page,
    uint8_t speed,
    uint8_t v_offset);
void sh1106_scroll_step(SH1106* d, bool left);
void sh1106_scroll_stop(SH1106* d);

// Fade / Blink
void sh1106_set_fade_blink(SH1106* d, uint8_t mode);

// Drawing primitives
void sh1106_clear(SH1106* d);
void sh1106_fill(SH1106* d, uint8_t pattern);
void sh1106_pixel(SH1106* d, int16_t x, int16_t y, bool on);
void sh1106_line(SH1106* d, int16_t x0, int16_t y0, int16_t x1, int16_t y1);
void sh1106_rect(SH1106* d, int16_t x, int16_t y, int16_t w, int16_t h);
void sh1106_fill_rect(SH1106* d, int16_t x, int16_t y, int16_t w, int16_t h);
void sh1106_circle(SH1106* d, int16_t cx, int16_t cy, int16_t r);
void sh1106_char(SH1106* d, int16_t x, int16_t y, char c);
void sh1106_string(SH1106* d, int16_t x, int16_t y, const char* s);

// Large 7-segment style digits (20w x 28h pixels each)
void sh1106_big_digit(SH1106* d, int16_t x, int16_t y, uint8_t digit);
void sh1106_big_colon(SH1106* d, int16_t x, int16_t y, bool on);
void sh1106_battery_icon(SH1106* d, int16_t x, int16_t y, uint8_t pct, bool charging);
