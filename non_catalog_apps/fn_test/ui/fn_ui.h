/**
 * @file fn_ui.h
 * @brief Константы верстки интерфейса 128x64 и стили текста.
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FN_UI_MARGIN_X    2
#define FN_UI_HEADER_H    12
#define FN_UI_CONTENT_Y   13
#define FN_UI_CONTENT_H   50
#define FN_UI_LINE_H      9
#define FN_UI_SMALL_H     7
#define FN_UI_SECTION_GAP 3
#define FN_UI_SCROLLBAR_W 3
#define FN_UI_TEXT_W      120

/* Секций на экране не больше четырёх (информация об ФН), запас — под жизнь,
 * результат и справку. Ограничение 6 выбрано по бюджету RAM (T085): модель
 * хранится дважды — в FNApp и в FnTextView. */
#define FN_TEXT_MAX_SECTIONS 6
#define FN_TEXT_MAX_LINES    12
/* Длина строки модели в БАЙТАХ UTF-8: кириллица занимает 2 байта на символ,
 * поэтому 72 байта — примерно 34 русских символа. Меньший буфер обрезал бы
 * обязательные поля (FR-025 запрещает потерю смысла при сокращении). */
#define FN_TEXT_LINE_LEN     72

/** Стиль строки в модели текстового экрана. */
typedef enum {
    FnTextStyleNormal = 0,
    FnTextStyleHeader,
    FnTextStyleWarning,
} FnTextStyle;

#ifdef __cplusplus
}
#endif
