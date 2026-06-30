#pragma once
#include <furi.h>
#include <font/font.h>

#define LCD_WIDTH 128
#define LCD_HEIGHT 64

Canvas *lcd_get_canvas();
void lcd_init_canvas(Canvas *c);

void lcd_init();
void lcd_deinit();

// Framebuffer drawing functions
void lcd_draw_pixel(uint16_t x, uint16_t y, uint16_t color);
void lcd_fill(uint16_t color);
void lcd_blit(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t *buffer);
void lcd_blit_16bit(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint16_t *buffer);

// Shape drawing functions
void lcd_draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void lcd_draw_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);
void lcd_fill_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);
void lcd_draw_circle(uint16_t center_x, uint16_t center_y, uint16_t radius, uint16_t color);
void lcd_fill_circle(uint16_t center_x, uint16_t center_y, uint16_t radius, uint16_t color);
void lcd_fill_triangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3, uint16_t color);
void lcd_fill_round_rectangle(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t radius, uint16_t color);
void lcd_draw_triangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3, uint16_t color);

// Text rendering functions
void lcd_draw_char(uint16_t x, uint16_t y, char c, uint16_t color, FontSize size);
void lcd_draw_text(uint16_t x, uint16_t y, const char *text, uint16_t color, FontSize size);
