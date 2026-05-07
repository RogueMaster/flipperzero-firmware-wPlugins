/**
 * @file scanner_scene.c
 * @brief Channel Scan 实现.fast-scan 路径,每 sweep ~25ms (126ch × 200µs).
 *
 * 线程模型:
 *   主线程 (GUI):scanner_input_callback 处理按键,直接调 view setter.
 *   worker 线程:持续做 126 通道 sweep,刷 hits[] + peak_ch + sweep_count,
 *                每 sweep 末调 scanner_view_update_sweep 触发 redraw.
 *   芯片访问: 全部 sweep 装在 pq_chip_with_nrf24 callback 内(M3/M11).
 */
#include "scanner_scene.h"

#include "../core/pq_chip_arbiter.h"
#include "../core/pq_nrf24.h"
#include "../core/pq_nrf24_regs.h"
#include "../core/pq_scan_export.h"
#include "../core/pq_scene_lifecycle.h"
#include "../pingequa_app.h"
#include "../views/scanner_view.h"

#include <furi.h>
#include <input/input.h>
#include <string.h>

#define TAG                 "PqScannerScene"
#define SCENE_NAME          "scanner"
#define WORKER_STACK        2048
#define WORKER_TIMEOUT_MS   100
#define WORKER_PAUSE_MS     50

/* CONFIG 用于"RX standby"模式:PWR_UP=1, PRIM_RX=1, IRQ 全 mask. */
#define NRF24_RX_STANDBY_CFG                                   \
    (NRF24_CONFIG_PWR_UP | NRF24_CONFIG_PRIM_RX |              \
     NRF24_CONFIG_MASK_MAX_RT | NRF24_CONFIG_MASK_TX_DS |      \
     NRF24_CONFIG_MASK_RX_DR)

/* ---------------------------------------------------------------------------
 * NRF24 设置:scene_on_enter 时一次性初始化寄存器
 * --------------------------------------------------------------------------*/

static bool scanner_setup_cb(void* ctx) {
    PqApp* app = ctx;
    if(!pq_nrf24_init_regs(app->nrf)) {
        FURI_LOG_E(TAG, "init_regs failed");
        return false;
    }
    pq_nrf24_write_reg(app->nrf, NRF24_REG_CONFIG, NRF24_RX_STANDBY_CFG);
    /* Tpd2stby ~1.5ms typ;留 2ms 安全余量(数据手册 §6.1.7 Table 16). */
    furi_delay_ms(2);
    return true;
}

/* ---------------------------------------------------------------------------
 * 单次 sweep:126 通道,each 200µs(~30µs SPI + dwell),累加 hits[]
 * --------------------------------------------------------------------------*/

static bool scan_one_sweep_cb(void* ctx) {
    PqApp* app = ctx;
    pq_chip_nrf24_ce_set(false); /* 起始确保 CE low. */
    for(uint8_t ch = 0; ch < PQ_NUM_CHANNELS; ch++) {
        pq_nrf24_set_channel(app->nrf, ch);
        pq_chip_nrf24_ce_set(true);
        furi_delay_us(app->dwell_us);
        /* 累加器模式(max-hold spectrum analyzer 经典):
         *   命中 → +2 直到饱和 PQ_HITS_MAX
         *   不衰减(由 sweep 末的 max-hold 自动暂停代替衰减)
         * 任何 bar 到顶后 worker 整体停止扫描,画面冻结供用户研究. */
        if(pq_nrf24_read_rpd(app->nrf)) {
            uint8_t v = app->hits[ch] + 2;
            app->hits[ch] = (v > PQ_HITS_MAX) ? PQ_HITS_MAX : v;
            app->hit_total++;
        }
        pq_chip_nrf24_ce_set(false);
    }
    return true;
}

/* ---------------------------------------------------------------------------
 * Worker 线程
 * --------------------------------------------------------------------------*/

