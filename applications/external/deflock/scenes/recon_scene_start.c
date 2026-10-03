// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
#include "../recon_app_i.h"
#include "../helpers/scan_session.h"

typedef enum {
    StartItemFlock,
    StartItemPreflight,
    StartItemFirmware,
    StartItemReports,
    StartItemSettings,
    StartItemAbout,
    StartItemFlockMap,
    StartItemSavedHits,
    StartItemSurvey,
    StartItemDeflockShare,
    StartItemLocator,
    StartItemSupport,
} StartItem;

static void recon_scene_start_submenu_cb(void* context, uint32_t index) {
    ReconApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

// The start-screen header. Marauder/Generic mode cannot do BLE, so it loses the
// BLE half of Flock detection -- say so rather than let a quiet screen imply
// full coverage.
static void recon_scene_start_update_header(ReconApp* app) {
    // Two different ways to end up Wi-Fi only: a scraper backend that cannot ask
    // for BLE, or a companion SoC with no Bluetooth radio to ask.
    bool wifi_only = (app->settings.backend != EspBackendCompanion) ||
                     recon_esp_chip_has_no_ble(app->esp_chip);
    // SHORT NAME WHEN THE WARNING IS ON. The full "FlipDeFlock v0.96 - WiFi only"
    // ran off the 128 px header and rendered as "...v0.96 - WiFi", dropping the
    // word that carries the meaning -- "WiFi" alone reads like a feature, "WiFi
    // only" reads like the limitation it is. The warning matters more here than
    // spelling the app's own name out, so the name gives way instead.
    submenu_set_header(
        app->submenu,
        wifi_only ? "FDF " RECON_VERSION " - WiFi only" : "FlipDeFlock " RECON_VERSION);
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
    submenu_add_item(
        submenu, "Health / Preflight", StartItemPreflight, recon_scene_start_submenu_cb, app);
    submenu_add_item(submenu, "Locator", StartItemLocator, recon_scene_start_submenu_cb, app);
    submenu_add_item(submenu, "Flock Map", StartItemFlockMap, recon_scene_start_submenu_cb, app);
    // Post-drive review: confirm what you went and looked at, name what you
    // worked out, bin the junk. Deliberately not buried in Reports -- going
    // through a drive's hits is a primary job, not an export option.
    submenu_add_item(submenu, "Saved Hits", StartItemSavedHits, recon_scene_start_submenu_cb, app);
    // What was in the air, matched or not. Sits next to Saved Hits because it is
    // the same post-drive question asked of the rows detection threw away -- and
    // for a camera on a randomised MAC, which no OUI table can ever match, it is
    // the only place it shows up at all (issue #25).
    submenu_add_item(submenu, "Air Survey", StartItemSurvey, recon_scene_start_submenu_cb, app);
    submenu_add_item(
        submenu, "ESP32 Firmware", StartItemFirmware, recon_scene_start_submenu_cb, app);
    submenu_add_item(submenu, "Reports", StartItemReports, recon_scene_start_submenu_cb, app);
    submenu_add_item(
        submenu, "Share to DeFlock", StartItemDeflockShare, recon_scene_start_submenu_cb, app);
    submenu_add_item(submenu, "Settings", StartItemSettings, recon_scene_start_submenu_cb, app);
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
        case StartItemPreflight:
            scene_manager_next_scene(app->scene_manager, ReconScenePreflight);
            break;
        case StartItemLocator:
            scene_manager_next_scene(app->scene_manager, ReconSceneLocator);
            break;
        case StartItemSavedHits:
            scene_manager_next_scene(app->scene_manager, ReconSceneSavedHits);
            break;
        case StartItemFlockMap:
            scene_manager_next_scene(app->scene_manager, ReconSceneFlockMap);
            break;
        case StartItemSurvey:
            scene_manager_next_scene(app->scene_manager, ReconSceneSurvey);
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
