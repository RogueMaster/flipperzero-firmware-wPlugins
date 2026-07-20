// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "../recon_app_i.h"
#include "../helpers/esp_link.h"
#include "../helpers/gps_link.h"

void recon_scene_flock_map_on_enter(void* context) {
    ReconApp* app = context;

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->esp_connected = false;
    app->esp_deauths = 0;
    app->deauth_count = 0;
    furi_mutex_release(app->mutex);

    // ESP first so it claims its UART (and disables the expansion manager)
    // before GPS. GPS only starts if it's on a *different* port -- otherwise it
    // would steal the ESP's UART and silently kill detection.
    // Re-entry guard for consistency with the scan scenes (this scene has no
    // detail child today, but guard anyway so it can't leak if that changes).
    if(!app->esp) {
        app->esp = esp_link_alloc(app);
        esp_link_start(app->esp);
        // On the companion firmware, run dual-band (WiFi + BLE) Flock detection.
        // Marauder can't do this -> it stays WiFi-only via the generic backend.
        if(app->settings.backend == EspBackendCompanion) {
            esp_link_send(app->esp, "flockcombo");
        }
    }
    if(app->settings.gps_enabled && app->settings.gps_uart != app->settings.esp_uart) {
        if(!app->gps) {
            app->gps = gps_link_alloc(app);
            gps_link_start(app->gps);
        }
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewFlockMap);
}

bool recon_scene_flock_map_on_event(void* context, SceneManagerEvent event) {
    ReconApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeTick) {
        flock_map_view_refresh(app->flock_map_view);
        consumed = true;
    }
    return consumed;
}

void recon_scene_flock_map_on_exit(void* context) {
    ReconApp* app = context;
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
