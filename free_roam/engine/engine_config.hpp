#pragma once

// memory
#define ENGINE_MEM_INCLUDE "furi.h"
#define ENGINE_MEM_NEW new
#define ENGINE_MEM_DELETE delete
#define ENGINE_MEM_MALLOC malloc
#define ENGINE_MEM_FREE free

// delay
#define ENGINE_DELAY_INCLUDE "furi.h"
#define ENGINE_DELAY_MS(ms) furi_delay_ms(ms)

// font
#define ENGINE_FONT_INCLUDE "font/font.h"
#define ENGINE_FONT_SIZE FontSize
#define ENGINE_FONT_DEFAULT FONT_SIZE_SMALL

// LCD
#define ENGINE_LCD_INCLUDE "game/lcd.hpp"
#define ENGINE_LCD_INIT lcd_init
// #define ENGINE_LCD_DEINIT lcd_deinit
#define ENGINE_LCD_WIDTH LCD_WIDTH
#define ENGINE_LCD_HEIGHT LCD_HEIGHT
#define ENGINE_LCD_CHAR lcd_draw_char
#define ENGINE_LCD_CIRCLE lcd_draw_circle
#define ENGINE_LCD_CLEAR lcd_fill
#define ENGINE_LCD_FILL_CIRCLE lcd_fill_circle
#define ENGINE_LCD_FILL_RECTANGLE lcd_fill_rect
#define ENGINE_LCD_FILL_ROUND_RECTANGLE lcd_fill_round_rectangle
#define ENGINE_LCD_FILL_TRIANGLE lcd_fill_triangle
#define ENGINE_LCD_BLIT lcd_blit
#define ENGINE_LCD_BLIT_16BIT lcd_blit_16bit
#define ENGINE_LCD_LINE lcd_draw_line
#define ENGINE_LCD_PIXEL lcd_draw_pixel
#define ENGINE_LCD_RECTANGLE lcd_draw_rect
// #define ENGINE_LCD_SWAP lcd_swap
#define ENGINE_LCD_TEXT lcd_draw_text
#define ENGINE_LCD_TRIANGLE lcd_draw_triangle

// storage
#define ENGINE_STORAGE_INCLUDE "game/storage.h"
#define ENGINE_STORAGE_READ storage_read // (const char *file_path, void *buffer, size_t buffer_size)