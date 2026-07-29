// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "../recon_app_i.h"

#include <string.h>

// The screen itself lives in views/flock_detail_view.c (a canvas view, so the
// RSSI row can draw the same graphical bars as the list -- GitHub issue #5).
// This scene owns only the two actions those buttons trigger.

typedef enum {
    DetailCustomToggleMark = 200,
    DetailCustomLockIn = 201,
} DetailCustomEvent;

static void recon_scene_flock_detail_mark_cb(void* context) {
    ReconApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, DetailCustomToggleMark);
}

static void recon_scene_flock_detail_lock_cb(void* context) {
    ReconApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, DetailCustomLockIn);
}

void recon_scene_flock_detail_on_enter(void* context) {
    ReconApp* app = context;
    flock_detail_view_reset(app->flock_detail_view); // a new selection starts at the top
    flock_detail_view_set_callbacks(
        app->flock_detail_view,
        recon_scene_flock_detail_mark_cb,
        recon_scene_flock_detail_lock_cb,
        app);
    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewFlockDetail);
}

bool recon_scene_flock_detail_on_event(void* context, SceneManagerEvent event) {
    ReconApp* app = context;

    if(event.type == SceneManagerEventTypeTick) {
        // The parent scan scene is only suspended, not exited, so the ESP worker
        // keeps feeding this entry -- repaint so RSSI and the sighting count are
        // live while you are looking at them. The suspended parent stops getting
        // ticks, so announce alerts here too (as every other scan scene does),
        // otherwise a hit found while you read this screen waits for you to leave.
        recon_app_alert_tick(app);
        flock_detail_view_refresh(app->flock_detail_view);
        return true;
    }
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == DetailCustomToggleMark) {
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        if(app->selected >= 0 && app->selected < (int)app->flock_count) {
            app->flock[app->selected].marked = !app->flock[app->selected].marked;
        }
        furi_mutex_release(app->mutex);
        flock_detail_view_refresh(app->flock_detail_view);
        return true;
    }

    if(event.event == DetailCustomLockIn) {
        // Copy this entry into the Locator target slots -- the same fields the
        // Locator's own marked-device picker fills, so the homing HUD needs no
        // special case. Kind/channel follow that picker exactly: a BLE sighting
        // ('L') homes on the BLE radio, everything else on Wi-Fi at its channel.
        bool valid = false;
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        if(app->selected >= 0 && app->selected < (int)app->flock_count) {
            const FlockEntry* e = &app->flock[app->selected];
            memcpy(app->locate_mac, e->mac, 6);
            app->locate_kind = (e->ftype == 'L') ? 'b' : 'w';
            app->locate_ch = e->channel;
            if(e->ssid[0]) {
                snprintf(app->locate_label, sizeof(app->locate_label), "Flock %s", e->ssid);
            } else {
                snprintf(
                    app->locate_label,
                    sizeof(app->locate_label),
                    "Flock %02X%02X%02X",
                    e->mac[3],
                    e->mac[4],
                    e->mac[5]);
            }
            valid = true;
        }
        furi_mutex_release(app->mutex);
        // The Locator is companion-only; navigate unconditionally and let
        // ReconSceneLocatorHome show its own Marauder guard screen rather than
        // duplicating that check here. Backing out of the HUD lands back on this
        // screen, and one more Back returns to the Flock list, whose on_enter
        // restarts the general sweep (the Locator's on_exit stopped the scan
        // session) -- the round trip issue #6 asked for.
        if(valid) scene_manager_next_scene(app->scene_manager, ReconSceneLocatorHome);
        return true;
    }
    return false;
}

void recon_scene_flock_detail_on_exit(void* context) {
    UNUSED(context);
}
