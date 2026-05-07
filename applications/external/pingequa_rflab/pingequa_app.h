/**
 * @file pingequa_app.h
 * @brief 应用全局状态 + 入口声明.
 *
 * 规范权威:PINGEQUA RF Toolkit 系统架构规范 v1.4 §12 项目布局,§6 Scene 层,
 * §7 启动生命周期.
 *
 * v0.1.7 旧 PingequaState 已废弃,P13 强制重写为 ViewDispatcher + SceneManager
 * 模型(spec 反模式 §17 R3 明确:不允许自管 ViewPort).
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <furi.h>
#include <gui/gui.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/submenu.h>
#include <notification/notification.h>

#include "core/pq_board_detect.h"
#include "core/pq_cc1101.h"
#include "core/pq_chip_arbiter.h"
#include "core/pq_config.h"
#include "core/pq_nrf24.h"
#include "core/pq_nrf24_regs.h"

/* JammerMode 在 views/jammer_view.h 定义;PqApp 直接持有 enum 字段需要完整声明. */
#include "views/jammer_view.h"

/* ---------------------------------------------------------------------------
 * Scene / View / 自定义事件 ID
 * --------------------------------------------------------------------------*/

/** Scene ID — 顺序无强约束,但与 scene_handlers[] 数组对齐. */
typedef enum {
    PqSceneSplash,
    PqSceneMainMenu, /* v1.2 起作为 splash 落地后的总入口(规范 §6.2 v1.1+ 扩展点) */
    PqSceneScanner,
    PqSceneJammer,   /* v1.2 NRF24 Jammer */
    PqSceneError,
    PqSceneCount,
} PqScene;

/** View ID — 注册到 ViewDispatcher 的句柄. */
typedef enum {
    PqViewSplash,
    PqViewMainMenu,
    PqViewScanner,
    PqViewJammer,
    PqViewError,
} PqView;

/** Custom event ID — 通过 view_dispatcher_send_custom_event 传递. */
typedef enum {
    PqCustomEventSplashTick = 1, /* 50 ms 定时,UI 进度条刷新 */
    PqCustomEventDetectDone,     /* splash worker 完成探测 */
    PqCustomEventScanTick,       /* scanner worker 完成一次 sweep,触发 UI redraw */
    PqCustomEventErrorExit,      /* error_scene 用户确认 → 退出 app */
} PqCustomEvent;

/* ---------------------------------------------------------------------------
 * Channel scan 调参
 * --------------------------------------------------------------------------*/

#define PQ_NUM_CHANNELS NRF24_CHANNEL_COUNT /* 126 */
#define PQ_HITS_MAX     32 /* 饱和计数;0..32 映射到 0..47 像素柱高 */
#define PQ_DECAY_EVERY  8 /* 每 8 sweep 全局 hits-- 一次 */

#define PQ_DWELL_DEFAULT 150  /* µs,htotoo 实测合理值,稍高于 Tstby2a (130) */
#define PQ_DWELL_MIN     130  /* NRF24 Tstby2a;低于此 RPD 不可靠 */
#define PQ_DWELL_MAX     2000
#define PQ_DWELL_STEP_S  10
#define PQ_DWELL_STEP_L  50

/* ---------------------------------------------------------------------------
 * 全局 App 状态
 * --------------------------------------------------------------------------*/

typedef struct PqApp PqApp;

struct PqApp {
    /* Furi services. */
    Gui* gui;
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;

    /* 板卡配置 + 探测结果. */
    PqBoardConfig cfg;
    PqDetectResult detect;
    bool otg_was_on; /* 启动时 OTG 状态;退出时不主动关(规范 §5.7). */

    /* 芯片驱动句柄(scanner_scene / jammer_scene 拥有生命期). */
    PqNrf24* nrf;
    PqCc1101* cc;

    /* 自定义 View 实例 + 官方模块句柄(app-owned;一次 alloc + add_view,
     * scene 只 switch_to;app_free 时统一 remove + free). */
    struct SplashView* splash_view;
    Submenu* main_menu_submenu;
    struct ScannerView* scanner_view;
    struct JammerView* jammer_view;
    DialogEx* error_dialog;

    /* Splash:worker 探测线程 + UI 进度 timer. */
    FuriThread* splash_worker;
    FuriTimer* splash_timer;

    /* Scanner:worker 扫描线程. */
    FuriThread* scan_thread;
    volatile bool scan_stop_requested;
    volatile bool scan_running; /* OK 键暂停/恢复;volatile 保证 worker 看到 GUI 写入 */
    volatile bool auto_paused;  /* worker 因 max-hold 自动暂停;OK 键清零重扫 */

    /* Scanner UI 状态. */
    uint8_t hits[PQ_NUM_CHANNELS];
    uint8_t cursor_ch;
    uint8_t peak_ch;
    volatile uint16_t dwell_us; /* GUI 写,worker 读,volatile 防缓存 */
    uint16_t sweep_count;
    uint32_t hit_total; /* uint16 在 ~43s 后溢出,改 uint32 */
    uint8_t decay_counter;

    /* Jammer:worker 干扰线程 + 状态(规范 §13 Phase 5 / §16). */
    FuriThread* jammer_thread;
    volatile bool jammer_stop_requested;
    volatile bool jammer_running;        /* OK 键启停,worker 观察并触发 start_cb / stop_cb */
    volatile JammerMode jammer_mode;     /* CW / Sweep,worker + GUI 共读 */
    volatile uint8_t jammer_cw_channel;  /* CW 用户当前选定 ch(0..125) */
    volatile bool jammer_cw_dirty;       /* GUI 改 ch 后置 true,worker 下个 chunk 写 RF_CH 后清零 */
    uint8_t jammer_sweep_cursor;         /* worker 内部:sweep 模式当前 ch */
    uint32_t jammer_chunk_count;         /* worker 写,GUI 经 view 间接读 */
};

/** FAP 入口点(application.fam 中 entry_point 字段). */
int32_t pingequa_rf_toolkit_app(void* p);
