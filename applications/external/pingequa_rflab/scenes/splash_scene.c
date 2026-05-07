/**
 * @file splash_scene.c
 * @brief 启动屏 Scene 实现.
 *
 * 时序(规范 §7):
 *   T+0       on_enter:启动 worker + tick timer + 显示 splash view
 *   T+250     LDO 稳定(worker 内 furi_delay_ms)
 *   T+400     PoR 稳定(worker 继续 furi_delay_ms)
 *   T+~500    pq_board_detect 完成 → 发 DetectDone 事件
 *   T+1000    splash 动画结束 → 转 scanner_scene 或 error_scene
 *
 * 转场触发:必须同时满足
 *   (1) 探测完成(detect_done == true)
 *   (2) 总耗时 ≥ 1000ms(用户视觉体验)
 */
#include "splash_scene.h"

#include "../core/pq_board_detect.h"
#include "../core/pq_scene_lifecycle.h"
#include "../pingequa_app.h"
#include "../views/splash_view.h"

#include <furi.h>

#define SCENE_NAME            "splash"
#define SPLASH_DURATION_MS    1000
#define SPLASH_TICK_PERIOD_MS 50
#define LDO_SETTLE_MS         250
#define POR_WAIT_MS           150
#define WORKER_STACK_SIZE     2048

/* Tick → ms 转换(Furi SDK 没有 ticks_to_ms,自己用 tick frequency 换算). */
static inline uint32_t pq_elapsed_ms(uint32_t start_tick) {
    return (furi_get_tick() - start_tick) * 1000U / furi_kernel_get_tick_frequency();
}

/* Scene-local state.PqApp 单例,static 安全. */
static struct {
    uint32_t start_tick;
    bool detect_done;
} s_splash;

/* ---------------------------------------------------------------------------
 * Worker(子线程):等待硬件稳定 → 探测 → 通知主线程
 * --------------------------------------------------------------------------*/

static int32_t splash_worker_func(void* ctx) {
    PqApp* app = ctx;
    furi_delay_ms(LDO_SETTLE_MS);
    furi_delay_ms(POR_WAIT_MS);
    app->detect = pq_board_detect();
    view_dispatcher_send_custom_event(app->view_dispatcher, PqCustomEventDetectDone);
    return 0;
}

/* ---------------------------------------------------------------------------
 * Tick(GUI 线程定时器):驱动进度条 + 触发条件检查
 * --------------------------------------------------------------------------*/

static void splash_tick_cb(void* ctx) {
    PqApp* app = ctx;
    view_dispatcher_send_custom_event(app->view_dispatcher, PqCustomEventSplashTick);
}

/* ---------------------------------------------------------------------------
 * 转场判定 — 满足 (elapsed ≥ 1s) 且 (detect_done) 才切到下一 scene
 * --------------------------------------------------------------------------*/

static void try_transition(PqApp* app) {
    if(!s_splash.detect_done) return;
    uint32_t elapsed_ms = pq_elapsed_ms(s_splash.start_tick);
    if(elapsed_ms < SPLASH_DURATION_MS) return;

    /* search_and_switch_to_another_scene 而不是 next_scene:
     * splash 是一次性引导屏,转场后必须从栈上移除,这样 Back 从 main_menu /
     * error 退出 app(否则会回退到 splash 重跑探测).
     * v1.2 起改跳 main_menu(规范 §6.2 v1.1+ 扩展点) — main_menu 再分流到
     * scanner / jammer. */
    if(app->detect.type == PQ_BOARD_PINGEQUA_2IN1) {
        scene_manager_search_and_switch_to_another_scene(
            app->scene_manager, PqSceneMainMenu);
    } else {
        scene_manager_search_and_switch_to_another_scene(
            app->scene_manager, PqSceneError);
    }
}

/* ---------------------------------------------------------------------------
 * Scene 接口
 * --------------------------------------------------------------------------*/

void splash_scene_on_enter(void* context) {
    PqApp* app = context;
    pq_scene_enter(SCENE_NAME);

    s_splash.start_tick = furi_get_tick();
    s_splash.detect_done = false;

    /* 1. Reset view 状态(view 是 app-owned,可能有上轮残留). */
    splash_view_set_progress(app->splash_view, 0);
    view_dispatcher_switch_to_view(app->view_dispatcher, PqViewSplash);

    /* 2. Worker(低优先级,主线程不被阻塞). */
    app->splash_worker = furi_thread_alloc_ex(
        "PqSplashWorker", WORKER_STACK_SIZE, splash_worker_func, app);
    furi_thread_set_priority(app->splash_worker, FuriThreadPriorityLow);
    furi_thread_start(app->splash_worker);

    /* 3. UI tick(50ms 周期 → 20 帧 1 秒动画). */
    app->splash_timer = furi_timer_alloc(splash_tick_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(app->splash_timer, furi_ms_to_ticks(SPLASH_TICK_PERIOD_MS));
}

bool splash_scene_on_event(void* context, SceneManagerEvent event) {
    PqApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == PqCustomEventSplashTick) {
        uint32_t elapsed_ms = pq_elapsed_ms(s_splash.start_tick);
        uint8_t progress = (elapsed_ms * 100u) / SPLASH_DURATION_MS;
        if(progress > 100) progress = 100;
        splash_view_set_progress(app->splash_view, progress);
        try_transition(app);
        return true;
    }

    if(event.event == PqCustomEventDetectDone) {
        s_splash.detect_done = true;
        try_transition(app);
        return true;
    }

    return false;
}

void splash_scene_on_exit(void* context) {
    PqApp* app = context;

    /* 1. 停 timer. */
    if(app->splash_timer) {
        furi_timer_stop(app->splash_timer);
        furi_timer_free(app->splash_timer);
        app->splash_timer = NULL;
    }

    /* 2. join worker(它必已完成). */
    if(app->splash_worker) {
        furi_thread_join(app->splash_worker);
        furi_thread_free(app->splash_worker);
        app->splash_worker = NULL;
    }

    /* 3. View 留给 app_free 时统一 remove + free.scene 不动 view. */

    pq_scene_exit(SCENE_NAME);
}