static int32_t scanner_worker_func(void* ctx) {
    PqApp* app = ctx;

    while(!app->scan_stop_requested) {
        if(!app->scan_running) {
            furi_delay_ms(WORKER_PAUSE_MS);
            continue;
        }

        bool ok = pq_chip_with_nrf24(scan_one_sweep_cb, app, WORKER_TIMEOUT_MS);
        if(!ok) {
            FURI_LOG_W(TAG, "with_nrf24 timeout, retry");
            furi_delay_ms(WORKER_PAUSE_MS);
            continue;
        }

        app->sweep_count++;

        /* 全局衰减 已废弃 — 现在每通道在 sweep 内泄漏积分. */

        /* 更新峰值通道. */
        uint8_t peak_ch = 0;
        uint8_t peak_val = 0;
        for(int i = 0; i < PQ_NUM_CHANNELS; i++) {
            if(app->hits[i] > peak_val) {
                peak_val = app->hits[i];
                peak_ch = (uint8_t)i;
            }
        }
        app->peak_ch = peak_ch;

        /* 推送给 view. */
        scanner_view_update_sweep(app->scanner_view, app->hits, app->peak_ch, app->sweep_count);

        /* Max-hold 自动暂停:任何 bar 触顶 → 停扫描 + 冻结画面.
         * 用户按 OK 清零重扫(input cb 处理). */
        if(peak_val >= PQ_HITS_MAX) {
            app->scan_running = false;
            app->auto_paused = true;
            scanner_view_set_running(app->scanner_view, false);
            scanner_view_set_auto_paused(app->scanner_view, true);
        }

        /* Yield 30ms 给 GUI 线程处理输入. */
        furi_delay_ms(30);
    }

    return 0;
}

/* ---------------------------------------------------------------------------
 * Input 回调 — 跑在 GUI 线程
 * --------------------------------------------------------------------------*/

static bool scanner_input_callback(InputEvent* event, void* ctx) {
    PqApp* app = ctx;

    /* Press 阶段:Short / Long / Repeat;Press/Release 不处理. */
    bool is_short = (event->type == InputTypeShort);
    bool is_long_or_rep = (event->type == InputTypeLong) || (event->type == InputTypeRepeat);
    if(!is_short && !is_long_or_rep) return false;

    switch(event->key) {
    case InputKeyLeft: {
        /* Short ±1, Long/Repeat ±5(批量翻 2.4G 频谱). */
        uint8_t step = is_long_or_rep ? 5 : 1;
        app->cursor_ch = (app->cursor_ch >= step) ? (app->cursor_ch - step) : 0;
        scanner_view_set_cursor(app->scanner_view, app->cursor_ch);
        return true;
    }

    case InputKeyRight: {
        uint8_t step = is_long_or_rep ? 5 : 1;
        uint16_t v = (uint16_t)app->cursor_ch + step;
        app->cursor_ch = (v < PQ_NUM_CHANNELS) ? (uint8_t)v : (PQ_NUM_CHANNELS - 1);
        scanner_view_set_cursor(app->scanner_view, app->cursor_ch);
        return true;
    }

    case InputKeyUp: {
        uint16_t step = is_long_or_rep ? PQ_DWELL_STEP_L : PQ_DWELL_STEP_S;
        uint32_t v = (uint32_t)app->dwell_us + step;
        if(v > PQ_DWELL_MAX) v = PQ_DWELL_MAX;
        app->dwell_us = (uint16_t)v;
        scanner_view_set_dwell(app->scanner_view, app->dwell_us);
        return true;
    }

    case InputKeyDown: {
        uint16_t step = is_long_or_rep ? PQ_DWELL_STEP_L : PQ_DWELL_STEP_S;
        uint16_t v = (app->dwell_us > step + PQ_DWELL_MIN) ? (app->dwell_us - step) : PQ_DWELL_MIN;
        app->dwell_us = v;
        scanner_view_set_dwell(app->scanner_view, app->dwell_us);
        return true;
    }

    case InputKeyOk:
        if(is_short) {
            if(app->auto_paused) {
                /* MAX HOLD → OK 清零重扫. */
                memset(app->hits, 0, sizeof(app->hits));
                app->peak_ch = 0;
                app->hit_total = 0;
                app->auto_paused = false;
                app->scan_running = true;
                scanner_view_set_auto_paused(app->scanner_view, false);
            } else {
                /* 普通暂停/恢复. */
                app->scan_running = !app->scan_running;
            }
            scanner_view_set_running(app->scanner_view, app->scan_running);
        } else if(event->type == InputTypeLong) {
            /* 长按 OK → 导出当前 hits[] 到 CSV(/ext/apps_data/pingequa/scans/).
             * Storage IO 在 GUI 线程同步执行(~50 ms),用户视觉无明显卡顿.
             * 导出后顶栏右上角 "SAVED!" 闪 ~1 秒(view 自动衰减). */
            char path[96] = {0};
            bool ok = pq_scan_export_csv(
                app->hits, PQ_NUM_CHANNELS, app->peak_ch, app->dwell_us, app->sweep_count,
                path, sizeof(path));
            if(ok) {
                FURI_LOG_I(TAG, "scan exported: %s", path);
                scanner_view_show_export_flash(app->scanner_view);
            } else {
                FURI_LOG_E(TAG, "scan export failed");
            }
        }
        return true;

    case InputKeyBack:
        /* v1.2 起 splash 已从栈上移除(splash_scene → search_and_switch),
         * 当前栈底是 main_menu — 让 SceneManager nav 走 previous_scene 即可
         * (规范 §6.1).返回 false 把 Back 让给 navigation_event_callback. */
        if(is_short) {
            app->scan_stop_requested = true; /* 提前发停信号,缩短 join 等待 */
            app->scan_running = false;
        }
        return false;

    default:
        return false;
    }
}

