/**
 * @file about_view.h
 * @brief About 屏自定义 View —— 三页:品牌+QR / GitHub+QR / Legal.规范 §6.2 + §12.
 *
 * 三页静态信息,Up/Down 翻页(由 about_scene 的 input_callback 驱动),Back 退场.
 * 无 worker、无芯片访问.右边缘 1px 轨道 + 3px thumb 进度条指示当前页.
 *
 * 模型只有一个 uint8_t page,只被 GUI 线程读写 → ViewModelTypeLockFree.
 */
#pragma once

#include <gui/view.h>
#include <stdint.h>

#define ABOUT_PAGE_COUNT 3

typedef struct AboutView AboutView;

AboutView* about_view_alloc(void);
void about_view_free(AboutView* av);
View* about_view_get_view(AboutView* av);

/** 设置当前页(0..ABOUT_PAGE_COUNT-1).越界 clamp.scene on_enter 复位 0. */
void about_view_set_page(AboutView* av, uint8_t page);

/** 查询当前页 —— input_callback 翻页判边界用. */
uint8_t about_view_get_page(AboutView* av);
