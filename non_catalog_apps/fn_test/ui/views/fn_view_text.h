/**
 * @file fn_view_text.h
 * @brief Прокручиваемый текстовый экран (информация, результат, справка).
 */
#pragma once

#include "../fn_text_model.h"

#include <gui/view.h>

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FnTextView FnTextView;

FnTextView* fn_view_text_alloc(void);
void fn_view_text_free(FnTextView* view);
View* fn_view_text_get_view(FnTextView* view);

/** Сбросить секции и картинку. */
void fn_view_text_clear(FnTextView* view);

/** Добавить секцию с заголовком (NULL — без заголовка). */
void fn_view_text_add_section(FnTextView* view, const char* title);

/** Добавить строку с форматированием; перенос выполняется при отрисовке. */
void fn_view_text_add_line(FnTextView* view, FnTextStyle style, const char* fmt, ...);

/** Завершить сборку: пересчитать высоту и сбросить прокрутку. */
void fn_view_text_commit(FnTextView* view);

/** Показать готовую модель (заголовки секций должны жить дольше модели). */
void fn_view_text_set_model(FnTextView* view, const FnTextModel* model);

/** Показать полноэкранную иконку (распиновка). */
void fn_view_text_set_image(FnTextView* view, const Icon* icon);

#ifdef __cplusplus
}
#endif