/* ---------------------------------------------------------------------------
 * Scene 接口
 * --------------------------------------------------------------------------*/

void scanner_scene_on_enter(void* context) {
    PqApp* app = context;
    pq_scene_enter(SCENE_NAME);

    /* 复位 scanner 状态. */
    memset(app->hits, 0, sizeof(app->hits));
    app->cursor_ch = 0;
    app->peak_ch = 0;
    app->dwell_us = PQ_DWELL_DEFAULT;
    app->sweep_count = 0;
    app->hit_total = 0;
    app->decay_counter = 0;
    app->scan_running = true;
    app->scan_stop_requested = false;
    app->auto_paused = false;

    /* Alloc nrf24 driver(全程使用). */
    app->nrf = pq_nrf24_alloc(&app->cfg);

    /* 一次性写寄存器到 RX standby. */
    if(!pq_chip_with_nrf24(scanner_setup_cb, app, 200)) {
        FURI_LOG_E(TAG, "scanner_setup_cb failed; transitioning to error");
        /* 探测时已通过,但此处仍可能失败(电源滑动等).转到 error. */
        scene_manager_next_scene(app->scene_manager, PqSceneError);
        return;
    }

    /* View 是 app-owned;scene 只设上下文/回调 + 切换. */
    View* v = scanner_view_get_view(app->scanner_view);
    view_set_context(v, app);
    view_set_input_callback(v, scanner_input_callback);
    view_dispatcher_switch_to_view(app->view_dispatcher, PqViewScanner);

    /* 初值同步给 view. */
    scanner_view_set_cursor(app->scanner_view, app->cursor_ch);
    scanner_view_set_dwell(app->scanner_view, app->dwell_us);
    scanner_view_set_running(app->scanner_view, app->scan_running);

    /* Worker. */
    app->scan_thread = furi_thread_alloc_ex(
        "PqScanWorker", WORKER_STACK, scanner_worker_func, app);
    furi_thread_set_priority(app->scan_thread, FuriThreadPriorityLow);
    furi_thread_start(app->scan_thread);
}

bool scanner_scene_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    /* 输入由 view 的 input_callback 直接消化;此处无 custom event 待处理. */
    return false;
}

void scanner_scene_on_exit(void* context) {
    PqApp* app = context;

    /* 1. 通知 worker 停 + join. */
    if(app->scan_thread) {
        app->scan_stop_requested = true;
        furi_thread_join(app->scan_thread);
        furi_thread_free(app->scan_thread);
        app->scan_thread = NULL;
    }

    /* 2. 释放 nrf24 driver. chip_arbiter 后续 deinit 会把 CSN 拉回 Analog. */
    if(app->nrf) {
        pq_nrf24_free(app->nrf);
        app->nrf = NULL;
    }

    /* 3. View 是 app-owned,scene 不释放. */

    pq_scene_exit(SCENE_NAME);
}
