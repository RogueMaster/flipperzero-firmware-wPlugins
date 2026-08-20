// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
#include "../recon_app_i.h"

#include <string.h>

// Net Guardian -> Right -> this list: pick the network the Guardian is guarding.
//
// WHY THIS EXISTS. Untargeted, the Guardian answers "is anything around me under
// attack?". Left running on a desk in a flat, an office or a hotel that is mostly
// somebody else's traffic, and the operator learns to ignore it -- which is the
// failure mode the fused score was built to avoid in the first place. Targeted, it
// answers "is MY network under attack?", which is the question someone leaving a
// Flipper next to their router actually has.
//
// Only the NETWORK-shaped inputs are filtered (deauth attribution, evil twins).
// Flock, trackers, a Flipper nearby and attack-tool signatures are about the
// OPERATOR, not the network, so they keep contributing whatever the target is --
// filtering those on a BSSID would be meaningless.

#define GUARD_TARGET_MAX         32
#define GUARD_TARGET_CLEAR_INDEX 0xFFFFu

/**
 * Scene-scoped snapshot of the AP list, allocated on entry and freed on exit.
 *
 * Same reasoning as the Suspicious list next door: a static array would hold
 * this permanently for a screen the operator is almost never on, and ~5.9 KB
 * went that way once already. NULL is a supported state -- the list simply
 * renders empty rather than crashing.
 */
typedef struct {
    uint8_t bssid[6];
    char ssid[RECON_SSID_LEN];
} GuardTargetRow;

static GuardTargetRow* s_rows;
static size_t s_row_count;

static void guardian_target_cb(void* context, uint32_t index) {
    ReconApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void recon_scene_guardian_target_on_enter(void* context) {
    ReconApp* app = context;
    Submenu* submenu = app->submenu;
    submenu_reset(submenu);

    s_rows = malloc(sizeof(GuardTargetRow) * GUARD_TARGET_MAX);
    s_row_count = 0;

    // Snapshot the AP table under the lock, then build the menu unlocked: the
    // ESP worker writes wifi[] from its own thread and the submenu calls back
    // into the GUI.
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    if(s_rows) {
        for(size_t i = 0; i < app->wifi_count && s_row_count < GUARD_TARGET_MAX; i++) {
            const WifiAp* ap = &app->wifi[i];
            // An unnamed AP cannot be matched by SSID, so it could never catch an
            // evil twin -- half the feature. Still offered: deauth attribution
            // works on the BSSID alone, which is the other half.
            GuardTargetRow* r = &s_rows[s_row_count++];
            memcpy(r->bssid, ap->bssid, 6);
            strncpy(r->ssid, ap->ssid, RECON_SSID_LEN - 1);
            r->ssid[RECON_SSID_LEN - 1] = '\0';
        }
    }
    bool active = app->guard_active;
    char current[RECON_SSID_LEN];
    strncpy(current, app->guard_ssid, RECON_SSID_LEN - 1);
    current[RECON_SSID_LEN - 1] = '\0';
    furi_mutex_release(app->mutex);

    submenu_set_header(submenu, active ? "Guarding one network" : "Guarding everything");

    // "Guard everything" is always first and always present, so there is never a
    // state the operator cannot get out of -- including when the AP that was
    // targeted has since aged out of the table and cannot be re-selected.
    submenu_add_item(
        submenu,
        active ? "< Clear target (guard all)" : "* Guard everything (now)",
        GUARD_TARGET_CLEAR_INDEX,
        guardian_target_cb,
        app);

    if(s_row_count == 0) {
        submenu_add_item(submenu, "(no APs seen yet - run a scan)", 0, guardian_target_cb, app);
    } else {
        for(size_t i = 0; i < s_row_count; i++) {
            char label[40];
            const char* name = s_rows[i].ssid[0] ? s_rows[i].ssid : "(hidden)";
            bool is_current = active && strcmp(name, current) == 0;
            snprintf(
                label,
                sizeof(label),
                "%s%.18s  %02X:%02X",
                is_current ? "* " : "",
                name,
                s_rows[i].bssid[4],
                s_rows[i].bssid[5]);
            submenu_add_item(submenu, label, (uint32_t)i, guardian_target_cb, app);
        }
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewSubmenu);
}

bool recon_scene_guardian_target_on_event(void* context, SceneManagerEvent event) {
    ReconApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == GUARD_TARGET_CLEAR_INDEX) {
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        app->guard_active = false;
        app->guard_ssid[0] = '\0';
        memset(app->guard_bssid, 0, 6);
        furi_mutex_release(app->mutex);
    } else if(s_rows && event.event < s_row_count) {
        const GuardTargetRow* r = &s_rows[event.event];
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        memcpy(app->guard_bssid, r->bssid, 6);
        strncpy(app->guard_ssid, r->ssid, RECON_SSID_LEN - 1);
        app->guard_ssid[RECON_SSID_LEN - 1] = '\0';
        app->guard_active = true;
        furi_mutex_release(app->mutex);
    } else {
        return true; // the "no APs seen" placeholder row: consume, change nothing
    }

    // The target changes what the score MEANS, so start it over rather than
    // carrying a score earned against a different question.
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    watchscore_init(&app->watch);
    furi_mutex_release(app->mutex);
    recon_settings_save(app);
    scene_manager_previous_scene(app->scene_manager);
    return true;
}

void recon_scene_guardian_target_on_exit(void* context) {
    ReconApp* app = context;
    submenu_reset(app->submenu);
    free(s_rows);
    s_rows = NULL;
    s_row_count = 0;
}
