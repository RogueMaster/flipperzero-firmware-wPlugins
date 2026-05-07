/**
 * @file pingequa_app.c
 * @brief FAP 入口 + ViewDispatcher / SceneManager 装配 + 生命周期.
 *
 * 规范权威:§7 启动生命周期 + §6.1 Scene 标准结构.
 *
 * 流程概览(规范 §7):
 *   T+0    pingequa_rf_toolkit_app(): malloc PqApp 0-init
 *   T+0    Furi 服务 open: gui, notification(storage 由 pq_config 自管)
 *   T+0    OTG 5V 启用(按需,启动前若已开则不动)
 *   T+0    ViewDispatcher + SceneManager alloc
 *   T+0    pq_chip_arbiter_init() — 两 CSN HIGH,SPI peripheral 上电
 *   T+0    pq_config_load() — 失败回 default
 *   T+15   scene_manager_next_scene(splash) → view_dispatcher_run
 *           ↓ splash → scanner / error → exit
 *   T+exit pq_app_shutdown() — chip_arbiter 兜底释放
 *   T+exit Furi 服务 close,free PqApp
 *   T+exit OTG **不**主动关(规范 §5.7)
 */
#include "pingequa_app.h"

#include "core/pq_chip_arbiter.h"
#include "core/pq_config.h"
#include "core/pq_scene_lifecycle.h"
#include "scenes/error_scene.h"
#include "scenes/jammer_scene.h"
#include "scenes/main_menu_scene.h"
#include "scenes/scanner_scene.h"
#include "scenes/splash_scene.h"
#include "views/jammer_view.h"
#include "views/scanner_view.h"
#include "views/splash_view.h"

#include <furi.h>
#include <furi_hal.h>

#include <stdlib.h>
#include <string.h>

#define TAG "PqApp"

/* ---------------------------------------------------------------------------
 * Scene handlers — 顺序与 PqScene 枚举一致(designated initializer 保证).
 * --------------------------------------------------------------------------*/

static void (*const pq_scene_on_enter_handlers[PqSceneCount])(void*) = {
    [PqSceneSplash] = splash_scene_on_enter,
    [PqSceneMainMenu] = main_menu_scene_on_enter,
    [PqSceneScanner] = scanner_scene_on_enter,
    [PqSceneJammer] = jammer_scene_on_enter,
    [PqSceneError] = error_scene_on_enter,
};

static bool (*const pq_scene_on_event_handlers[PqSceneCount])(void*, SceneManagerEvent) = {
    [PqSceneSplash] = splash_scene_on_event,
    [PqSceneMainMenu] = main_menu_scene_on_event,
    [PqSceneScanner] = scanner_scene_on_event,
    [PqSceneJammer] = jammer_scene_on_event,
    [PqSceneError] = error_scene_on_event,
};

static void (*const pq_scene_on_exit_handlers[PqSceneCount])(void*) = {
    [PqSceneSplash] = splash_scene_on_exit,
    [PqSceneMainMenu] = main_menu_scene_on_exit,
    [PqSceneScanner] = scanner_scene_on_exit,
    [PqSceneJammer] = jammer_scene_on_exit,
    [PqSceneError] = error_scene_on_exit,
};

static const SceneManagerHandlers pq_scene_handlers = {
    .on_enter_handlers = pq_scene_on_enter_handlers,
    .on_event_handlers = pq_scene_on_event_handlers,
    .on_exit_handlers = pq_scene_on_exit_handlers,
    .scene_num = PqSceneCount,
};

/* ---------------------------------------------------------------------------
 * ViewDispatcher 事件路由
 * --------------------------------------------------------------------------*/

