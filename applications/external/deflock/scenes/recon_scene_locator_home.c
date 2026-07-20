// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "../recon_app_i.h"
#include "../helpers/esp_link.h"
#include "../helpers/gps_link.h"

// Locator step 2: the homing HUD. Tells the companion to stream live RSSI for
// the selected target (`locate <w|b> <mac> <ch>`) and shows the hot/cold meter.
// Companion-only: the generic/Marauder backend has no `locate` command.

static bool s_blocked;

static void locator_home_show_guard(ReconApp* app) {
    widget_reset(app->widget);
    widget_add_text_scroll_element(
        app->widget,
        0,
        0,
        128,
        64,
        "Locator needs the\nFlipDeFlock companion FW\n(live signal homing).\n\nYou're in Marauder mode.\nFlash via 'ESP32 Firmware'\nor switch Board Mode in\nSettings.");
    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewWidget);
}

void recon_scene_locator_home_on_enter(void* context) {
    ReconApp* app = context;

    if(app->settings.backend != EspBackendCompanion) {
        s_blocked = true;
        locator_home_show_guard(app);
        return;
    }
    s_blocked = false;

    // Fresh reading state, and snapshot the target out of the lock.
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->locate_have = false;
    app->locate_rssi = 0;
    app->locate_tick = 0;
    app->esp_connected = false;
    uint8_t kind = app->locate_kind;
    uint8_t ch = app->locate_ch;
    uint8_t mac[6];
    memcpy(mac, app->locate_mac, 6);
    furi_mutex_release(app->mutex);

    locator_view_reset(app->locator_view);

    // ESP first so it claims its UART; GPS only if on a different port (the
    // homing meter works without it -- GPS only adds the "strongest here" note).
    // Re-entry guard for consistency with the scan scenes; the target `locate`
    // command below is (re)sent every enter so the companion always homes the
    // currently selected device.
    if(!app->esp) {
        app->esp = esp_link_alloc(app);
        esp_link_start(app->esp);
    }
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

    if(app->settings.gps_enabled && app->settings.gps_uart != app->settings.esp_uart) {
        if(!app->gps) {
            app->gps = gps_link_alloc(app);
            gps_link_start(app->gps);
        }
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewLocator);
}

bool recon_scene_locator_home_on_event(void* context, SceneManagerEvent event) {
    ReconApp* app = context;
    if(s_blocked) return false; // guard screen: let Back exit
    if(event.type == SceneManagerEventTypeTick) {
        locator_view_refresh(app->locator_view);
        return true;
    }
    return false;
}

void recon_scene_locator_home_on_exit(void* context) {
    ReconApp* app = context;
    if(app->esp) {
        esp_link_send(app->esp, "stop"); // end locate mode on the companion
        esp_link_stop(app->esp);
        esp_link_free(app->esp);
        app->esp = NULL;
    }
    if(app->gps) {
        gps_link_stop(app->gps);
        gps_link_free(app->gps);
        app->gps = NULL;
    }
    widget_reset(app->widget);
}
