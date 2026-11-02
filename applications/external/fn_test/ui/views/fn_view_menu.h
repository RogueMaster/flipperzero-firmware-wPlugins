/**
 * @file fn_view_menu.h
 * @brief Главное меню: пять пунктов, собственная отрисовка.
 */
#pragma once

#include <gui/view.h>

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FnMenuView FnMenuView;

/** Callback выбора пункта: только отправка custom event, без сцен. */
typedef void (*FnMenuCallback)(void* context, uint8_t index);

FnMenuView* fn_view_menu_alloc(void);
void fn_view_menu_free(FnMenuView* view);
View* fn_view_menu_get_view(FnMenuView* view);

void fn_view_menu_set_callback(FnMenuView* view, FnMenuCallback callback, void* context);
void fn_view_menu_set_selected(FnMenuView* view, uint8_t index);
uint8_t fn_view_menu_get_selected(const FnMenuView* view);

#ifdef __cplusplus
}
#endif
