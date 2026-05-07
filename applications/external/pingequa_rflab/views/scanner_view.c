/**
 * @file scanner_view.c
 * @brief Channel Scan 自定义 View — 专业 2.4 GHz 频谱风格 v3.5.
 *
 * 屏布局(128×64):
 *   y=0..7    Header (Secondary):  Ch042 2442 MHz                 N1234
 *   y=8       Divider 1
 *   y=9..12   Marker band 4px:
 *               · ▲ WiFi  5×4 上指三角  ch 12/37/62
 *               · ■ BLE   4×4 实心方块  ch 2/26/80
 *   y=13      Divider 2
 *   y=14..15  Cursor band 2px(专属缓冲):▼ 5×2
 *   y=16..50  Chart 35px(纯净:bars + 光标虚线,无干扰)
 *   y=51      Divider 3
 *   y=52..56  Gap 5px(显著区分分隔与信息栏)
 *   y=57..62  Footer (Secondary):  Pk@h   Cur:h    Dwl:Nus
 *   y=63      1px 边距
 *
 * Bar 缩放:auto-scale + EMA 平滑.smooth_scale 跟随 max(hits[]),
 *          最强通道总顶屏顶,其它按比例,无齐平.MIN_AUTOSCALE=8 防弱信号过放大.
 */
#include "scanner_view.h"

#include <furi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 几何 v3.6 — divider2 与 cursor 之间加 1px 间距(防边界 cursor 与 divider 视觉粘连) */
#define CHART_X         1

#define HDR_BASELINE    7
#define HDR_DIV1_Y      8

#define MARKER_TOP      9
#define MARKER_BOT      12
#define MARKER_DIV_Y    13
/* y=14: 1px 留白(防 cursor 边界与 divider2 粘连) */
#define CURSOR_BAND_TOP 15
#define CURSOR_BAND_BOT 16

#define CHART_Y_TOP     17
#define CHART_Y_FLOOR   51
#define CHART_H         (CHART_Y_FLOOR - CHART_Y_TOP) /* 34 px */

#define BAR_DIV_Y       52
#define FTR_BASELINE    62
/* y=53..56:gap 4px(原 5px,缩 1);y=57..62:footer;y=63:margin */

/* 数据 + 缩放 */
#define HITS_MAX        32
#define MIN_BAR_H       3
#define MIN_AUTOSCALE   8
#define DWELL_DEFAULT   150

/* 频段映射 */
#define WIFI_1_NRF      12
#define WIFI_6_NRF      37
#define WIFI_11_NRF     62
#define BLE_37_NRF      2
#define BLE_38_NRF      26
#define BLE_39_NRF      80

/* Footer slots — Dwl 右对齐适应文字宽度,Cur 左移给 Dwl 留空间.
 *   worst case: Pk 0..40, Cur 44..74, Dwl 77..127(变宽,最坏 "Dwl:2000us" 50px) */
#define SLOT_PK_X       0
#define SLOT_CUR_X      44
#define SLOT_DWL_R_X    127 /* 右对齐锚点 */

/* ---------------------------------------------------------------------------
 * Model
 * --------------------------------------------------------------------------*/

typedef struct {
    uint8_t hits[126];
    uint8_t cursor_ch;
    uint8_t peak_ch;
    uint16_t dwell_us;
    uint16_t sweep_count;
    bool running;
    bool auto_paused; /* true = MAX HOLD;false = 用户 PAUSED 或正在扫描 */
    uint8_t export_flash_ticks; /* 长按 OK 导出 CSV 后,顶栏显 "SAVED!" 倒计时
                                 * (sweep update 时 -1,每 ~30-50 ms 一次,初值 30
                                 * 大约 1-1.5 秒可见) */
} ScannerViewModel;

struct ScannerView {
    View* view;
};

/* ---------------------------------------------------------------------------
 * Marker / Cursor 绘制
 * --------------------------------------------------------------------------*/

/* WiFi ▲ 5×4 上指三角 — 顶部 1 像素尖,逐行加宽到 5 像素. */
static inline void draw_wifi_marker(Canvas* canvas, uint8_t nrf_ch) {
    int x = CHART_X + nrf_ch;
    canvas_draw_dot(canvas, x, MARKER_TOP);                                   /* y=9: 1 wide tip */
    canvas_draw_line(canvas, x - 1, MARKER_TOP + 1, x + 1, MARKER_TOP + 1);   /* y=10: 3 wide */
    canvas_draw_line(canvas, x - 2, MARKER_TOP + 2, x + 2, MARKER_TOP + 2);   /* y=11: 5 wide */
    canvas_draw_line(canvas, x - 2, MARKER_TOP + 3, x + 2, MARKER_TOP + 3);   /* y=12: 5 wide base */
}

