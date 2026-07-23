// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "../recon_app_i.h"
#include "../helpers/esp_link.h"
#include "../helpers/scan_session.h"
#include "../helpers/scene_util.h"
#include "../helpers/wifi_audit.h"
#include "../helpers/recon_report.h"

#include <string.h>

// Custom-event ids for the action rows and (offset) AP rows. The list view
// reports a raw row index; the scene maps action rows first, then AP rows.
#define WIFI_ACTION_COUNT 2 // [0] = Rescan, [1] = Save Report
#define WIFI_EV_RESCAN    0
#define WIFI_EV_SAVE      1
#define WIFI_EV_AP        100 // + AP index

#define WIFI_SCAN_TIMEOUT_MS 9000

// GUI-thread-only scene state (one WiFi-audit scene at a time).

// Action-row labels (static lifetime; the view keeps the pointers).
static const char* const WIFI_ACTIONS_SCAN[] = {"Rescan"};
static const char* const WIFI_ACTIONS_RESULTS[] = {"Rescan", "Save Report"};

// The list view reports the raw selected row index; map it to a custom event.
static void wifi_view_ok_cb(void* context, int selected_index) {
    ReconApp* app = context;
    // Action rows come first, then AP rows. The action-row count is state-
    // dependent: scanning/timeout show only [0]=Rescan; results also has [1]=Save.
    // Using the fixed WIFI_ACTION_COUNT put every AP row off by one whenever fewer
    // than two actions were shown.
    int actions = (app->wifi_ui_state == 1) ? 2 : 1; // results state has Save; others don't
    uint32_t ev;
    if(selected_index == 0) {
        ev = WIFI_EV_RESCAN;
    } else if(actions == 2 && selected_index == 1) {
        ev = WIFI_EV_SAVE;
    } else {
        ev = WIFI_EV_AP + (uint32_t)(selected_index - actions);
    }
    view_dispatcher_send_custom_event(app->view_dispatcher, ev);
}

static void recon_scene_wifi_show_scanning(ReconApp* app) {
    // One action row (Rescan); pressing OK during the scan retriggers (today the
    // lone "Scanning" submenu item carried WIFI_ITEM_RESCAN).
    wifi_list_view_set_actions(app->wifi_list_view, WIFI_ACTIONS_SCAN, 1);
    wifi_list_view_set_right(app->wifi_list_view, NULL);
    wifi_list_view_set_header(app->wifi_list_view, "WiFi Audit");
    wifi_list_view_set_status(app->wifi_list_view, "Scanning... please wait");
    wifi_list_view_reset(app->wifi_list_view);
    wifi_list_view_refresh(app->wifi_list_view);
}

static void recon_scene_wifi_show_timeout(ReconApp* app) {
    wifi_list_view_set_actions(app->wifi_list_view, WIFI_ACTIONS_SCAN, 1); // just Rescan
    wifi_list_view_set_right(app->wifi_list_view, NULL);
    wifi_list_view_set_header(app->wifi_list_view, "WiFi Audit - no data");
    wifi_list_view_set_status(app->wifi_list_view, "no data (needs companion FW)");
    wifi_list_view_reset(app->wifi_list_view);
    wifi_list_view_refresh(app->wifi_list_view);
}

/** Sort the AP list worst-first (by grade, then signal) and push to the view. */
static void recon_scene_wifi_show_results(ReconApp* app) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    size_t n = app->wifi_count;
    // Grade each AP ONCE up front -- wifi_audit_grade is a pure function of
    // (authmode,pairwise,wps,ssid), so caching it turns the old O(n^2) grade
    // recompute (it was called inside the insertion loop) into a single O(n)
    // pass, shrinking the work done while app->mutex is held.
    static WifiGrade grades[RECON_WIFI_MAX];
    for(size_t i = 0; i < n; i++) {
        grades[i] = wifi_audit_grade(
            app->wifi[i].authmode,
            app->wifi[i].pairwise,
            app->wifi[i].wps,
            app->wifi[i].ssid,
            NULL);
    }
    // Insertion sort in place (grade desc, then rssi desc), keeping grades[] aligned.
    for(size_t i = 1; i < n; i++) {
        WifiAp key = app->wifi[i];
        WifiGrade kg = grades[i];
        int j = (int)i - 1;
        while(j >= 0 && !(grades[j] > kg || (grades[j] == kg && app->wifi[j].rssi >= key.rssi))) {
            app->wifi[j + 1] = app->wifi[j];
            grades[j + 1] = grades[j];
            j--;
        }
        app->wifi[j + 1] = key;
        grades[j + 1] = kg;
    }
    // dup/rogue (evil-twin) flags are computed once at scan completion in
    // recon_app_wifi_end (so Net Guardian sees them too); tally from the cache.
    int crit = 0, weak = 0, twin = 0;
    for(size_t i = 0; i < n; i++) {
        if(grades[i] == WifiGradeCritical)
            crit++;
        else if(grades[i] == WifiGradeWeak)
            weak++;
        if(app->wifi[i].dup || app->wifi[i].rogue) twin++;
    }
    furi_mutex_release(app->mutex);

    snprintf(
        app->text_store,
        RECON_TEXT_STORE,
        "%u AP  %dcrit %dweak %dtwin",
        (unsigned)n,
        crit,
        weak,
        twin);

    char right[14];
    snprintf(right, sizeof(right), "%uAP", (unsigned)n);

    wifi_list_view_set_actions(app->wifi_list_view, WIFI_ACTIONS_RESULTS, WIFI_ACTION_COUNT);
    wifi_list_view_set_right(app->wifi_list_view, right);
    wifi_list_view_set_header(app->wifi_list_view, app->text_store);
    wifi_list_view_set_status(app->wifi_list_view, NULL);
    wifi_list_view_reset(app->wifi_list_view);
    wifi_list_view_refresh(app->wifi_list_view);
}

