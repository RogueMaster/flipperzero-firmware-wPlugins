/**
 * @file fn_text_model.h
 * @brief Модель прокручиваемого текстового экрана: только данные, без GUI.
 *
 * Условие host-тестируемости: файл не включает gui/, furi и fn_view_*.
 */
#pragma once

#include "fn_ui.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Одна строка секции. */
typedef struct {
    char text[FN_TEXT_LINE_LEN];
    FnTextStyle emphasis;
} FnTextLine;

/** Секция с заголовком и строками. */
typedef struct {
    const char* title; /**< может быть NULL */
    FnTextLine lines[FN_TEXT_MAX_LINES];
    uint8_t line_count;
} FnTextSection;

/** Полная модель текстового экрана. */
typedef struct {
    FnTextSection sections[FN_TEXT_MAX_SECTIONS];
    uint8_t section_count;
} FnTextModel;

#ifdef __cplusplus
}
#endif
