/**
 * @file fn_lang.h
 * @brief Язык интерфейса приложения.
 *
 * Модуль чистого ядра: не включает furi/gui, пригоден для host-тестов.
 * Язык хранится в настройках приложения и передаётся явно во все функции,
 * которые возвращают пользовательский текст.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FnLangEn = 0, /**< английский (язык по умолчанию для каталога) */
    FnLangRu = 1, /**< русский */
} FnLang;

/** Язык по умолчанию: английский. */
#define FN_LANG_DEFAULT FnLangEn

#ifdef __cplusplus
}
#endif
