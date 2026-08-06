// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
#include "../recon_app_i.h"
#include "../helpers/esp_link.h"
#include "../helpers/scan_session.h"

void recon_scene_flock_map_on_enter(void* context) {
    ReconApp* app = context;

    // ESP first so it claims its UART (and disables the expansion manager) before
    // GPS. This scene opens no child scene, so in practice it is always entered
    // fresh -- it was never affected by the B7 data-loss bug. The gate is kept
    // anyway so all five scan scenes read the same way and none of them has to
    // be the one that "happens to be safe".
    if(scan_session_start(app)) {
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        app->esp_connected = false;
        app->esp_deauths = 0;
        app->deauth_count = 0;
        app->esp_frames = 0; // per-session frame/hit counters start at 0...
        app->esp_hits = 0;
        app->esp_rebase =
            true; // ...and rebase off the companion's lifetime total (like flock/guardian)
        furi_mutex_release(app->mutex);
    }

    // Kickoff on every enter, for the same reason as the Flock list: an
    // idempotent mode-select costs nothing and never leaves the board idle.
    // Marauder can't do dual-band -> it stays WiFi-only via the generic backend.
    if(app->settings.backend == EspBackendCompanion) {
        esp_link_send(app->esp, "flockcombo");
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
    UNUSED(context);
    // No teardown here either -- the Main Menu's on_enter owns it, uniformly for
    // every scan scene (see helpers/scan_session.h).
}
