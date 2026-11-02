/**
 * @file fn_font.h
 * @brief Доступ к встроенным шрифтам с кириллицей.
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Основной шрифт (u8g2-массив, высота строки ~9 px). */
const uint8_t* fn_font_primary(void);

/** Мелкий шрифт (u8g2-массив, высота строки ~7 px). */
const uint8_t* fn_font_small(void);

#ifdef __cplusplus
}
#endif
