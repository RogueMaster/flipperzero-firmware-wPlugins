// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
#include "../recon_app_i.h"
#include "../helpers/scan_session.h"

typedef enum {
    StartItemFlock,
    StartItemFirmware,
    StartItemReports,
    StartItemSettings,
    StartItemAbout,
    StartItemFlockMap,
    StartItemDeflockShare,
    StartItemLocator,
    StartItemSupport,
    StartItemHelp,
} StartItem;

static void recon_scene_start_submenu_cb(void* context, uint32_t index) {
    ReconApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

// The start-screen header. Marauder/Generic mode cannot do BLE, so it loses the
// BLE half of Flock detection -- say so rather than let a quiet screen imply
// full coverage.
static void recon_scene_start_update_header(ReconApp* app) {
    bool wifi_only = (app->settings.backend != EspBackendCompanion);
    submenu_set_header(
        app->submenu,
        wifi_only ? "FlipDeFlock " RECON_VERSION " - WiFi only" : "FlipDeFlock " RECON_VERSION);
}

void recon_scene_start_on_enter(void* context) {
    ReconApp* app = context;

    // Reaching the Main Menu is the ONE moment we know the user has genuinely
    // left a scan feature -- a List->Detail hop never comes through here. So
    // this is where the ESP/GPS link is released (freeing the UART for the
    // flasher) and the session's detections are persisted. A scan scene's
    // on_exit cannot do it: the SDK calls that on the way INTO a Detail child
    // too, which is what killed scanning mid-feature and wiped live tables on
    // Back (see helpers/scan_session.h). No-op at launch and on every menu
    // visit where no scan ran.
    scan_session_stop(app);

    Submenu* submenu = app->submenu;
    submenu_reset(submenu);
    recon_scene_start_update_header(app);
    submenu_add_item(
        submenu, "Flock / ALPR Detect", StartItemFlock, recon_scene_start_submenu_cb, app);
    submenu_add_item(submenu, "Locator", StartItemLocator, recon_scene_start_submenu_cb, app);
    submenu_add_item(submenu, "Flock Map", StartItemFlockMap, recon_scene_start_submenu_cb, app);
    submenu_add_item(
        submenu, "ESP32 Firmware", StartItemFirmware, recon_scene_start_submenu_cb, app);
    submenu_add_item(submenu, "Reports", StartItemReports, recon_scene_start_submenu_cb, app);
    submenu_add_item(
        submenu, "Share to DeFlock", StartItemDeflockShare, recon_scene_start_submenu_cb, app);
    submenu_add_item(submenu, "Settings", StartItemSettings, recon_scene_start_submenu_cb, app);
    submenu_add_item(submenu, "Help & Warnings", StartItemHelp, recon_scene_start_submenu_cb, app);
    submenu_add_item(submenu, "About", StartItemAbout, recon_scene_start_submenu_cb, app);
    submenu_add_item(submenu, "Support", StartItemSupport, recon_scene_start_submenu_cb, app);
    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, ReconSceneStart));
    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewSubmenu);
}

bool recon_scene_start_on_event(void* context, SceneManagerEvent event) {
    ReconApp* app = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, ReconSceneStart, event.event);
        consumed = true;
        switch(event.event) {
        case StartItemFlock:
            scene_manager_next_scene(app->scene_manager, ReconSceneFlock);
            break;
        case StartItemLocator:
            scene_manager_next_scene(app->scene_manager, ReconSceneLocator);
            break;
        case StartItemFlockMap:
            scene_manager_next_scene(app->scene_manager, ReconSceneFlockMap);
            break;
            break;
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
        case StartItemHelp:
            scene_manager_next_scene(app->scene_manager, ReconSceneHelp);
            break;
        case StartItemAbout:
            scene_manager_next_scene(app->scene_manager, ReconSceneAbout);
            break;
        case StartItemSupport:
            scene_manager_next_scene(app->scene_manager, ReconSceneSupport);
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
