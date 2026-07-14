#include "c_font.h"
#include <furi.h>
#include <lib/toolbox/path.h>
#include <storage/storage.h>
#include <gui/gui.h>

#define CFONT_FONTS_PATH    APP_ASSETS_PATH("fonts")
#define CFONT_RET_FLAG_NONE 0xFFFF
#define CEIL_DIV(a, b)      (a / b + (a % b != 0 ? 1 : 0))

#define TAG "CFont"

CFont* cfont = NULL;

const CFontInfo_t CFONT_INFO_TAB[] = {
    [FontSizeNone] = {},
    [FontSize12px] =
        {
            .px = 12,
            .gbk_font_file_name = "GB18030_12.bin",
            .asc_font_file_name = "ASC_12.bin",
            .size = 24,
            .spacing = 2,
            .min_width = 5,
        },
    [FontSize16px] =
        {
            .px = 16,
            .gbk_font_file_name = "GB18030_16.bin",
            .size = 32,
        },
    [FontSize24px] =
        {
            .gbk_font_file_name = "GB18030_24.bin",
            .size = 72,
            .px = 24,
        },
};

CFontInfo_t cfont_get_info(CFontSize size) {
    if(size < FontSizeMAX)
        return CFONT_INFO_TAB[size];
    else
        return CFONT_INFO_TAB[FontSizeNone];
}

static uint32_t cfont_gbk_to_offset(uint16_t gbk) {
    CFontInfo_t font_info = cfont_get_info(cfont->font_size);
    uint32_t offset = CFONT_RET_FLAG_NONE;

    uint8_t c0 = 0xff & gbk;
    uint8_t c1 = 0xff & (gbk >> 8);

    if(c0 >= 0xA1 && c0 <= 0xA9 && c1 >= 0xA1) {
        offset = (c0 - 0xA1) * 94 + (c1 - 0xA1);
    } else if(c0 >= 0xA8 && c0 <= 0xA9 && c1 < 0xA1) {
        if(c1 > 0x7F) c1 -= 1;
        offset = (c0 - 0xA8) * 96 + (c1 - 0x40) + 846;
    } else if(c0 >= 0xB0 && c0 <= 0xF7 && c1 >= 0xA1) {
        offset = (c0 - 0xB0) * 94 + (c1 - 0xA1) + 1038;
    } else if(c0 >= 0x81 && c0 < 0xA1 && c1 >= 0x40) {
        if(c1 > 0x7F) c1 -= 1;
        offset = (c0 - 0x81) * 190 + (c1 - 0x40) + 1038 + 6768;
    } else if(c0 >= 0xAA && c1 < 0xA1) {
        if(c1 > 0x7F) c1 -= 1;
        offset = (c0 - 0xAA) * 96 + (c1 - 0x40) + 1038 + 12848;
    }

    if(offset != CFONT_RET_FLAG_NONE) offset *= font_info.size;

    return offset;
}

static uint32_t cfont_asc_to_offset(uint8_t asc) {
    CFontInfo_t font_info = cfont_get_info(cfont->font_size);
    uint32_t offset = CFONT_RET_FLAG_NONE;

    if(asc >= 0x20 && asc <= 0x7E) {
        offset = asc - 0x20;
    }

    if(offset != CFONT_RET_FLAG_NONE) offset *= font_info.size;

    return offset;
}

