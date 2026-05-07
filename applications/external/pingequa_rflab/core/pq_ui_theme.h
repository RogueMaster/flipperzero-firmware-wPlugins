/**
 * @file pq_ui_theme.h
 * @brief 统一 UI 视觉风格的 Canvas 绘制 helper.规范 §5.6.
 *
 * 强制约束:所有函数必须在 GUI 线程的 view_draw_callback 内调用
 * (Canvas API 非线程安全).
 *
 * Flipper 屏幕 = 128 × 64.绝对坐标,左上原点.
 */
#pragma once

#include <gui/canvas.h>
#include <stdint.h>

/** 顶栏(高 ~12px):主字体绘标题 + 一条横线分隔正文. */
void pq_ui_draw_header(Canvas* canvas, const char* title);

/** 底栏(从 y=52 起):一条横线分隔 + 副字体绘提示. */
void pq_ui_draw_footer(Canvas* canvas, const char* hint);

/**
 * 启动屏:Logo 文字 + 进度条(0..100).
 * canvas_clear 由本函数完成,调用方无须自己清.
 */
void pq_ui_draw_splash(Canvas* canvas, uint8_t progress);
