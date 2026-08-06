// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
#include "scan_session.h"
#include "../recon_app_i.h"
#include "esp_link.h"
#include "gps_link.h"

bool scan_session_start(void* _app) {
    ReconApp* app = _app;
    if(app->esp) return false; // already live (a Back re-entry) -> keep it, don't leak
    app->esp = esp_link_alloc(app);
    esp_link_start(app->esp);

    // Configure the companion's GPS relay once, centrally, for every screen that
    // opens a session -- rather than in each scene, which is how the alert
    // delivery bug happened. The command is built in esp_link so that this call
    // and the on-banner re-send cannot drift apart; it is a no-op on Marauder.
    esp_link_send_band(app->esp);
    esp_link_send_gps_cfg(app->esp);
    return true;
}

void scan_session_gps_start(void* _app) {
    ReconApp* app = _app;
    if(app->gps) return; // already running
    if(!app->settings.gps_enabled) return;
    // Companion source: the fix arrives as `G,<nmea>` on the ESP link, so there
    // is no second UART to open here at all. Opening one would take a port for
    // nothing (and on a single-UART wiring, take the ESP's).
    if(app->settings.gps_source == ReconGpsSourceCompanion) return;
    if(app->settings.gps_uart == app->settings.esp_uart) return; // would steal the ESP's UART
    app->gps = gps_link_alloc(app);
    gps_link_start(app->gps);
}

void scan_session_stop(void* _app) {
    ReconApp* app = _app;
    // Nothing running -> nothing to tear down, and crucially nothing to persist.
    // This is called from recon_scene_start_on_enter(), which also runs once at
    // launch before any scan has happened; without this guard that first call
    // would write an EMPTY table straight over the hits.csv recon_hits_load()
    // had just restored -- issue #5's exact failure, from the other direction.
    if(!app->esp && !app->gps) return;

    if(app->esp) {
        esp_link_stop(app->esp);
        esp_link_free(app->esp);
        app->esp = NULL;
    }
    if(app->gps) {
        gps_link_stop(app->gps);
        gps_link_free(app->gps);
        app->gps = NULL;
    }
    // Persist the detections this scan collected (opt-in; a no-op when the
    // setting is off). Both call sites -- the Main Menu's on_enter and the app
    // teardown -- funnel through here, so one call site still covers Flock,
    // Flock Map, Net Guardian, WiFi, BLE and Locator, including the case that
    // prompted the request: backing straight out of the app after a scan. Done
    // AFTER the links are torn down, so the write can't race a still-running
    // ESP worker.
    recon_hits_save(app);
}
