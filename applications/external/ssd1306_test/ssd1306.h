#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <stdint.h>
#include <stdbool.h>

#define SSD1306_WIDTH 128
#define SSD1306_HEIGHT 64
#define SSD1306_PAGES (SSD1306_HEIGHT / 8)
#define SSD1306_BUF_SIZE (SSD1306_WIDTH * SSD1306_PAGES)

#define SSD1306_ADDR_3C (0x3C << 1)
#define SSD1306_ADDR_3D (0x3D << 1)

typedef struct {
    uint8_t buffer[SSD1306_BUF_SIZE];
    uint8_t i2c_addr;
    bool initialized;
} SSD1306;

// Lifecycle
bool ssd1306_detect(uint8_t addr);
bool ssd1306_init(SSD1306* d, uint8_t addr);
void ssd1306_power(SSD1306* d, bool on);

// Low-level
bool ssd1306_cmd(SSD1306* d, uint8_t cmd);
bool ssd1306_cmd2(SSD1306* d, uint8_t cmd, uint8_t arg);
void ssd1306_flush(SSD1306* d);

// Hardware commands
void ssd1306_set_contrast(SSD1306* d, uint8_t val);
void ssd1306_set_invert(SSD1306* d, bool invert);
void ssd1306_set_all_on(SSD1306* d, bool force);
void ssd1306_set_flip_h(SSD1306* d, bool flip);
void ssd1306_set_flip_v(SSD1306* d, bool flip);
void ssd1306_set_offset(SSD1306* d, uint8_t rows);
void ssd1306_set_start_line(SSD1306* d, uint8_t line);
void ssd1306_set_clock(SSD1306* d, uint8_t divide, uint8_t freq);
void ssd1306_set_precharge(SSD1306* d, uint8_t phase1, uint8_t phase2);
void ssd1306_set_vcomh(SSD1306* d, uint8_t level);
void ssd1306_set_charge_pump(SSD1306* d, bool enable);

// Scrolling
void ssd1306_scroll_h(SSD1306* d, bool left, uint8_t start_page, uint8_t end_page, uint8_t speed);
void ssd1306_scroll_hv(SSD1306* d, bool left, uint8_t start_page, uint8_t end_page, uint8_t speed, uint8_t v_offset);
void ssd1306_scroll_stop(SSD1306* d);

// Drawing primitives
void ssd1306_clear(SSD1306* d);
void ssd1306_fill(SSD1306* d, uint8_t pattern);
void ssd1306_pixel(SSD1306* d, int16_t x, int16_t y, bool on);
void ssd1306_line(SSD1306* d, int16_t x0, int16_t y0, int16_t x1, int16_t y1);
void ssd1306_rect(SSD1306* d, int16_t x, int16_t y, int16_t w, int16_t h);
void ssd1306_fill_rect(SSD1306* d, int16_t x, int16_t y, int16_t w, int16_t h);
void ssd1306_circle(SSD1306* d, int16_t cx, int16_t cy, int16_t r);
void ssd1306_char(SSD1306* d, int16_t x, int16_t y, char c);
void ssd1306_string(SSD1306* d, int16_t x, int16_t y, const char* s);

// Large 7-segment style digits (20w x 28h pixels each)
void ssd1306_big_digit(SSD1306* d, int16_t x, int16_t y, uint8_t digit);
void ssd1306_big_colon(SSD1306* d, int16_t x, int16_t y, bool on);
void ssd1306_battery_icon(SSD1306* d, int16_t x, int16_t y, uint8_t pct, bool charging);
