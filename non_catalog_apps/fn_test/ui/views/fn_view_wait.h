/**
 * @file fn_view_wait.h
 * @brief Экран ожидания: анимация подключения ФН и текст состояния.
 */
#pragma once

#include <gui/view.h>

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FnWaitView FnWaitView;

typedef enum {
    FnWaitModeDetect,
    FnWaitModeRead,
    FnWaitModeFlash,
} FnWaitMode;

FnWaitView* fn_view_wait_alloc(void);
void fn_view_wait_free(FnWaitView* view);
View* fn_view_wait_get_view(FnWaitView* view);

void fn_view_wait_set(
    FnWaitView* view,
    FnWaitMode mode,
    const char* title,
    const char* subtitle,
    bool cancelable);

#ifdef __cplusplus
}
#endif