bool cfont_unicode_to_gbk(uint16_t uni, uint16_t* gbk) {
    bool status = false;
    do {
        uint32_t offset = CFONT_RET_FLAG_NONE;

        if(uni >= 0x00A0 && uni <= 0x0451)
            offset = 0 + (uni - 0x00A0) * 2;
        else if(uni >= 0x2010 && uni <= 0x2642)
            offset = 1892 + (uni - 0x2010) * 2;
        else if(uni >= 0x3000 && uni <= 0x33D5)
            offset = 5066 + (uni - 0x3000) * 2;
        else if(uni >= 0x4E00 && uni <= 0x9FA5)
            offset = 7030 + (uni - 0x4E00) * 2;
        else if(uni >= 0xFE30 && uni <= 0xFE6B)
            offset = 48834 + (uni - 0xFE30) * 2;
        else if(uni >= 0xFF01 && uni <= 0xFF5E)
            offset = 48954 + (uni - 0xFF01) * 2;
        else if(uni >= 0xFFE0 && uni <= 0xFFE5)
            offset = 49142 + (uni - 0xFFE0) * 2;
        else if(uni >= 0xF92C && uni <= 0xFA29)
            offset = 49312 + (uni - 0xF92C) * 2;
        else if(uni >= 0xE816 && uni <= 0xE864)
            offset = 49820 + (uni - 0xE816) * 2;
        else if(uni >= 0x2E81 && uni <= 0x2ECA)
            offset = 49978 + (uni - 0x2E81) * 2;
        else if(uni >= 0x4947 && uni <= 0x49B7)
            offset = 50126 + (uni - 0x4947) * 2;
        else if(uni >= 0x4C77 && uni <= 0x4DAE)
            offset = 50352 + (uni - 0x4C77) * 2;
        else if(uni >= 0x3447 && uni <= 0x3CE0)
            offset = 50976 + (uni - 0x3447) * 2;
        else if(uni >= 0x4056 && uni <= 0x478D)
            offset = 55380 + (uni - 0x4056) * 2;

        if(offset == CFONT_RET_FLAG_NONE) break;
        if(!stream_seek(cfont->uni2gbk_db_file, offset, StreamOffsetFromStart)) break;
        if(stream_read(cfont->uni2gbk_db_file, (uint8_t*)gbk, 2) == 0) break;
        status = true;
    } while(false);
    return status;
}

uint8_t cfont_get_bytes_per_line(CFontInfo_t font_info) {
    uint8_t bytes_per_line = CEIL_DIV(font_info.px, 8);
    return bytes_per_line;
}

uint8_t cfont_get_min_width(uint8_t* dot_matrix) {
    CFontInfo_t font_info = cfont_get_info(cfont->font_size);
    uint8_t bytes_per_line = cfont_get_bytes_per_line(font_info);
    uint8_t width = font_info.px;

    uint8_t dot;
    for(dot = font_info.px; dot > 0; dot--) {
        if(width != font_info.px) break;
        for(uint8_t line = 0; line < font_info.px; line++) {
            if(*(dot_matrix + bytes_per_line * line + dot / 8) & (0x80 >> (dot % 8))) {
                width = dot;
                break;
            }
        }
    }
    if(dot == 0 && width == font_info.px) {
        // Zero width
        width = font_info.min_width;
    } else
        width += font_info.spacing;

    return width;
}

uint8_t cfont_get_glyph_gbk(uint16_t gbk, uint8_t* dot_matrix) {
    uint8_t width = 0;

    furi_check(furi_mutex_acquire(cfont->mutex, FuriWaitForever) == FuriStatusOk);

    do {
        if(cfont->font_size == FontSizeNone) break;
        CFontInfo_t font_info = cfont_get_info(cfont->font_size);
        uint32_t offset = cfont_gbk_to_offset(gbk);
        if(offset == CFONT_RET_FLAG_NONE) break;
        if(!stream_seek(cfont->gbk_font_file, offset, StreamOffsetFromStart)) break;
        if(stream_read(cfont->gbk_font_file, dot_matrix, font_info.size) == 0) break;
        width = cfont_get_min_width(dot_matrix);
    } while(false);

    furi_check(furi_mutex_release(cfont->mutex) == FuriStatusOk);

    return width;
}

uint8_t cfont_get_glyph_asc(uint8_t asc, uint8_t* dot_matrix) {
    uint8_t width = 0;

    furi_check(furi_mutex_acquire(cfont->mutex, FuriWaitForever) == FuriStatusOk);

    do {
        if(cfont->font_size == FontSizeNone) break;
        CFontInfo_t font_info = cfont_get_info(cfont->font_size);
        uint32_t offset = cfont_asc_to_offset(asc);
        if(offset == CFONT_RET_FLAG_NONE) break;
        if(!stream_seek(cfont->asc_font_file, offset, StreamOffsetFromStart)) break;
        if(stream_read(cfont->asc_font_file, dot_matrix, font_info.size) == 0) break;
        width = cfont_get_min_width(dot_matrix);
    } while(false);

    furi_check(furi_mutex_release(cfont->mutex) == FuriStatusOk);

    return width;
}

