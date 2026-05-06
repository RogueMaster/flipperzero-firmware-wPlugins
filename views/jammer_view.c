/**
 * @file jammer_view.c
 * @brief NRF24 Jammer 自定义 View 实现 — 规范 §6.2 / §11.3 / §17.1 反模式 14.
 *
 * draw_callback 内只读 model + canvas 调用,无计算/分配/SPI(§17.1 反模式 14).
 *
 * 屏布局(128×64):
 *   y=0..10   Header FontPrimary "JAMMER" + FontSecondary status (baseline=10)
 *   y=12      Divider 1
 *   y=14..22  Mode FontPrimary "CW"/"SWEEP" + power FontSecondary (baseline=22)
 *   y=24..32  Channel FontPrimary "Ch 042"/"0-125" + sweep cursor (baseline=32)
 *   y=34..40  Freq FontSecondary (baseline=40)
 *   y=42..48  Band tag / chunks FontSecondary (baseline=48)
 *   y=51      Divider 2
 *   y=52..56  Gap 5 px
 *   y=57..62  Footer FontSecondary + 按键图标 (baseline=62)
 */
#include "jammer_view.h"

#include <furi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 频段标签 — NRF24 信道到 BLE/WiFi 的映射(channel = MHz - 2400). */
#define BLE_ADV_CH37_NRF 2  /* 2402 MHz */
#define BLE_ADV_CH38_NRF 26 /* 2426 MHz */
#define BLE_ADV_CH39_NRF 80 /* 2480 MHz */
#define WIFI_1_NRF       12 /* 2412 MHz */
#define WIFI_6_NRF       37 /* 2437 MHz */
#define WIFI_11_NRF      62 /* 2462 MHz */

/* 垂直布局 — 所有 baseline 与 scanner_view 同范式(参考 views/scanner_view.c). */
#define HDR_BASELINE     10  /* FontPrimary "JAMMER" 顶留 2 px 余量,防字顶剪 */
#define HDR_DIV_Y        12

#define MODE_BASELINE    22  /* FontPrimary 大字模式 */
#define CHAN_BASELINE    32  /* FontPrimary 信道 */
#define FREQ_BASELINE    40  /* FontSecondary 频率 */
#define TAG_BASELINE     48  /* FontSecondary 频段标签 / chunk 数 */

#define BTM_DIV_Y        51
#define FTR_BASELINE     62

/* 按键图标尺寸常量 — 与实际绘制函数尺寸对应. */
#define ICON_HARROW_W    4   /* draw_arrow_left/right: 4×7 */
#define ICON_HARROW_H    7
#define ICON_VARROW_W    7   /* draw_arrow_up/down: 7×4 */
#define ICON_VARROW_H    4

/* 4×7 左箭头 */
static void draw_arrow_left(Canvas* canvas, int x, int y) {
    canvas_draw_dot(canvas, x + 3, y);
    canvas_draw_line(canvas, x + 2, y + 1, x + 3, y + 1);
    canvas_draw_line(canvas, x + 1, y + 2, x + 3, y + 2);
    canvas_draw_line(canvas, x,     y + 3, x + 3, y + 3);
    canvas_draw_line(canvas, x + 1, y + 4, x + 3, y + 4);
    canvas_draw_line(canvas, x + 2, y + 5, x + 3, y + 5);
    canvas_draw_dot(canvas, x + 3, y + 6);
}

/* 4×7 右箭头 */
static void draw_arrow_right(Canvas* canvas, int x, int y) {
    canvas_draw_dot(canvas, x, y);
    canvas_draw_line(canvas, x, y + 1, x + 1, y + 1);
    canvas_draw_line(canvas, x, y + 2, x + 2, y + 2);
    canvas_draw_line(canvas, x, y + 3, x + 3, y + 3);
    canvas_draw_line(canvas, x, y + 4, x + 2, y + 4);
    canvas_draw_line(canvas, x, y + 5, x + 1, y + 5);
    canvas_draw_dot(canvas, x, y + 6);
}

/* 7×4 上箭头 */
static void draw_arrow_up(Canvas* canvas, int x, int y) {
    canvas_draw_dot(canvas,  x + 3, y);
    canvas_draw_line(canvas, x + 2, y + 1, x + 4, y + 1);
    canvas_draw_line(canvas, x + 1, y + 2, x + 5, y + 2);
    canvas_draw_line(canvas, x,     y + 3, x + 6, y + 3);
}

/* 7×4 下箭头 */
static void draw_arrow_down(Canvas* canvas, int x, int y) {
    canvas_draw_line(canvas, x,     y,     x + 6, y);
    canvas_draw_line(canvas, x + 1, y + 1, x + 5, y + 1);
    canvas_draw_line(canvas, x + 2, y + 2, x + 4, y + 2);
    canvas_draw_dot(canvas,  x + 3, y + 3);
}

/* ---------------------------------------------------------------------------
 * Model
 * --------------------------------------------------------------------------*/

