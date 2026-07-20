// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "../recon_app_i.h"

#include <math.h>

typedef enum {
    BleDetailToggleTag = 410,
} BleDetailEvent;

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
    if(input == InputTypeShort && type == GuiButtonTypeCenter) {
        view_dispatcher_send_custom_event(app->view_dispatcher, BleDetailToggleTag);
    }
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
    furi_mutex_release(app->mutex);

    float moved = 0.0f;
    bool has_move = !isnan(d.first_lat) && !isnan(d.last_lat);
    if(has_move) {
        float dlat = (d.last_lat - d.first_lat) * 111320.0f;
        float dlon =
            (d.last_lon - d.first_lon) * 111320.0f * cosf(d.first_lat * (float)M_PI / 180.0f);
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

    widget_add_text_scroll_element(widget, 0, 0, 128, 52, furi_string_get_cstr(s));
    widget_add_button_element(
        widget,
        GuiButtonTypeCenter,
        d.marked ? "Untag" : "Tag",
        recon_scene_ble_detail_button_cb,
        app);
    furi_string_free(s);
}

void recon_scene_ble_detail_on_enter(void* context) {
    ReconApp* app = context;
    recon_scene_ble_detail_render(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewWidget);
}

bool recon_scene_ble_detail_on_event(void* context, SceneManagerEvent event) {
    ReconApp* app = context;
    if(event.type == SceneManagerEventTypeCustom && event.event == BleDetailToggleTag) {
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        if(app->ble_selected >= 0 && app->ble_selected < (int)app->ble_count) {
            app->ble[app->ble_selected].marked = !app->ble[app->ble_selected].marked;
        }
        furi_mutex_release(app->mutex);
        recon_scene_ble_detail_render(app);
        return true;
    }
    return false;
}

void recon_scene_ble_detail_on_exit(void* context) {
    ReconApp* app = context;
    widget_reset(app->widget);
}