/* BLE ■ 4×4 实心方块. */
static inline void draw_ble_marker(Canvas* canvas, uint8_t nrf_ch) {
    int x = CHART_X + nrf_ch;
    canvas_draw_box(canvas, x - 1, MARKER_TOP, 4, 4);                         /* x..x+3, y=9..12 */
}

/* ▼ Cursor 5×2 — 顶 5px / 底 3px;边界 clamp 防止画到 chart 范围之外造成"divider 加粗"错觉. */
static void draw_cursor(Canvas* canvas, uint8_t cursor_ch) {
    int cx = CHART_X + cursor_ch;
    const int x_min = CHART_X;
    const int x_max = CHART_X + 125; /* chart 最右像素 */

    /* 顶行 5px:clamp 到 [x_min, x_max] */
    int l1 = (cx - 2 < x_min) ? x_min : cx - 2;
    int r1 = (cx + 2 > x_max) ? x_max : cx + 2;
    canvas_draw_line(canvas, l1, CURSOR_BAND_TOP, r1, CURSOR_BAND_TOP);

    /* 底行 3px:clamp */
    int l2 = (cx - 1 < x_min) ? x_min : cx - 1;
    int r2 = (cx + 1 > x_max) ? x_max : cx + 1;
    canvas_draw_line(canvas, l2, CURSOR_BAND_BOT, r2, CURSOR_BAND_BOT);

    /* chart 内虚线竖线(从 cursor 三角下穿到柱区底). */
    for(int y = CHART_Y_TOP; y <= CHART_Y_FLOOR; y += 2) {
        canvas_draw_dot(canvas, cx, y);
    }
}

/* ---------------------------------------------------------------------------
 * Draw
 * --------------------------------------------------------------------------*/

static void scanner_view_draw_callback(Canvas* canvas, void* _model) {
    const ScannerViewModel* m = _model;
    canvas_clear(canvas);

    /* === Header === */
    canvas_set_font(canvas, FontSecondary);
    char buf[32];
    int freq_mhz = 2400 + m->cursor_ch;
    snprintf(buf, sizeof(buf), "Ch%03u  %d MHz", m->cursor_ch, freq_mhz);
    canvas_draw_str(canvas, 0, HDR_BASELINE, buf);

    /* 右上角状态指示器(替换 N count,5 char 内,不挡 bars). */
    const char* right_text;
    if(m->export_flash_ticks > 0) {
        right_text = "SAVED!"; /* 长按 OK 导出 CSV 成功后闪 ~1 秒 */
    } else if(m->auto_paused) {
        right_text = "MAX!";
    } else if(!m->running) {
        right_text = "PAUSE";
    } else {
        snprintf(buf, sizeof(buf), "N%u", (unsigned)m->sweep_count);
        right_text = buf;
    }
    canvas_draw_str_aligned(canvas, 127, HDR_BASELINE, AlignRight, AlignBottom, right_text);

    canvas_draw_line(canvas, 0, HDR_DIV1_Y, 127, HDR_DIV1_Y);

    /* === Marker band(顶部) === */
    draw_wifi_marker(canvas, WIFI_1_NRF);
    draw_wifi_marker(canvas, WIFI_6_NRF);
    draw_wifi_marker(canvas, WIFI_11_NRF);
    draw_ble_marker(canvas, BLE_37_NRF);
    draw_ble_marker(canvas, BLE_38_NRF);
    draw_ble_marker(canvas, BLE_39_NRF);

    canvas_draw_line(canvas, 0, MARKER_DIV_Y, 127, MARKER_DIV_Y);

    /* === Bars (直接线性映射 hit/HITS_MAX → CHART_H,max-hold 模式)=== */
    for(int ch = 0; ch < 126; ch++) {
        int hit = m->hits[ch];
        if(hit <= 0) continue;
        int h = (hit * CHART_H) / HITS_MAX;
        if(h < MIN_BAR_H) h = MIN_BAR_H;
        if(h > CHART_H) h = CHART_H;
        canvas_draw_line(
            canvas, CHART_X + ch, CHART_Y_FLOOR, CHART_X + ch, CHART_Y_FLOOR - h);
    }

    /* === Cursor (over bars) === */
    draw_cursor(canvas, m->cursor_ch);

    /* === Bottom divider === */
    canvas_draw_line(canvas, 0, BAR_DIV_Y, 127, BAR_DIV_Y);

    /* === Footer 自适应布局(动态测字宽,Cur 在 Pk 与 Dwl 之间居中)=== */
    char buf_pk[16], buf_cur[16], buf_dwl[16];
    snprintf(buf_pk, sizeof(buf_pk), "Pk%u@%u", m->peak_ch, m->hits[m->peak_ch]);
    snprintf(buf_cur, sizeof(buf_cur), "Cur:%u", m->hits[m->cursor_ch]);
    snprintf(buf_dwl, sizeof(buf_dwl), "Dwl:%uus", m->dwell_us);

    /* Pk 左固,Dwl 右固到屏最右 */
    canvas_draw_str(canvas, 0, FTR_BASELINE, buf_pk);
    canvas_draw_str_aligned(canvas, 127, FTR_BASELINE, AlignRight, AlignBottom, buf_dwl);

    /* Cur 在剩余空间居中.字宽用 canvas_string_width 实测,自动避让. */
    int pk_w = canvas_string_width(canvas, buf_pk);
    int dwl_w = canvas_string_width(canvas, buf_dwl);
    int cur_w = canvas_string_width(canvas, buf_cur);
    int avail_left = pk_w + 3;             /* 至少 3 px 间距给 Pk 让 */
    int avail_right = 127 - dwl_w - 2;     /* 至少 2 px 间距给 Dwl 让 */
    int cur_x = (avail_left + avail_right) / 2 - cur_w / 2;
    if(cur_x < avail_left) cur_x = avail_left; /* 极端窄时贴左边 */
    canvas_draw_str(canvas, cur_x, FTR_BASELINE, buf_cur);

    /* 不再有中央 PAUSED/MAX HOLD 覆盖层 — 状态指示已在顶栏右上角. */
}