typedef struct {
    JammerMode mode;
    uint8_t cw_channel;     /* CW 用户选定 ch */
    uint8_t cur_channel;    /* worker 上报的最近 sweep ch */
    uint32_t chunk_count;   /* 累计 chunk(每 16 ch 一个 chunk,sweep 模式增长;CW 模式同样递增表 RUN tick) */
    bool running;
} JammerViewModel;

struct JammerView {
    View* view;
};

/* ---------------------------------------------------------------------------
 * 频段标签辅助
 * --------------------------------------------------------------------------*/

static const char* band_tag_for_channel(uint8_t ch) {
    if(ch == BLE_ADV_CH37_NRF) return "BLE37";
    if(ch == BLE_ADV_CH38_NRF) return "BLE38";
    if(ch == BLE_ADV_CH39_NRF) return "BLE39";
    if(ch == WIFI_1_NRF) return "WiFi1";
    if(ch == WIFI_6_NRF) return "WiFi6";
    if(ch == WIFI_11_NRF) return "WiFi11";
    return NULL;
}

static const char* mode_label(JammerMode m) {
    switch(m) {
    case JammerModeCwCustom:    return "CW";
    case JammerModeBleAdv:      return "BLE Adv";
    case JammerModeReactiveBle: return "BLE React";
    case JammerModeWifi1:       return "WiFi 1";
    case JammerModeWifi6:       return "WiFi 6";
    case JammerModeWifi11:      return "WiFi 11";
    case JammerModeAllBand:     return "ALL 2.4G";
    default:                    return "?";
    }
}

/* 信道范围字符串(显示在 chan 行).返回 NULL = CwCustom 用 cw_channel 动态构造. */
static const char* mode_chan_label(JammerMode m) {
    switch(m) {
    case JammerModeBleAdv:      return "Adv 37/38/39";
    case JammerModeReactiveBle: return "RPD 37/38/39";
    case JammerModeWifi1:       return "Pilots ×4";
    case JammerModeWifi6:       return "Pilots ×4";
    case JammerModeWifi11:      return "Pilots ×4";
    case JammerModeAllBand:     return "Ch 0-125";
    default:                    return NULL;
    }
}

/* 频率范围字符串(显示在 freq 行).返回 NULL = CwCustom 用 cw_channel 动态构造. */
static const char* mode_freq_label(JammerMode m) {
    switch(m) {
    case JammerModeBleAdv:      return "2402/26/80 MHz";
    case JammerModeReactiveBle: return "Listen+jam";
    case JammerModeWifi1:       return "2412 \xb1 OFDM";
    case JammerModeWifi6:       return "2437 \xb1 OFDM";
    case JammerModeWifi11:      return "2462 \xb1 OFDM";
    case JammerModeAllBand:     return "2400-2525 MHz";
    default:                    return NULL;
    }
}

/* ---------------------------------------------------------------------------
 * Draw
 * --------------------------------------------------------------------------*/

