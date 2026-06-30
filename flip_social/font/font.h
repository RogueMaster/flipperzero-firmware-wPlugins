#pragma once
#include <stdint.h>
#include <gui/view.h>
#include <gui/elements.h>
#ifdef __cplusplus
extern "C"
{
#endif
    typedef enum
    {
        FONT_NOT_SET = 0,
        FONT_SIZE_SMALL = 1,
        FONT_SIZE_MEDIUM = 2,
        FONT_SIZE_LARGE = 3,
        FONT_SIZE_XLARGE = 4
    } FontSize;
    extern size_t canvas_current_font_height_custom(Canvas *canvas);
    extern void canvas_draw_str_multi(Canvas *canvas, uint8_t x, uint8_t y, const char *str);
    extern bool canvas_set_font_custom(Canvas *canvas, FontSize font_size);
    extern uint16_t canvas_string_width_custom(Canvas *canvas, const char *str);
#ifdef __cplusplus
}
#endif