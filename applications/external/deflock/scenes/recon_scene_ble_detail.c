// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
#include "../recon_app_i.h"
#include "../helpers/esp_link.h"
#include "../helpers/tracker_rules.h"

#include <math.h>
#include "../helpers/fast_trig.h"

typedef enum {
    BleDetailToggleTag = 410,
    BleDetailPing,
    BleDetailRing,
} BleDetailEvent;

static const char* ble_action_label(BleActionKind kind) {
    return kind == BleActionRing ? "RING" : "PING";
}

static void recon_scene_ble_detail_render(ReconApp* app);

static const char* ble_cat_label(uint8_t cat) {
    switch(cat) {
    case BleCatFlock:
        return "Flock/Raven (BLE)"; // refined to the decoded model below when known
    case BleCatAirTag:
        return "Apple Find My/AirTag";
    case BleCatTile:
        return "Tile tracker";
    case BleCatSmartTag:
        return "Samsung SmartTag";
    case BleCatFindMyDevice:
        return "Find My Device (FMDN)";
    case BleCatFlipper:
        return "Flipper Zero";
    default:
        return "BLE device";
    }
}

static void recon_scene_ble_detail_button_cb(GuiButtonType type, InputType input, void* context) {
    ReconApp* app = context;
    if(input != InputTypeShort) return;
    if(type == GuiButtonTypeCenter) {
        view_dispatcher_send_custom_event(app->view_dispatcher, BleDetailToggleTag);
    } else if(type == GuiButtonTypeLeft) {
        view_dispatcher_send_custom_event(app->view_dispatcher, BleDetailPing);
    } else if(type == GuiButtonTypeRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, BleDetailRing);
    }
}

static void recon_scene_ble_detail_action(ReconApp* app, BleActionKind kind) {
    uint8_t addr[6] = {0};
    uint8_t cat = BleCatUnknown;
    bool valid = false;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    if(app->ble_selected >= 0 && app->ble_selected < (int)app->ble_count) {
        const BleDevice* d = &app->ble[app->ble_selected];
        memcpy(addr, d->addr, sizeof(addr));
        cat = d->cat;
        valid = true;
    }
    furi_mutex_release(app->mutex);

    if(!valid) return;
    if(!ble_tracker_category_is_known(cat)) {
        recon_app_set_ble_action(app, kind, "not_tracker", false, 0);
        recon_scene_ble_detail_render(app);
        return;
    }
    // Ring is intentionally limited to the Apple/Find My non-owner path. Tile,
    // SmartTag, and FMDN each have different control protocols and must not be
    // guessed at or sent an arbitrary write.
    if(kind == BleActionRing && cat != BleCatAirTag) {
        recon_app_set_ble_action(app, kind, "unsupported", false, 0);
        recon_scene_ble_detail_render(app);
        return;
    }
    if(app->settings.backend != EspBackendCompanion || !app->esp) {
        recon_app_set_ble_action(app, kind, "companion_only", false, 0);
        recon_scene_ble_detail_render(app);
        return;
    }

    recon_app_ble_action_begin(app, kind);
    char cmd[40];
    snprintf(
        cmd,
        sizeof(cmd),
        "ble_%s %02x%02x%02x%02x%02x%02x",
        kind == BleActionRing ? "ring" : "ping",
        addr[0],
        addr[1],
        addr[2],
        addr[3],
        addr[4],
        addr[5]);
    esp_link_send(app->esp, cmd);
    recon_scene_ble_detail_render(app);
}

