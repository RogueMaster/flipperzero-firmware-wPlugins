// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
#include "../recon_app_i.h"
#include "../helpers/esp_link.h"

#include <string.h>

// Net Guardian -> Right -> this list: pick the network the Guardian is guarding.
//
// WHY THIS EXISTS. Untargeted, the Guardian answers "is anything around me under
// attack?". Left on a desk in a flat, an office or a hotel that is mostly somebody
// else's traffic, and the operator learns to ignore it -- the failure mode the
// fused score was built to avoid. Targeted, it answers "is MY network under
// attack?", which is the question someone leaving a Flipper by their router has.
//
// Only the NETWORK-shaped inputs are filtered (deauth attribution, evil twins).
// Flock, trackers, a Flipper nearby and attack-tool signatures are about the
// OPERATOR, so they keep contributing whatever the target is.
//
// THE LIST SCANS FOR ITSELF. It used to render "(no APs seen yet - run a scan)"
// when the table was empty, which was wrong twice over: the submenu clipped it to
// "(no APs seen yet - run a s..." so the instruction was cut off mid-word, and
// selecting the row did nothing anyway. A screen that tells you to go do something
// else, in a sentence you cannot finish reading, is worse than an empty screen.
// There is now a "Scan for networks" row that actually runs the sweep, and the
// list repopulates in place as results arrive.

#define GUARD_TARGET_MAX         32
#define GUARD_TARGET_CLEAR_INDEX 0xFFFFu
#define GUARD_TARGET_SCAN_INDEX  0xFFFEu
// One companion `wifiscan` sweep is ~6 s. Hold the scanning state a little longer
// so the row does not flip back to "Scan" while results are still landing.
#define GUARD_TARGET_SCAN_MS 9000u

typedef struct {
    uint8_t bssid[6];
    char ssid[RECON_SSID_LEN];
} GuardTargetRow;

/**
 * Scene-scoped snapshot, allocated on entry and freed on exit.
 *
 * Same reasoning as the Suspicious list next door: a static array would hold this
 * permanently for a screen the operator is rarely on, and ~5.9 KB went that way
 * once already. NULL is a supported state -- the list renders without APs rather
 * than crashing.
 */
static GuardTargetRow* s_rows;
static size_t s_row_count;
static size_t s_shown_wifi; /**< wifi_count the menu was last built from */
static uint32_t s_scan_mark; /**< tick a scan was requested, 0 = not scanning */
static bool s_was_scanning; /**< last rendered scan state, so the row can flip back */

