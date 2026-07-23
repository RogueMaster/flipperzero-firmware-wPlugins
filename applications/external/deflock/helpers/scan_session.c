// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "scan_session.h"
#include "../recon_app_i.h"
#include "esp_link.h"
#include "gps_link.h"

bool scan_session_start(void* _app) {
    ReconApp* app = _app;
    if(app->esp) return false; // already live (a Back re-entry) -> keep it, don't leak
    app->esp = esp_link_alloc(app);
    esp_link_start(app->esp);
    return true;
}

void scan_session_gps_start(void* _app) {
    ReconApp* app = _app;
    if(app->gps) return; // already running
    if(!app->settings.gps_enabled) return;
    if(app->settings.gps_uart == app->settings.esp_uart) return; // would steal the ESP's UART
    app->gps = gps_link_alloc(app);
    gps_link_start(app->gps);
}

void scan_session_stop(void* _app) {
    ReconApp* app = _app;
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
}
