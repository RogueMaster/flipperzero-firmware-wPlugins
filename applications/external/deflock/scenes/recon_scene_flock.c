// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "../recon_app_i.h"
#include "../helpers/esp_link.h"
#include "../helpers/scan_session.h"

typedef enum {
    FlockCustomOpenDetail = 100,
} FlockCustomEvent;

static void recon_scene_flock_ok_cb(void* context, int selected_index) {
    ReconApp* app = context;
    app->selected = selected_index;
    view_dispatcher_send_custom_event(app->view_dispatcher, FlockCustomOpenDetail);
}

void recon_scene_flock_on_enter(void* context) {
    ReconApp* app = context;

    flock_view_reset(app->flock_view);
    flock_view_set_ok_callback(app->flock_view, recon_scene_flock_ok_cb, app);

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->esp_connected = false;
    app->esp_deauths = 0;
    app->deauth_count = 0;
    app->esp_frames = 0; // per-session frame/hit counters start at 0...
    app->esp_hits = 0;
    app->esp_rebase = true; // ...and rebase off the companion's lifetime total
    furi_mutex_release(app->mutex);

    // ESP first so it claims its UART (and disables the expansion manager) before
    // GPS. scan_session_start is idempotent across a detail-view Back re-entry
    // (see scan_session.h / bug B1) and returns true only on a FRESH start, so the
    // dual-band kickoff is sent exactly once.
    if(scan_session_start(app)) {
        // On the companion firmware, run dual-band (WiFi + BLE) Flock detection.
        // Marauder can't do this -> it stays WiFi-only via the generic backend.
        if(app->settings.backend == EspBackendCompanion) {
            esp_link_send(app->esp, "flockcombo");
        }
    }
    scan_session_gps_start(app);

    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewFlock);
}

bool recon_scene_flock_on_event(void* context, SceneManagerEvent event) {
    ReconApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeTick) {
        flock_view_refresh(app->flock_view);
        consumed = true;
    } else if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == FlockCustomOpenDetail) {
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            bool valid = app->selected >= 0 && app->selected < (int)app->flock_count;
            furi_mutex_release(app->mutex);
            if(valid) {
                scene_manager_next_scene(app->scene_manager, ReconSceneFlockDetail);
            }
            consumed = true;
        }
    }
    return consumed;
}

void recon_scene_flock_on_exit(void* context) {
    ReconApp* app = context;
    scan_session_stop(app);
}