static void guardian_target_cb(void* context, uint32_t index) {
    ReconApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

/** True while a requested sweep is still expected to be delivering results. */
static bool guardian_target_scanning(void) {
    return s_scan_mark != 0 &&
           (furi_get_tick() - s_scan_mark) < furi_ms_to_ticks(GUARD_TARGET_SCAN_MS);
}

/**
 * Rebuild the whole submenu from the current AP table.
 *
 * Called on entry and again whenever the table grows, so a scan started from this
 * screen fills the list in place. Rebuilding resets the selection, which is why
 * the caller only does it when something actually changed -- otherwise the cursor
 * would jump under the operator's thumb on every tick.
 */
static void guardian_target_build(ReconApp* app) {
    Submenu* submenu = app->submenu;
    submenu_reset(submenu);

    s_row_count = 0;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    if(s_rows) {
        for(size_t i = 0; i < app->wifi_count && s_row_count < GUARD_TARGET_MAX; i++) {
            GuardTargetRow* r = &s_rows[s_row_count++];
            memcpy(r->bssid, app->wifi[i].bssid, 6);
            strncpy(r->ssid, app->wifi[i].ssid, RECON_SSID_LEN - 1);
            r->ssid[RECON_SSID_LEN - 1] = 0;
        }
    }
    s_shown_wifi = app->wifi_count;
    bool active = app->guard_active;
    bool have_esp = app->esp != NULL;
    char current[RECON_SSID_LEN];
    strncpy(current, app->guard_ssid, RECON_SSID_LEN - 1);
    current[RECON_SSID_LEN - 1] = 0;
    furi_mutex_release(app->mutex);

    submenu_set_header(submenu, active ? "Guarding one network" : "Guarding everything");

    // Always first, always present: there is never a state the operator cannot get
    // out of -- including when the targeted AP has aged out and cannot be reselected.
    submenu_add_item(
        submenu,
        active ? "Guard all (clear)" : "Guard all (now)",
        GUARD_TARGET_CLEAR_INDEX,
        guardian_target_cb,
        app);

    // Every label below is kept short enough to render whole. The submenu clips
    // with an ellipsis, and a half-read instruction is worse than none.
    if(!have_esp) {
        submenu_add_item(
            submenu, "No ESP32 - check wiring", GUARD_TARGET_SCAN_INDEX, guardian_target_cb, app);
    } else if(guardian_target_scanning()) {
        submenu_add_item(submenu, "Scanning...", GUARD_TARGET_SCAN_INDEX, guardian_target_cb, app);
    } else {
        submenu_add_item(
            submenu,
            s_row_count ? "Scan again" : "Scan for networks",
            GUARD_TARGET_SCAN_INDEX,
            guardian_target_cb,
            app);
    }

    for(size_t i = 0; i < s_row_count; i++) {
        char label[40];
        const char* name = s_rows[i].ssid[0] ? s_rows[i].ssid : "(hidden)";
        bool is_current = active && strcmp(name, current) == 0;
        // The last two BSSID bytes disambiguate two APs sharing a name (a mesh, or
        // the 2.4/5 GHz halves of one router) without spending width on all six.
        snprintf(
            label,
            sizeof(label),
            "%s%.14s %02X:%02X",
            is_current ? "*" : "",
            name,
            s_rows[i].bssid[4],
            s_rows[i].bssid[5]);
        submenu_add_item(submenu, label, (uint32_t)i, guardian_target_cb, app);
    }
}

void recon_scene_guardian_target_on_enter(void* context) {
    ReconApp* app = context;
    s_rows = malloc(sizeof(GuardTargetRow) * GUARD_TARGET_MAX);
    s_row_count = 0;
    s_scan_mark = 0;
    s_was_scanning = false;
    guardian_target_build(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewSubmenu);
}

bool recon_scene_guardian_target_on_event(void* context, SceneManagerEvent event) {
    ReconApp* app = context;

    // Repopulate in place as a sweep delivers, and flip the row back out of
    // "Scanning..." when the window closes. Gated on an actual change so the
    // cursor does not jump while the operator is choosing.
    if(event.type == SceneManagerEventTypeTick) {
        bool scanning = guardian_target_scanning();
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        size_t now_count = app->wifi_count;
        furi_mutex_release(app->mutex);
        if(now_count != s_shown_wifi || scanning != s_was_scanning) {
            s_was_scanning = scanning;
            guardian_target_build(app);
        }
        return true;
    }

    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == GUARD_TARGET_SCAN_INDEX) {
        // One-shot sweep on the link the Guardian already holds. Harmless to
        // re-send: the companion treats it as a mode select, and leaving this
        // scene re-arms the Guardian's own rotation from its on_enter.
        if(app->esp) {
            esp_link_send(app->esp, "wifiscan");
            s_scan_mark = furi_get_tick();
            if(s_scan_mark == 0) s_scan_mark = 1; // 0 is the "not scanning" sentinel
            s_was_scanning = true;
            guardian_target_build(app);
        }
        return true;
    }

    if(event.event == GUARD_TARGET_CLEAR_INDEX) {
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        app->guard_active = false;
        app->guard_ssid[0] = 0;
        memset(app->guard_bssid, 0, 6);
        furi_mutex_release(app->mutex);
    } else if(s_rows && event.event < s_row_count) {
        const GuardTargetRow* r = &s_rows[event.event];
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        memcpy(app->guard_bssid, r->bssid, 6);
        strncpy(app->guard_ssid, r->ssid, RECON_SSID_LEN - 1);
        app->guard_ssid[RECON_SSID_LEN - 1] = 0;
        app->guard_active = true;
        furi_mutex_release(app->mutex);
    } else {
        return true;
    }

    // The target changes what the score MEANS, so start it over rather than
    // carrying one earned against a different question.
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
    s_shown_wifi = 0;
    s_scan_mark = 0;
    s_was_scanning = false;
}
