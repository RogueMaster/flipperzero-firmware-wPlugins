#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <storage/storage.h>
#include <stream/file_stream.h>

#include "../utils/unicode_utils.h"

typedef enum {
    FontSizeNone,
    FontSize12px,
    FontSize16px,
    FontSize24px,

    FontSizeMAX,
} CFontSize;

typedef struct {
    Storage* storage;
    Stream* gbk_font_file;
    Stream* asc_font_file;
    Stream* uni2gbk_db_file;

    uint8_t dot_buffer[80];

    CFontSize font_size;
    FuriMutex* mutex;
} CFont;

typedef struct {
    uint8_t px;
    uint8_t spacing;
    uint8_t size;
    uint8_t min_width;
    char gbk_font_file_name[20];
    char asc_font_file_name[20];
} CFontInfo_t;

extern CFont* cfont;

void cfont_alloc(CFontSize size);
void cfont_free();

CFontInfo_t cfont_get_info(CFontSize size);

uint8_t cfont_get_glyph_asc(uint8_t asc, uint8_t* dot_matrix);
uint8_t cfont_get_glyph_gbk(uint16_t gbk, uint8_t* dot_matrix);
uint8_t cfont_get_glyph_utf16(Unicode uni, uint8_t* dot_matrix);

uint8_t cfont_get_utf16_glyph_width(const Unicode uni);
uint8_t cfont_get_utf16_str_width(const Unicode* uni);
void cfont_draw_utf16_str(Canvas* canvas, uint8_t x, uint8_t y, const Unicode* uni);
