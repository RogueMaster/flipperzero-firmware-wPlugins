// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "../recon_app_i.h"

typedef enum {
    StartItemFlock,
    StartItemGuardian,
    StartItemWifi,
    StartItemBle,
    StartItemNfc,
    StartItemFirmware,
    StartItemReports,
    StartItemSettings,
    StartItemAbout,
    StartItemFlockMap,
    StartItemDeflockShare,
    StartItemLocator,
} StartItem;

static void recon_scene_start_submenu_cb(void* context, uint32_t index) {
    ReconApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

// Surface the fused WATCHSCORE as the start-screen header, with an explainable
// per-signal breakdown when anything is contributing. CLEAR shows the plain
// app name so the idle screen stays calm.
//
// Coverage honesty: the BLE-follower, deauth and evil-twin signals only reach us
// through our companion firmware. In Marauder/Generic mode WATCHSCORE can only
// watch the WiFi/Flock side -- it can never reach ELEVATED, and a bare "clear"
// would falsely imply "you are not being watched." So in that mode we tag the
// header "(WiFi only)" and never let CLEAR read as all-clear.
static void recon_scene_start_update_header(ReconApp* app) {
    bool wifi_only = (app->settings.backend != EspBackendCompanion);
    WatchState st = (WatchState)app->watch.state;
    if(st == WatchStateClear) {
        submenu_set_header(
            app->submenu, wifi_only ? "FlipDeFlock - watch: WiFi only" : "FlipDeFlock");
        return;
    }
    const char* bd = app->watch.breakdown;
    const char* cov = wifi_only ? " (WiFi only)" : "";
    if(bd[0]) {
        snprintf(
            app->text_store,
            sizeof(app->text_store),
            "WATCH: %s%s - %s",
            watchscore_state_str(st),
            cov,
            bd);
    } else {
        snprintf(
            app->text_store, sizeof(app->text_store), "WATCH: %s%s", watchscore_state_str(st), cov);
    }
    submenu_set_header(app->submenu, app->text_store);
}

void recon_scene_start_on_enter(void* context) {
    ReconApp* app = context;
    Submenu* submenu = app->submenu;
    submenu_reset(submenu);
    recon_scene_start_update_header(app);
    submenu_add_item(
        submenu, "Flock / ALPR Detect", StartItemFlock, recon_scene_start_submenu_cb, app);
    submenu_add_item(
        submenu, "Net Guardian", StartItemGuardian, recon_scene_start_submenu_cb, app);
    submenu_add_item(submenu, "Locator", StartItemLocator, recon_scene_start_submenu_cb, app);
    submenu_add_item(submenu, "Flock Map", StartItemFlockMap, recon_scene_start_submenu_cb, app);
    submenu_add_item(submenu, "WiFi Audit", StartItemWifi, recon_scene_start_submenu_cb, app);
    submenu_add_item(
        submenu, "BLE / Tracker Scan", StartItemBle, recon_scene_start_submenu_cb, app);
    submenu_add_item(submenu, "NFC / RFID Audit", StartItemNfc, recon_scene_start_submenu_cb, app);
    submenu_add_item(
        submenu, "ESP32 Firmware", StartItemFirmware, recon_scene_start_submenu_cb, app);
    submenu_add_item(submenu, "Reports", StartItemReports, recon_scene_start_submenu_cb, app);
    submenu_add_item(
        submenu, "Share to DeFlock", StartItemDeflockShare, recon_scene_start_submenu_cb, app);
    submenu_add_item(submenu, "Settings", StartItemSettings, recon_scene_start_submenu_cb, app);
    submenu_add_item(submenu, "About", StartItemAbout, recon_scene_start_submenu_cb, app);
    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, ReconSceneStart));
    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewSubmenu);
}

bool recon_scene_start_on_event(void* context, SceneManagerEvent event) {
    ReconApp* app = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeTick) {
        // Keep the fused score decaying and the header live even while idle on
        // the menu. Cheap: a snapshot + a little arithmetic on the 250 ms tick.
        uint8_t prev = app->watch.state;
        recon_app_watchscore_tick(app);
        if(app->watch.state != prev || app->watch.state != WatchStateClear) {
            recon_scene_start_update_header(app);
        }
        consumed = true;
    } else if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, ReconSceneStart, event.event);
        consumed = true;
        switch(event.event) {
        case StartItemFlock:
            scene_manager_next_scene(app->scene_manager, ReconSceneFlock);
            break;
        case StartItemGuardian:
            scene_manager_next_scene(app->scene_manager, ReconSceneGuardian);
            break;
        case StartItemLocator:
            scene_manager_next_scene(app->scene_manager, ReconSceneLocator);
            break;
        case StartItemFlockMap:
            scene_manager_next_scene(app->scene_manager, ReconSceneFlockMap);
            break;
        case StartItemWifi:
            scene_manager_next_scene(app->scene_manager, ReconSceneWifi);
            break;
        case StartItemBle:
            scene_manager_next_scene(app->scene_manager, ReconSceneBle);
            break;
        case StartItemNfc:
            scene_manager_next_scene(app->scene_manager, ReconSceneNfc);
            break;
        case StartItemFirmware:
            scene_manager_next_scene(app->scene_manager, ReconSceneFirmware);
            break;
        case StartItemReports:
            scene_manager_next_scene(app->scene_manager, ReconSceneReports);
            break;
        case StartItemDeflockShare:
            scene_manager_next_scene(app->scene_manager, ReconSceneDeflockHandoff);
            break;
        case StartItemSettings:
            scene_manager_next_scene(app->scene_manager, ReconSceneSettings);
            break;
        case StartItemAbout:
            scene_manager_next_scene(app->scene_manager, ReconSceneAbout);
            break;
        default:
            consumed = false;
            break;
        }
    }
    return consumed;
}

void recon_scene_start_on_exit(void* context) {
    ReconApp* app = context;
    submenu_reset(app->submenu);
}
