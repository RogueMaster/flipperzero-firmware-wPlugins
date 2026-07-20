// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "../recon_app_i.h"
#include "../helpers/esp_link.h"
#include "../helpers/gps_link.h"

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

    // ESP first so it claims its UART (and disables the expansion manager)
    // before GPS. GPS only starts if it's on a *different* port -- otherwise it
    // would steal the ESP's UART and silently kill detection.
    // Re-entry guard: scene_manager_next_scene (opening a detail view) SUSPENDS
    // this scene without calling on_exit, and pressing Back re-runs on_enter.
    // Re-allocating over the still-live link would leak it (heap + worker stack)
    // and orphan the UART, so the replacement can't acquire it and scanning dies.
    // Alloc only when there is no live link; on_exit frees and NULLs it.
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
        if(!app->gps) { // same re-entry guard as the ESP link above
            app->gps = gps_link_alloc(app);
            gps_link_start(app->gps);
        }
    }

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
