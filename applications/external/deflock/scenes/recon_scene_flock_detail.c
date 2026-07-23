// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "../recon_app_i.h"

#include <math.h>

typedef enum {
    DetailCustomToggleMark = 200,
} DetailCustomEvent;

static void
    recon_scene_flock_detail_button_cb(GuiButtonType type, InputType input, void* context) {
    ReconApp* app = context;
    if(input == InputTypeShort && type == GuiButtonTypeCenter) {
        view_dispatcher_send_custom_event(app->view_dispatcher, DetailCustomToggleMark);
    }
}

static void recon_scene_flock_detail_render(ReconApp* app) {
    Widget* widget = app->widget;
    widget_reset(widget);

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    if(app->selected < 0 || app->selected >= (int)app->flock_count) {
        furi_mutex_release(app->mutex);
        widget_add_string_element(
            widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "No selection");
        return;
    }
    FlockEntry e = app->flock[app->selected];
    furi_mutex_release(app->mutex);

    const char* src;
    switch(e.ftype) {
    case 'L':
        src = "BLE";
        break;
    case 'P':
        src = "probe";
        break;
    case 'F':
        src = "probe-fp"; // B1 IE-fingerprint device-class match
        break;
    case 'B':
        src = "beacon";
        break;
    case 'R':
        src = "p-resp";
        break;
    default:
        src = "RF";
        break;
    }

    FuriString* s = furi_string_alloc();
    furi_string_printf(
        s,
        "%s  %s\n"
        "%02X:%02X:%02X:%02X:%02X:%02X\n"
        "SSID: %s\n"
        "RSSI %d  Ch %u  Seen %lu  via %s",
        flock_confidence_str(e.confidence),
        e.marked ? "(MARKED)" : "",
        e.mac[0],
        e.mac[1],
        e.mac[2],
        e.mac[3],
        e.mac[4],
        e.mac[5],
        e.ssid[0] ? e.ssid : "(hidden)",
        e.rssi,
        e.channel,
        (unsigned long)e.count,
        src);

    if(!isnan(e.lat) && !isnan(e.lon)) {
        furi_string_cat_printf(s, "\nGPS %.5f, %.5f", (double)e.lat, (double)e.lon);
    } else {
        furi_string_cat(s, "\nGPS: no fix");
    }

    // Show the probe IE-fingerprint when present: a confirmed unit's fp can be
    // dropped into signatures.json ("ie_fps") to catch its MAC-randomized twins.
    if(e.ie_fp != 0) {
        furi_string_cat_printf(s, "\nIE-fp: %08lx", (unsigned long)e.ie_fp);
    }

    widget_add_text_scroll_element(widget, 0, 0, 128, 44, furi_string_get_cstr(s));
    furi_string_free(s);

    widget_add_button_element(
        widget,
        GuiButtonTypeCenter,
        e.marked ? "Unmark" : "Mark",
        recon_scene_flock_detail_button_cb,
        app);
}

void recon_scene_flock_detail_on_enter(void* context) {
    ReconApp* app = context;
    recon_scene_flock_detail_render(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewWidget);
}

bool recon_scene_flock_detail_on_event(void* context, SceneManagerEvent event) {
    ReconApp* app = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom && event.event == DetailCustomToggleMark) {
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        if(app->selected >= 0 && app->selected < (int)app->flock_count) {
            app->flock[app->selected].marked = !app->flock[app->selected].marked;
        }
        furi_mutex_release(app->mutex);
        recon_scene_flock_detail_render(app);
        consumed = true;
    }
    return consumed;
}

void recon_scene_flock_detail_on_exit(void* context) {
    ReconApp* app = context;
    widget_reset(app->widget);
}