static void recon_scene_wifi_trigger(ReconApp* app) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->wifi_count = 0;
    app->wifi_done = false;
    app->wifi_scanning = false;
    furi_mutex_release(app->mutex);
    if(app->esp) esp_link_send(app->esp, "wifiscan");
    app->wifi_ui_state = 0;
    app->wifi_scan_start = furi_get_tick();
    recon_scene_wifi_show_scanning(app);
}

void recon_scene_wifi_on_enter(void* context) {
    ReconApp* app = context;

    // The WiFi audit relies on the companion firmware protocol (the only way to
    // get auth/cipher/WPS). In Marauder mode it can't work -> explain instead of
    // showing a dead screen.
    app->saved_backend = app->settings.backend;
    if(app->settings.backend != EspBackendCompanion) {
        app->wifi_blocked = true;
        scene_show_companion_guard(
            app,
            "WiFi Audit needs the\nFlipDeFlock companion FW.\n\nYou're in Marauder mode\n(Flock detect only).\nFlash via 'ESP32 Firmware'\nor switch Board Mode in\nSettings.");
        return;
    }
    app->wifi_blocked = false;

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->esp_connected = false;
    furi_mutex_release(app->mutex);

    wifi_list_view_set_ok_callback(app->wifi_list_view, wifi_view_ok_cb, app);

    // scan_session_start keeps the live link across a detail-view round-trip
    // (idempotent; see scan_session.h / bug B1).
    scan_session_start(app);

    recon_scene_wifi_trigger(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewWifiList);
}

bool recon_scene_wifi_on_event(void* context, SceneManagerEvent event) {
    ReconApp* app = context;
    bool consumed = false;

    if(app->wifi_blocked) return false; // Marauder mode guard screen; let Back exit

    if(event.type == SceneManagerEventTypeTick) {
        if(app->wifi_ui_state == 0) {
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            bool done = app->wifi_done;
            furi_mutex_release(app->mutex);
            if(done) {
                recon_scene_wifi_show_results(app);
                app->wifi_ui_state = 1;
            } else if(furi_get_tick() - app->wifi_scan_start > WIFI_SCAN_TIMEOUT_MS) {
                recon_scene_wifi_show_timeout(app);
                app->wifi_ui_state = 2;
            }
        }
        // Keep the live view ticking (bars, selection redraw).
        wifi_list_view_refresh(app->wifi_list_view);
        consumed = true;
    } else if(event.type == SceneManagerEventTypeCustom) {
        uint32_t id = event.event;
        if(id == WIFI_EV_RESCAN) {
            recon_scene_wifi_trigger(app);
            consumed = true;
        } else if(id == WIFI_EV_SAVE) {
            char path[128] = {0};
            bool ok = recon_report_save_wifi(app, path, sizeof(path));
            scene_report_notify(app, ok);
            consumed = true;
        } else if(id >= WIFI_EV_AP) {
            int idx = (int)id - WIFI_EV_AP;
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            bool valid = idx >= 0 && idx < (int)app->wifi_count;
            furi_mutex_release(app->mutex);
            if(valid) {
                app->wifi_selected = idx;
                scene_manager_next_scene(app->scene_manager, ReconSceneWifiDetail);
            }
            consumed = true;
        }
    }
    return consumed;
}

void recon_scene_wifi_on_exit(void* context) {
    ReconApp* app = context;
    scan_session_stop(app);
    app->settings.backend = app->saved_backend; // restore Flock backend choice
}
