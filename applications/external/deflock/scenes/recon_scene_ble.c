// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "../recon_app_i.h"
#include "../helpers/esp_link.h"
#include "../helpers/recon_report.h"

#include <string.h>

// Custom-event ids for the action rows and (offset) device rows. The list view
// reports a raw row index; the scene maps action rows first, then device rows.
#define BLE_ACTION_COUNT 1 // [0] = Save Report
#define BLE_EV_SAVE      0
#define BLE_EV_DEVICE    100 // + device index

#define BLE_RESCAN_GAP_MS   4000
#define BLE_SCAN_TIMEOUT_MS 12000

static bool s_pending; // a blescan is in flight (awaiting BEND)
static uint32_t s_mark; // tick of last state transition

// Action-row labels (static lifetime; the view keeps the pointers).
static const char* const BLE_ACTIONS[] = {"Save Report"};

static int ble_rank(const BleDevice* d) {
    // following > tracker-category > plain BLE
    return (d->following ? 2 : 0) + (d->cat != BleCatUnknown ? 1 : 0);
}

// The list view reports the raw selected row index; map it to a custom event.
static void ble_view_ok_cb(void* context, int selected_index) {
    ReconApp* app = context;
    uint32_t ev;
    if(selected_index < BLE_ACTION_COUNT) {
        ev = BLE_EV_SAVE; // [0] Save Report
    } else {
        ev = BLE_EV_DEVICE + (uint32_t)(selected_index - BLE_ACTION_COUNT);
    }
    view_dispatcher_send_custom_event(app->view_dispatcher, ev);
}

static void ble_trigger(ReconApp* app) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->ble_done = false;
    furi_mutex_release(app->mutex);
    if(app->esp) esp_link_send(app->esp, "blescan");
    s_pending = true;
    s_mark = furi_get_tick();
}

static void ble_show_scanning(ReconApp* app) {
    ble_list_view_set_actions(app->ble_list_view, BLE_ACTIONS, BLE_ACTION_COUNT);
    ble_list_view_set_right(app->ble_list_view, NULL);
    ble_list_view_set_header(app->ble_list_view, "BLE / Tracker scan");
    ble_list_view_set_status(app->ble_list_view, "Scanning (~6s)...");
    ble_list_view_reset(app->ble_list_view);
    ble_list_view_refresh(app->ble_list_view);
}

static void ble_show_results(ReconApp* app) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    size_t n = app->ble_count;
    // Insertion sort in place: rank desc, then rssi desc.
    for(size_t i = 1; i < n; i++) {
        BleDevice key = app->ble[i];
        int kr = ble_rank(&key);
        int j = (int)i - 1;
        while(j >= 0) {
            BleDevice* a = &app->ble[j];
            int ar = ble_rank(a);
            if(ar > kr || (ar == kr && a->rssi >= key.rssi)) break;
            app->ble[j + 1] = app->ble[j];
            j--;
        }
        app->ble[j + 1] = key;
    }
    int track = 0, follow = 0;
    for(size_t i = 0; i < n; i++) {
        // "trk" counts trackers; a Flipper is a recon tool, not a tracker.
        if(app->ble[i].cat != BleCatUnknown && app->ble[i].cat != BleCatFlipper) track++;
        if(app->ble[i].following) follow++;
    }
    furi_mutex_release(app->mutex);

    snprintf(
        app->text_store, RECON_TEXT_STORE, "BLE %u  trk %d  follow %d", (unsigned)n, track, follow);

    char right[14];
    snprintf(right, sizeof(right), "n%u", (unsigned)n);

    ble_list_view_set_actions(app->ble_list_view, BLE_ACTIONS, BLE_ACTION_COUNT);
    ble_list_view_set_right(app->ble_list_view, right);
    ble_list_view_set_header(app->ble_list_view, app->text_store);
    ble_list_view_set_status(app->ble_list_view, NULL);
    ble_list_view_reset(app->ble_list_view);
    ble_list_view_refresh(app->ble_list_view);
}

static bool s_ble_blocked; // companion-only feature opened in Marauder mode

static void ble_show_guard(ReconApp* app) {
    widget_reset(app->widget);
    widget_add_text_scroll_element(
        app->widget,
        0,
        0,
        128,
        64,
        "BLE / Tracker Scan needs\nthe FlipDeFlock companion\nFW.\n\nYou're in Marauder mode\n(Flock detect only).\nFlash via 'ESP32 Firmware'\nor switch Board Mode in\nSettings.");
}

void recon_scene_ble_on_enter(void* context) {
    ReconApp* app = context;
    // BLE scan needs the companion firmware protocol; explain in Marauder mode.
    app->saved_backend = app->settings.backend;
    if(app->settings.backend != EspBackendCompanion) {
        s_ble_blocked = true;
        ble_show_guard(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewWidget);
        return;
    }
    s_ble_blocked = false;

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->ble_count = 0;
    app->ble_done = false;
    app->esp_connected = false;
    furi_mutex_release(app->mutex);

    ble_list_view_set_ok_callback(app->ble_list_view, ble_view_ok_cb, app);

    // Re-entry guard: keep the live link across a detail-view round-trip
    // (see recon_scene_flock_on_enter for the full rationale).
    if(!app->esp) {
        app->esp = esp_link_alloc(app);
        esp_link_start(app->esp);
    }

    ble_show_scanning(app);
    ble_trigger(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewBleList);
}

bool recon_scene_ble_on_event(void* context, SceneManagerEvent event) {
    ReconApp* app = context;
    bool consumed = false;

    if(s_ble_blocked) return false; // Marauder mode guard screen; let Back exit

    if(event.type == SceneManagerEventTypeTick) {
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        bool done = app->ble_done;
        furi_mutex_release(app->mutex);
        uint32_t now = furi_get_tick();
        if(s_pending && done) {
            ble_show_results(app);
            s_pending = false;
            s_mark = now;
        } else if(s_pending && now - s_mark > BLE_SCAN_TIMEOUT_MS) {
            s_pending = false;
            s_mark = now; // give up; allow a rescan
        } else if(!s_pending && now - s_mark > BLE_RESCAN_GAP_MS) {
            ble_trigger(app); // continuous monitoring -> "following" detection
        }
        // Keep the live view ticking (counts, bars, following flag).
        ble_list_view_refresh(app->ble_list_view);
        consumed = true;
    } else if(event.type == SceneManagerEventTypeCustom) {
        uint32_t id = event.event;
        if(id == BLE_EV_SAVE) {
            char path[128] = {0};
            bool ok = recon_report_save_ble(app, path, sizeof(path));
            if(app->settings.sound) {
                notification_message(app->notifications, ok ? &sequence_success : &sequence_error);
            }
            consumed = true;
        } else if(id >= BLE_EV_DEVICE) {
            int idx = (int)id - BLE_EV_DEVICE;
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            bool valid = idx >= 0 && idx < (int)app->ble_count;
            furi_mutex_release(app->mutex);
            if(valid) {
                app->ble_selected = idx;
                scene_manager_next_scene(app->scene_manager, ReconSceneBleDetail);
            }
            consumed = true;
        }
    }
    return consumed;
}

void recon_scene_ble_on_exit(void* context) {
    ReconApp* app = context;
    if(app->esp) {
        esp_link_stop(app->esp);
        esp_link_free(app->esp);
        app->esp = NULL;
    }
    app->settings.backend = app->saved_backend;
}
