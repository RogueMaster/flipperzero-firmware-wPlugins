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

    // An archived entry's RSSI/channel were recorded on an earlier run, so label
    // them as last-known rather than presenting a stored reading as a live one.
    const char* rssi_label = e.archived ? "Last RSSI" : "RSSI";

    FuriString* s = furi_string_alloc();
    furi_string_printf(
        s,
        "%s  %s\n"
        "%s\n"
        "%02X:%02X:%02X:%02X:%02X:%02X\n"
        "SSID: %s\n"
        "%s %d  Ch %u  Seen %lu  via %s",
        flock_confidence_str(e.confidence),
        e.marked ? "(MARKED)" : "",
        // What it is, spelled out: the confidence rung above says how sure we
        // are, not which kind of device this is.
        flock_class_long_str((FlockDevClass)e.dev_class),
        e.mac[0],
        e.mac[1],
        e.mac[2],
        e.mac[3],
        e.mac[4],
        e.mac[5],
        // "(hidden)" here has always meant "we have no name for it". Only say the
        // AP is actively withholding one when we watched it beacon without a name.
        e.ssid[0] ? e.ssid : (e.hidden ? "(withheld by AP)" : "(none seen)"),
        rssi_label,
        e.rssi,
        e.channel,
        (unsigned long)e.count,
        src);

    // Where a stored hit came from, in wall-clock terms. Only meaningful for an
    // archived entry: a live one's seen_epoch is "moments ago" by definition.
    if(e.archived && e.seen_epoch) {
        DateTime dt;
        datetime_timestamp_to_datetime(e.seen_epoch, &dt);
        furi_string_cat_printf(
            s, "\nSaved: %04u-%02u-%02u %02u:%02u", dt.year, dt.month, dt.day, dt.hour, dt.minute);
    }

    if(!isnan(e.lat) && !isnan(e.lon)) {
        furi_string_cat_printf(s, "\nGPS %.5f, %.5f", (double)e.lat, (double)e.lon);
    } else {
        furi_string_cat(s, "\nGPS: no fix");
    }

    // Hidden-SSID beaconing. An OBSERVATION, not a score: it did not raise the
    // confidence rung above, and the wording must not imply that it did. Flock
    // moved to hidden SSIDs, but so do plenty of ordinary home routers.
    if(e.hidden) {
        furi_string_cat(s, "\nHidden SSID: beacons, no name (not scored)");
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