/* ---------------------------------------------------------------------------
 * Lifecycle
 * --------------------------------------------------------------------------*/

ScannerView* scanner_view_alloc(void) {
    ScannerView* sv = malloc(sizeof(*sv));
    furi_check(sv);
    sv->view = view_alloc();
    view_allocate_model(sv->view, ViewModelTypeLocking, sizeof(ScannerViewModel));
    view_set_draw_callback(sv->view, scanner_view_draw_callback);

    with_view_model(
        sv->view,
        ScannerViewModel * m,
        {
            m->cursor_ch = 0;
            m->peak_ch = 0;
            m->dwell_us = DWELL_DEFAULT;
            m->sweep_count = 0;
            m->running = true;
            m->auto_paused = false;
            m->export_flash_ticks = 0;
        },
        false);

    return sv;
}

void scanner_view_free(ScannerView* sv) {
    if(sv == NULL) return;
    view_free(sv->view);
    free(sv);
}

View* scanner_view_get_view(ScannerView* sv) {
    furi_check(sv);
    return sv->view;
}

/* ---------------------------------------------------------------------------
 * 模型更新
 * --------------------------------------------------------------------------*/

void scanner_view_update_sweep(
    ScannerView* sv,
    const uint8_t hits[126],
    uint8_t peak_ch,
    uint16_t sweep_count) {
    furi_check(sv);
    with_view_model(
        sv->view,
        ScannerViewModel * m,
        {
            memcpy(m->hits, hits, 126);
            m->peak_ch = peak_ch;
            m->sweep_count = sweep_count;
            /* 衰减 SAVED! 闪烁倒计时(每 sweep ~30-50ms,30 滴答 ≈ 1-1.5 秒). */
            if(m->export_flash_ticks > 0) m->export_flash_ticks--;
        },
        true);
}

void scanner_view_set_cursor(ScannerView* sv, uint8_t cursor_ch) {
    furi_check(sv);
    if(cursor_ch > 125) cursor_ch = 125;
    with_view_model(sv->view, ScannerViewModel * m, { m->cursor_ch = cursor_ch; }, true);
}

void scanner_view_set_dwell(ScannerView* sv, uint16_t dwell_us) {
    furi_check(sv);
    with_view_model(sv->view, ScannerViewModel * m, { m->dwell_us = dwell_us; }, true);
}

void scanner_view_set_running(ScannerView* sv, bool running) {
    furi_check(sv);
    with_view_model(sv->view, ScannerViewModel * m, { m->running = running; }, true);
}

void scanner_view_set_auto_paused(ScannerView* sv, bool auto_paused) {
    furi_check(sv);
    with_view_model(sv->view, ScannerViewModel * m, { m->auto_paused = auto_paused; }, true);
}

void scanner_view_show_export_flash(ScannerView* sv) {
    furi_check(sv);
    /* 30 滴答 ≈ 1-1.5 秒可见(由 update_sweep 衰减). */
    with_view_model(sv->view, ScannerViewModel * m, { m->export_flash_ticks = 30; }, true);
}
