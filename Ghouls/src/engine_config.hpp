#pragma once

// general
#define ENGINE_MAX_TRIANGLES_PER_SPRITE 64

// logging
// #define ENGINE_LOG_INCLUDE "furi.h"
#define ENGINE_LOG_INFO(...) // FURI_LOG_I("Ghouls", __VA_ARGS__) // (const char *format, ...) -> void

// memory
#define ENGINE_MEM_INCLUDE "stdlib.h"
#define ENGINE_MEM_NEW new
#define ENGINE_MEM_DELETE delete
#define ENGINE_MEM_MALLOC malloc
#define ENGINE_MEM_FREE free

// delay
// #define ENGINE_DELAY_INCLUDE "furi.h"
// #define ENGINE_DELAY_MS(ms) furi_delay_ms(ms) // (uint32_t ms) -> void

// font
// #define ENGINE_FONT_INCLUDE "font/font.h"
// #define ENGINE_FONT_SIZE FontSize
// #define ENGINE_FONT_DEFAULT FONT_SIZE_SMALL

// LCD
// #define ENGINE_LCD_INCLUDE "lcd.hpp"
// #define ENGINE_LCD_INIT lcd_init // () -> void
// #define ENGINE_LCD_DEINIT lcd_deinit                                    // () -> void
// #define ENGINE_LCD_WIDTH LCD_WIDTH
// #define ENGINE_LCD_HEIGHT LCD_HEIGHT
// #define ENGINE_LCD_CHAR lcd_draw_char                            // (uint16_t x, uint16_t y, char c, uint16_t color, FontSize size) -> void
// #define ENGINE_LCD_CIRCLE lcd_draw_circle                        // (uint16_t center_x, uint16_t center_y, uint16_t radius, uint16_t color) -> void
// #define ENGINE_LCD_CLEAR lcd_fill                                // (uint16_t color) -> void
// #define ENGINE_LCD_FILL_CIRCLE lcd_fill_circle                   // (uint16_t center_x, uint16_t center_y, uint16_t radius, uint16_t color) -> void
// #define ENGINE_LCD_FILL_RECTANGLE lcd_fill_rect                  // (uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color) -> void
// #define ENGINE_LCD_FILL_ROUND_RECTANGLE lcd_fill_round_rectangle // (uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t radius, uint16_t color) -> void
// #define ENGINE_LCD_FILL_TRIANGLE lcd_fill_triangle               // (uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3, uint16_t color) -> void
// #define ENGINE_LCD_BLIT lcd_blit                                 // (uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t *buffer) -> void
// #define ENGINE_LCD_BLIT_16BIT lcd_blit_16bit                     // (uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint16_t *buffer) -> void
// #define ENGINE_LCD_LINE lcd_draw_line                            // (uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color) -> void
// #define ENGINE_LCD_PIXEL lcd_draw_pixel                          // (uint16_t x, uint16_t y, uint16_t color) -> void
// #define ENGINE_LCD_RECTANGLE lcd_draw_rect                       // (uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color) -> void
// #define ENGINE_LCD_SWAP lcd_swap
// #define ENGINE_LCD_TEXT lcd_draw_text         // (uint16_t x, uint16_t y, const char *text, uint16_t color, FontSize size) -> void
// #define ENGINE_LCD_TRIANGLE lcd_draw_triangle // (uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3, uint16_t color) -> void

// storage
// #define ENGINE_STORAGE_INCLUDE "storage.h"
// #define ENGINE_STORAGE_READ storage_read // (const char *file_path, void *buffer, size_t buffer_size) -> size_t bytes_read