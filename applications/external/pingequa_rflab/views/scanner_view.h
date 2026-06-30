/**
 * @file scanner_view.h
 * @brief Channel Scan 自定义 View(126 通道 RPD 柱状图).规范 §6.2 + §12.
 *
 * 屏布局(128×64):
 *   y=0..11   顶栏 — 当前 ch / dwell / 峰值 ch / sweep 计数
 *   y=12..51  柱状图 — 每通道 1 px 宽,共 126 列居中(x=1..126)
 *   y=52..63  底栏 — 输入提示
 *
 * 模型用 ViewModelTypeLocking,worker 与 GUI 线程都能安全更新.
 */
#pragma once

#include <gui/view.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct ScannerView ScannerView;

ScannerView* scanner_view_alloc(void);
void scanner_view_free(ScannerView* sv);

/** 取底层 View*,用于 view_dispatcher_add_view + 注册 input_callback. */
View* scanner_view_get_view(ScannerView* sv);

/* ---- 模型更新 API(任意线程可调) ---- */

/** Worker 每 sweep 完成后调一次:批量刷新柱高 + 元数据.触发 redraw. */
void scanner_view_update_sweep(
    ScannerView* sv,
    const uint8_t hits[126],
    uint8_t peak_ch,
    uint16_t sweep_count);

/** 用户左右键移动光标后调用. */
void scanner_view_set_cursor(ScannerView* sv, uint8_t cursor_ch);

/** 用户上下键调 dwell 后调用. */
void scanner_view_set_dwell(ScannerView* sv, uint16_t dwell_us);

/** OK 键切暂停/恢复后调用. */
void scanner_view_set_running(ScannerView* sv, bool running);

/** worker 触发 max-hold 自动暂停时调用,影响 PAUSED 覆盖层文字("MAX HOLD"). */
void scanner_view_set_auto_paused(ScannerView* sv, bool auto_paused);

/** 长按 OK 触发 CSV 导出后调用 — 顶栏右上角 "SAVED!" 闪 ~1 秒(自动随 sweep
 * 更新衰减). */
void scanner_view_show_export_flash(ScannerView* sv);
