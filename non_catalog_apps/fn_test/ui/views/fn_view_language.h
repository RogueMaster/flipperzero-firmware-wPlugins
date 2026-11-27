/**
 * @file fn_view_language.h
 * @brief Экран выбора языка интерфейса.
 */
#pragma once

#include "../../lib/fn/core/fn_lang.h"

#include <gui/view.h>

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FnLanguageView FnLanguageView;

/** Callback выбора: confirmed=false — пользователь вышел по Back. */
typedef void (*FnLanguageCallback)(void* context, FnLang lang, bool confirmed);

FnLanguageView* fn_view_language_alloc(void);
void fn_view_language_free(FnLanguageView* view);
View* fn_view_language_get_view(FnLanguageView* view);

/** Показать экран: текущий язык интерфейса и выбранный пункт. */
void fn_view_language_set(FnLanguageView* view, FnLang ui_lang, FnLang selected);
void fn_view_language_set_callback(
    FnLanguageView* view,
    FnLanguageCallback callback,
    void* context);

#ifdef __cplusplus
}
#endif
