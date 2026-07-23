// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "../recon_app_i.h"
#include "../helpers/esp_link.h"
#include "../helpers/scan_session.h"

void recon_scene_flock_map_on_enter(void* context) {
    ReconApp* app = context;

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->esp_connected = false;
    app->esp_deauths = 0;
    app->deauth_count = 0;
    app->esp_frames = 0; // per-session frame/hit counters start at 0...
    app->esp_hits = 0;
    app->esp_rebase =
        true; // ...and rebase off the companion's lifetime total (like flock/guardian)
    furi_mutex_release(app->mutex);

    // ESP first so it claims its UART (and disables the expansion manager) before
    // GPS. scan_session_start is idempotent (see scan_session.h / bug B1) and
    // returns true only on a FRESH start, so the dual-band kickoff is sent once.
    if(scan_session_start(app)) {
        // On the companion firmware, run dual-band (WiFi + BLE) Flock detection.
        // Marauder can't do this -> it stays WiFi-only via the generic backend.
        if(app->settings.backend == EspBackendCompanion) {
            esp_link_send(app->esp, "flockcombo");
        }
    }
    scan_session_gps_start(app);

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
    scan_session_stop(app);
}