static void recon_scene_ble_detail_render(ReconApp* app) {
    Widget* widget = app->widget;
    widget_reset(widget);

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    if(app->ble_selected < 0 || app->ble_selected >= (int)app->ble_count) {
        furi_mutex_release(app->mutex);
        widget_add_string_element(
            widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "No selection");
        return;
    }
    BleDevice d = app->ble[app->ble_selected];
    BleActionKind action_kind = (BleActionKind)app->ble_action_kind;
    bool action_pending = app->ble_action_pending;
    bool action_done = app->ble_action_done;
    bool action_have_rssi = app->ble_action_have_rssi;
    int8_t action_rssi = app->ble_action_rssi;
    uint32_t action_seq = app->ble_action_seq;
    char action_status[RECON_BLE_ACTION_STATUS_LEN];
    strncpy(action_status, app->ble_action_status, sizeof(action_status) - 1);
    action_status[sizeof(action_status) - 1] = '\0';
    furi_mutex_release(app->mutex);

    float moved = 0.0f;
    bool has_move = !isnan(d.first_lat) && !isnan(d.last_lat);
    if(has_move) {
        float dlat = (d.last_lat - d.first_lat) * 111320.0f;
        float dlon =
            (d.last_lon - d.first_lon) * 111320.0f * trig_cosf(d.first_lat * (float)M_PI / 180.0f);
        moved = sqrtf(dlat * dlat + dlon * dlon);
    }

    FuriString* s = furi_string_alloc();
    furi_string_printf(
        s,
        "%s%s\n%s\n"
        "%02X:%02X:%02X:%02X:%02X:%02X\n"
        "RSSI %d  seen %lu  co 0x%04X\n",
        ble_cat_label(d.cat),
        d.marked ? " *TAG" : "",
        d.name[0] ? d.name : "(no name)",
        d.addr[0],
        d.addr[1],
        d.addr[2],
        d.addr[3],
        d.addr[4],
        d.addr[5],
        d.rssi,
        (unsigned long)d.count,
        (unsigned)d.company);
    if(d.cat == BleCatFlock) {
        // Model line. A Raven is now positively identified via its 0x3100-0x3500
        // GATT services (GATT-backed -> confident, no "?"); otherwise this stays
        // generic. Falcon is never asserted (no Falcon-specific tell).
        furi_string_cat_printf(s, "%s\n", flock_ble_model_str((FlockBleModel)d.model));
        // Serial is always shown on-screen (saved-report logging is gated by the
        // "Log Flock serials" privacy toggle, not this view).
        if(d.serial[0]) furi_string_cat_printf(s, "SN %s\n", d.serial);
    }
    if(d.following) {
        furi_string_cat_printf(
            s,
            "! FOLLOWING you: %dm track\nover %d waypoints, %lus\n",
            (int)d.max_span_m,
            (int)d.inrange_wp_count,
            (unsigned long)((d.last_tick - d.first_tick) / 1000));
    } else if(has_move) {
        furi_string_cat_printf(s, "moved %dm vs first seen\n", (int)moved);
    }
    if(d.cat == BleCatAirTag || d.cat == BleCatTile || d.cat == BleCatSmartTag) {
        furi_string_cat(s, "Tracker - confirm it's yours");
    }
    if(action_kind != BleActionNone) {
        if(action_pending) {
            furi_string_cat_printf(s, "\n%s sending...", ble_action_label(action_kind));
        } else if(action_done) {
            furi_string_cat_printf(s, "\n%s %s", ble_action_label(action_kind), action_status);
            if(action_have_rssi) furi_string_cat_printf(s, " %ddBm", (int)action_rssi);
        }
    }

    widget_add_text_scroll_element(widget, 0, 0, 128, 52, furi_string_get_cstr(s));
    if(ble_tracker_category_is_known(d.cat)) {
        widget_add_button_element(
            widget, GuiButtonTypeLeft, "Ping", recon_scene_ble_detail_button_cb, app);
    }
    widget_add_button_element(
        widget,
        GuiButtonTypeCenter,
        d.marked ? "Untag" : "Tag",
        recon_scene_ble_detail_button_cb,
        app);
    if(d.cat == BleCatAirTag) {
        widget_add_button_element(
            widget, GuiButtonTypeRight, "Ring", recon_scene_ble_detail_button_cb, app);
    }
    furi_string_free(s);
    app->ble_action_render_seq = action_seq;
}

void recon_scene_ble_detail_on_enter(void* context) {
    ReconApp* app = context;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->ble_action_kind = BleActionNone;
    app->ble_action_pending = false;
    app->ble_action_done = false;
    app->ble_action_have_rssi = false;
    app->ble_action_status[0] = '\0';
    app->ble_action_seq++;
    if(app->ble_action_seq == 0) app->ble_action_seq = 1;
    furi_mutex_release(app->mutex);
    recon_scene_ble_detail_render(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewWidget);
}

bool recon_scene_ble_detail_on_event(void* context, SceneManagerEvent event) {
    ReconApp* app = context;
    if(event.type == SceneManagerEventTypeTick) {
        bool pending;
        uint8_t kind;
        uint32_t tick;
        uint32_t seq;
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        pending = app->ble_action_pending;
        kind = app->ble_action_kind;
        tick = app->ble_action_tick;
        seq = app->ble_action_seq;
        furi_mutex_release(app->mutex);
        uint32_t now = furi_get_tick();
        if(pending && now - tick > 8000) {
            recon_app_set_ble_action(app, (BleActionKind)kind, "timeout", false, 0);
            seq = app->ble_action_seq;
        }
        if(seq != app->ble_action_render_seq) recon_scene_ble_detail_render(app);
        return true;
    }
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == BleDetailPing) {
            recon_scene_ble_detail_action(app, BleActionPing);
            return true;
        }
        if(event.event == BleDetailRing) {
            recon_scene_ble_detail_action(app, BleActionRing);
            return true;
        }
        if(event.event == BleDetailToggleTag) {
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            if(app->ble_selected >= 0 && app->ble_selected < (int)app->ble_count) {
                app->ble[app->ble_selected].marked = !app->ble[app->ble_selected].marked;
            }
            furi_mutex_release(app->mutex);
            recon_scene_ble_detail_render(app);
            return true;
        }
    }
    return false;
}

void recon_scene_ble_detail_on_exit(void* context) {
    ReconApp* app = context;
    widget_reset(app->widget);
}
