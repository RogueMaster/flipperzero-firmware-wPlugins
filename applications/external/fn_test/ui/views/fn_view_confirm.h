/**
 * @file fn_view_confirm.h
 * @brief Экран подтверждения опасной операции.
 */
#pragma once

#include <gui/view.h>

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FnConfirmView FnConfirmView;

/** Callback выбора: true — подтверждено. */
typedef void (*FnConfirmCallback)(void* context, bool confirmed);

FnConfirmView* fn_view_confirm_alloc(void);
void fn_view_confirm_free(FnConfirmView* view);
View* fn_view_confirm_get_view(FnConfirmView* view);

void fn_view_confirm_set(FnConfirmView* view, const char* title, const char* body);
void fn_view_confirm_set_callback(FnConfirmView* view, FnConfirmCallback callback, void* context);

#ifdef __cplusplus
}
#endif
