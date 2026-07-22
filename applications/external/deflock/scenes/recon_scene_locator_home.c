// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "../recon_app_i.h"
#include "../helpers/esp_link.h"
#include "../helpers/scan_session.h"
#include "../helpers/scene_util.h"

// Locator step 2: the homing HUD. Tells the companion to stream live RSSI for
// the selected target (`locate <w|b> <mac> <ch>`) and shows the hot/cold meter.
// Companion-only: the generic/Marauder backend has no `locate` command.

void recon_scene_locator_home_on_enter(void* context) {
    ReconApp* app = context;

    if(app->settings.backend != EspBackendCompanion) {
        app->locator_blocked = true;
        scene_show_companion_guard(
            app,
            "Locator needs the\nFlipDeFlock companion FW\n(live signal homing).\n\nYou're in Marauder mode.\nFlash via 'ESP32 Firmware'\nor switch Board Mode in\nSettings.");
        return;
    }
    app->locator_blocked = false;

    // Fresh reading state, and snapshot the target out of the lock.
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->locate_have = false;
    app->locate_rssi = 0;
    app->locate_tick = 0;
    app->locate_init = false; // reset the peak-hold fold (now lives in ReconApp)
    app->locate_peak = -128;
    app->esp_connected = false;
    uint8_t kind = app->locate_kind;
    uint8_t ch = app->locate_ch;
    uint8_t mac[6];
    memcpy(mac, app->locate_mac, 6);
    furi_mutex_release(app->mutex);

    locator_view_reset(app->locator_view);

    // ESP first so it claims its UART; GPS only if on a different port (the
    // homing meter works without it -- GPS only adds the "strongest here" note).
    // scan_session_start is idempotent across re-entry (see scan_session.h); the
    // target `locate` command below is (re)sent every enter regardless, so the
    // companion always homes the currently selected device.
    scan_session_start(app);
    char cmd[40];
    snprintf(
        cmd,
        sizeof(cmd),
        "locate %c %02x%02x%02x%02x%02x%02x %u",
        kind == 'b' ? 'b' : 'w',
        mac[0],
        mac[1],
        mac[2],
        mac[3],
        mac[4],
        mac[5],
        ch);
    esp_link_send(app->esp, cmd);

    scan_session_gps_start(app);

    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewLocator);
}

bool recon_scene_locator_home_on_event(void* context, SceneManagerEvent event) {
    ReconApp* app = context;
    if(app->locator_blocked) return false; // guard screen: let Back exit
    if(event.type == SceneManagerEventTypeTick) {
        locator_view_refresh(app->locator_view);
        return true;
    }
    return false;
}

void recon_scene_locator_home_on_exit(void* context) {
    ReconApp* app = context;
    if(app->esp) esp_link_send(app->esp, "stop"); // end locate mode on the companion
    scan_session_stop(app);
    widget_reset(app->widget);
}