static void jammer_view_draw_callback(Canvas* canvas, void* _model) {
    const JammerViewModel* m = _model;
    canvas_clear(canvas);

    char buf[32];

    /* === Header === */
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, HDR_BASELINE, "JAMMER");

    canvas_set_font(canvas, FontSecondary);
    const char* status = m->running ? "RUN" : "STOP";
    canvas_draw_str_aligned(canvas, 127, HDR_BASELINE, AlignRight, AlignBottom, status);

    canvas_draw_line(canvas, 0, HDR_DIV_Y, 127, HDR_DIV_Y);

    /* === Mode (FontPrimary 大字) === */
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, MODE_BASELINE, mode_label(m->mode));

    /* 右侧:+20 dBm 固定标(规范 §3.2 决策表) */
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 127, MODE_BASELINE, AlignRight, AlignBottom, "+20dBm");

    /* === Channel / Range (FontPrimary)=== */
    canvas_set_font(canvas, FontPrimary);
    if(m->mode == JammerModeCwCustom) {
        snprintf(buf, sizeof(buf), "Ch %03u", m->cw_channel);
        canvas_draw_str(canvas, 0, CHAN_BASELINE, buf);
    } else {
        const char* chan_str = mode_chan_label(m->mode);
        if(chan_str) canvas_draw_str(canvas, 0, CHAN_BASELINE, chan_str);
    }

    /* === Frequency (FontSecondary) === */
    canvas_set_font(canvas, FontSecondary);
    if(m->mode == JammerModeCwCustom) {
        snprintf(buf, sizeof(buf), "%d MHz", 2400 + m->cw_channel);
        canvas_draw_str(canvas, 0, FREQ_BASELINE, buf);
    } else {
        const char* freq_str = mode_freq_label(m->mode);
        if(freq_str) canvas_draw_str(canvas, 0, FREQ_BASELINE, freq_str);
    }

    /* === Tag 行 ===
     *   CwCustom : 信道命中已知频段时显示 BLE37/WiFi6 之类的文字标签
     *   多信道模式 (running) : "@045 2445MHz" 显示当前 worker 命中位置
     *   多信道模式 (stopped) : 空(避免与 chunk 计数无关的视觉噪声) */
    if(m->mode == JammerModeCwCustom) {
        const char* tag = band_tag_for_channel(m->cw_channel);
        if(tag != NULL) canvas_draw_str(canvas, 0, TAG_BASELINE, tag);
    } else if(m->running) {
        snprintf(buf, sizeof(buf), "@%03u %dMHz N%lu",
                 m->cur_channel, 2400 + m->cur_channel, (unsigned long)m->chunk_count);
        canvas_draw_str(canvas, 0, TAG_BASELINE, buf);
    }

    canvas_draw_line(canvas, 0, BTM_DIV_Y, 127, BTM_DIV_Y);

    /* === Footer 输入提示 — 用 SDK 内置按键图标(替代文字 <>/^v 容易跟字母混淆) === */
    canvas_set_font(canvas, FontSecondary);

    /* 横向箭头(4×7)与文字基线对齐:y_top = baseline - 6
     * 纵向箭头(7×4)垂直居中于文字带:y_top ≈ baseline - 5(略上移避开 baseline 笔画) */
    const int hy = FTR_BASELINE - 6; /* 横向箭头顶 */
    const int vy = FTR_BASELINE - 5; /* 纵向箭头顶 */

    int x = 0;
    if(m->mode == JammerModeCwCustom) {
        /* CwCustom 才显示左右箭头(只有它能用 ←→ 调信道):
         * [<][>] Ch  [^][v] Mode  OK Run */
        draw_arrow_left(canvas, x, hy);
        x += ICON_HARROW_W + 1;
        draw_arrow_right(canvas, x, hy);
        x += ICON_HARROW_W + 2;
        canvas_draw_str(canvas, x, FTR_BASELINE, "Ch");
        x += canvas_string_width(canvas, "Ch") + 4;

        draw_arrow_up(canvas, x, vy);
        x += ICON_VARROW_W + 1;
        draw_arrow_down(canvas, x, vy);
        x += ICON_VARROW_W + 2;
        canvas_draw_str(canvas, x, FTR_BASELINE, "Mode");
        x += canvas_string_width(canvas, "Mode") + 4;
    } else {
        /* [^][v] Mode  OK Run */
        draw_arrow_up(canvas, x, vy);
        x += ICON_VARROW_W + 1;
        draw_arrow_down(canvas, x, vy);
        x += ICON_VARROW_W + 2;
        canvas_draw_str(canvas, x, FTR_BASELINE, "Mode");
        x += canvas_string_width(canvas, "Mode") + 4;
    }

    /* OK Run 右对齐到屏右(给 mode 标签留弹性空间) */
    canvas_draw_str_aligned(canvas, 127, FTR_BASELINE, AlignRight, AlignBottom, "OK Run");
}

/* ---------------------------------------------------------------------------
 * Lifecycle
 * --------------------------------------------------------------------------*/

JammerView* jammer_view_alloc(void) {
    JammerView* jv = malloc(sizeof(*jv));
    furi_check(jv);
    jv->view = view_alloc();
    view_allocate_model(jv->view, ViewModelTypeLocking, sizeof(JammerViewModel));
    view_set_draw_callback(jv->view, jammer_view_draw_callback);

    with_view_model(
        jv->view,
        JammerViewModel * m,
        {
            m->mode = JammerModeCwCustom;
            m->cw_channel = 42; /* 默认 ~2442 MHz,非 BLE/WiFi 锚频 */
            m->cur_channel = 0;
            m->chunk_count = 0;
            m->running = false;
        },
        false);

    return jv;
}

void jammer_view_free(JammerView* jv) {
    if(jv == NULL) return;
    view_free(jv->view);
    free(jv);
}

View* jammer_view_get_view(JammerView* jv) {
    furi_check(jv);
    return jv->view;
}

/* ---------------------------------------------------------------------------
 * 模型更新
 * --------------------------------------------------------------------------*/

void jammer_view_set_mode(JammerView* jv, JammerMode mode) {
    furi_check(jv);
    if(mode >= JammerModeCount) return;
    with_view_model(jv->view, JammerViewModel * m, { m->mode = mode; }, true);
}

void jammer_view_set_cw_channel(JammerView* jv, uint8_t ch) {
    furi_check(jv);
    if(ch > 125) ch = 125;
    with_view_model(jv->view, JammerViewModel * m, { m->cw_channel = ch; }, true);
}

void jammer_view_set_running(JammerView* jv, bool running) {
    furi_check(jv);
    with_view_model(jv->view, JammerViewModel * m, { m->running = running; }, true);
}

void jammer_view_update_tick(JammerView* jv, uint8_t cur_channel, uint32_t chunk_count) {
    furi_check(jv);
    with_view_model(
        jv->view,
        JammerViewModel * m,
        {
            m->cur_channel = cur_channel;
            m->chunk_count = chunk_count;
        },
        true);
}