static bool pq_app_custom_event_callback(void* context, uint32_t event) {
    PqApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool pq_app_back_event_callback(void* context) {
    PqApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

/* ---------------------------------------------------------------------------
 * App 生命周期
 * --------------------------------------------------------------------------*/

static PqApp* pq_app_alloc(void) {
    PqApp* app = malloc(sizeof(PqApp));
    furi_check(app);
    memset(app, 0, sizeof(*app));

    /* 1. Furi 服务 open. Storage 由 pq_config 在每次读写时自管 record. */
    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    /* 2. ViewDispatcher / SceneManager. */
    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&pq_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, pq_app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, pq_app_back_event_callback);
    view_dispatcher_attach_to_gui(
        app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    /* 3. 配置:失败 fallback 默认值,继续启动. */
    pq_config_load(&app->cfg);

    /* 4. OTG 5V:启动前若已开则不动(尊重系统层 OTG 计数). */
    app->otg_was_on = furi_hal_power_is_otg_enabled();
    if(!app->otg_was_on) {
        furi_hal_power_enable_otg();
    }

    /* 5. Chip arbiter:两 CSN HIGH,SPI1 peripheral 上电.
     * 之后所有芯片访问都经 pq_chip_with_*. */
    pq_chip_arbiter_init(&app->cfg);

    /* 6. Views(app-owned 生命周期 — Flipper subghz 标准模式).
     * 一次 alloc + add_view,scene 只 switch_to;app_free 时一次 remove + free.
     * 这避免了"on_exit 时 view 还是 current view 不能 remove"的崩溃路径. */
    app->splash_view = splash_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, PqViewSplash, splash_view_get_view(app->splash_view));

    app->main_menu_submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, PqViewMainMenu, submenu_get_view(app->main_menu_submenu));

    app->scanner_view = scanner_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, PqViewScanner, scanner_view_get_view(app->scanner_view));

    app->jammer_view = jammer_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, PqViewJammer, jammer_view_get_view(app->jammer_view));

    app->error_dialog = dialog_ex_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, PqViewError, dialog_ex_get_view(app->error_dialog));

    return app;
}

static void pq_app_free(PqApp* app) {
    /* 0. 防御性清理 — 若用户从 scanner/jammer Back 退出而 scene 的 on_exit
     * 没被走完,这里兜底停 worker / 释放 driver(N+ 次 free 用 NULL 检查保证幂等). */
    if(app->scan_thread) {
        app->scan_stop_requested = true;
        furi_thread_join(app->scan_thread);
        furi_thread_free(app->scan_thread);
        app->scan_thread = NULL;
    }
    if(app->jammer_thread) {
        app->jammer_stop_requested = true;
        app->jammer_running = false;
        furi_thread_join(app->jammer_thread);
        furi_thread_free(app->jammer_thread);
        app->jammer_thread = NULL;
    }
    if(app->splash_worker) {
        furi_thread_join(app->splash_worker);
        furi_thread_free(app->splash_worker);
        app->splash_worker = NULL;
    }
    if(app->splash_timer) {
        furi_timer_stop(app->splash_timer);
        furi_timer_free(app->splash_timer);
        app->splash_timer = NULL;
    }
    if(app->nrf) {
        pq_nrf24_free(app->nrf);
        app->nrf = NULL;
    }

    /* 1. 芯片仲裁器兜底释放(规范 §5.7). */
    pq_app_shutdown();

    /* 2. Views remove + free(app-owned). */
    view_dispatcher_remove_view(app->view_dispatcher, PqViewSplash);
    splash_view_free(app->splash_view);
    app->splash_view = NULL;

    view_dispatcher_remove_view(app->view_dispatcher, PqViewMainMenu);
    submenu_free(app->main_menu_submenu);
    app->main_menu_submenu = NULL;

    view_dispatcher_remove_view(app->view_dispatcher, PqViewScanner);
    scanner_view_free(app->scanner_view);
    app->scanner_view = NULL;

    view_dispatcher_remove_view(app->view_dispatcher, PqViewJammer);
    jammer_view_free(app->jammer_view);
    app->jammer_view = NULL;

    view_dispatcher_remove_view(app->view_dispatcher, PqViewError);
    dialog_ex_free(app->error_dialog);
    app->error_dialog = NULL;

    /* 3. ViewDispatcher / SceneManager. */
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    /* 4. Furi 服务. */
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    /* 4. OTG:**不**主动关(规范 §5.7);若启动时 OTG 关、本 app 开,
     * 现在不去关 — 让 Flipper 系统层 power_insomnia 计数自然回落. */

    free(app);
}

/* ---------------------------------------------------------------------------
 * FAP 入口
 * --------------------------------------------------------------------------*/

int32_t pingequa_rf_toolkit_app(void* p) {
    UNUSED(p);
    FURI_LOG_I(TAG, "PINGEQUA RF Lab starting (spec v1.4)");

    PqApp* app = pq_app_alloc();

    /* 入栈第一个 scene. */
    scene_manager_next_scene(app->scene_manager, PqSceneSplash);

    /* 主循环 — 阻塞,直到 view_dispatcher_stop() 或栈空. */
    view_dispatcher_run(app->view_dispatcher);

    FURI_LOG_I(TAG, "exiting cleanly");
    pq_app_free(app);
    return 0;
}
