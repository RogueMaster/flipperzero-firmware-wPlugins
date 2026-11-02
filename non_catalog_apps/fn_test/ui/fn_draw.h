/**
 * @file fn_draw.h
 * @brief Общие помощники отрисовки: встроенный шрифт и перенос строк.
 */
#pragma once

#include <gui/canvas.h>

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Установить основной шрифт с кириллицей. */
void fn_draw_primary(Canvas* canvas);

/** Установить мелкий шрифт с кириллицей. */
void fn_draw_small(Canvas* canvas);

/**
 * Нарисовать текст с переносом по словам.
 * @return высота нарисованного блока в пикселях
 */
uint16_t fn_draw_wrapped(
    Canvas* canvas,
    int32_t x,
    int32_t y_top,
    const char* text,
    int32_t max_width,
    uint8_t line_h,
    uint8_t max_lines);

/** Высота блока текста без отрисовки (для прокрутки). */
uint16_t fn_draw_measure(
    Canvas* canvas,
    const char* text,
    int32_t max_width,
    uint8_t line_h,
    uint8_t max_lines);

#ifdef __cplusplus
}
#endif
