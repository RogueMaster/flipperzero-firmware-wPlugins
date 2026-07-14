/**
 * @file splash_view.h
 * @brief 启动屏自定义 View(Logo + 进度条).规范 §6.2 + §12.
 *
 * Wrapper 模式:对外暴露不透明句柄 + getter,内部封装 Flipper View*.
 * 进度更新走 view model(Locking 类型)— 跨线程安全.
 */
#pragma once

#include <gui/view.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct SplashView SplashView;

/** 分配 splash view + 内部 Flipper View + 模型. */
SplashView* splash_view_alloc(void);

/** 释放(Flipper View 也会被一起 free). */
void splash_view_free(SplashView* sv);

/** 获取底层 View*,用于 view_dispatcher_add_view. */
View* splash_view_get_view(SplashView* sv);

/**
 * 设置进度条百分比(0..100).任意线程可调,view model 锁保护.
 * 自动触发 redraw.
 */
void splash_view_set_progress(SplashView* sv, uint8_t progress);