uint8_t cfont_get_glyph_utf16(Unicode uni, uint8_t* dot_matrix) {
    uint8_t width = 0;
    uint16_t code;

    if(cfont_unicode_to_gbk(uni, &code)) {
        width = cfont_get_glyph_gbk(code, dot_matrix);
    } else if(uni <= 0x7F) {
        width = cfont_get_glyph_asc(uni, dot_matrix);
    }

    return width;
}

void cfont_draw_dotmatrix(Canvas* canvas, uint8_t x, uint8_t y, uint8_t width, uint8_t* dot_matrix) {
    CFontInfo_t font_info = cfont_get_info(cfont->font_size);

    if(!width) return;

    uint8_t bytes_per_line = cfont_get_bytes_per_line(font_info);
    for(uint8_t line = 0; line < font_info.px; line++) {
        for(uint8_t dot = 0; dot < width; dot++) {
            if(*(dot_matrix + line * bytes_per_line + dot / 8) & (0x80 >> (dot % 8)))
                canvas_draw_dot(canvas, x + dot, y - font_info.px + line);
        }
    }
}

void cfont_draw_utf16_str(Canvas* canvas, uint8_t x, uint8_t y, const Unicode* uni) {
    uint8_t width;
    do {
        width = cfont_get_glyph_utf16(*uni, cfont->dot_buffer);
        cfont_draw_dotmatrix(canvas, x, y, width, cfont->dot_buffer);
        x += width;
    } while(*uni++ != '\0');
}

uint8_t cfont_get_utf16_glyph_width(const Unicode uni) {
    return cfont_get_glyph_utf16(uni, cfont->dot_buffer);
}

uint8_t cfont_get_utf16_str_width(const Unicode* uni) {
    uint8_t width = 0;

    do {
        width += cfont_get_utf16_glyph_width(*uni);
    } while(*uni++ != '\0');

    return width;
}

void cfont_alloc(CFontSize size) {
    cfont = malloc(sizeof(CFont));
    cfont->storage = furi_record_open(RECORD_STORAGE);
    cfont->gbk_font_file = file_stream_alloc(cfont->storage);
    cfont->asc_font_file = file_stream_alloc(cfont->storage);
    cfont->uni2gbk_db_file = file_stream_alloc(cfont->storage);
    cfont->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    cfont->font_size = size;

    FuriString* file_path = furi_string_alloc();
    path_concat(CFONT_FONTS_PATH, "UNI2GBK.bin", file_path);
    furi_check(
        file_stream_open(
            cfont->uni2gbk_db_file, furi_string_get_cstr(file_path), FSAM_READ, FSOM_OPEN_EXISTING),
        "missing UNI2GBK DB file!");

    CFontInfo_t font_info = cfont_get_info(cfont->font_size);

    // load GBK Font
    path_concat(CFONT_FONTS_PATH, font_info.gbk_font_file_name, file_path);
    furi_check(
        file_stream_open(
            cfont->gbk_font_file, furi_string_get_cstr(file_path), FSAM_READ, FSOM_OPEN_EXISTING),
        "missing CFont-GBK DB!");
    FURI_LOG_D(
        TAG, "Load CFont-GBK DB %upx path=%s", font_info.px, furi_string_get_cstr(file_path));

    // load Ascii Font
    path_concat(CFONT_FONTS_PATH, font_info.asc_font_file_name, file_path);
    furi_check(
        file_stream_open(
            cfont->asc_font_file, furi_string_get_cstr(file_path), FSAM_READ, FSOM_OPEN_EXISTING),
        "missing CFont-ASC DB!");
    FURI_LOG_D(TAG, "Load CFont-ASC %upx path=%s", font_info.px, furi_string_get_cstr(file_path));

    furi_string_free(file_path);
}

void cfont_free() {
    stream_free(cfont->gbk_font_file);
    stream_free(cfont->asc_font_file);
    stream_free(cfont->uni2gbk_db_file);

    furi_record_close(RECORD_STORAGE);
    furi_mutex_free(cfont->mutex);
    free(cfont);
}
