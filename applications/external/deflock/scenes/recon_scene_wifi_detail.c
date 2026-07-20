// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "../recon_app_i.h"
#include "../helpers/wifi_audit.h"
#include "../helpers/oui_vendor.h"

typedef enum {
    WifiDetailToggleTag = 400,
} WifiDetailEvent;

static void recon_scene_wifi_detail_button_cb(GuiButtonType type, InputType input, void* context) {
    ReconApp* app = context;
    if(input == InputTypeShort && type == GuiButtonTypeCenter) {
        view_dispatcher_send_custom_event(app->view_dispatcher, WifiDetailToggleTag);
    }
}

static void recon_scene_wifi_detail_render(ReconApp* app) {
    Widget* widget = app->widget;
    widget_reset(widget);

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    if(app->wifi_selected < 0 || app->wifi_selected >= (int)app->wifi_count) {
        furi_mutex_release(app->mutex);
        widget_add_string_element(
            widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "No selection");
        return;
    }
    WifiAp ap = app->wifi[app->wifi_selected];
    furi_mutex_release(app->mutex);

    FuriString* reasons = furi_string_alloc();
    WifiGrade grade = wifi_audit_grade(ap.authmode, ap.pairwise, ap.wps, ap.ssid, reasons);
    const char* vendor = oui_vendor(ap.bssid);

    FuriString* s = furi_string_alloc();
    furi_string_printf(
        s,
        "%s  [%s]%s\n"
        "%02X:%02X:%02X:%02X:%02X:%02X %s\n"
        "Auth %s  Ch %u  %ddB%s\n"
        "--- issues ---\n%s",
        ap.ssid[0] ? ap.ssid : "(hidden)",
        wifi_grade_str(grade),
        ap.marked ? " *TAG" : "",
        ap.bssid[0],
        ap.bssid[1],
        ap.bssid[2],
        ap.bssid[3],
        ap.bssid[4],
        ap.bssid[5],
        vendor ? vendor : "",
        wifi_auth_str(ap.authmode),
        ap.channel,
        ap.rssi,
        ap.wps ? " WPS" : "",
        furi_string_get_cstr(reasons));

    if(ap.rogue) {
        furi_string_cat(s, "!! EVIL TWIN: same SSID\nwith MISMATCHED security\n");
    } else if(ap.dup) {
        furi_string_cat(s, "~ Dup SSID on >1 BSSID\n(or mesh/extender)\n");
    }

    widget_add_text_scroll_element(widget, 0, 0, 128, 52, furi_string_get_cstr(s));
    widget_add_button_element(
        widget,
        GuiButtonTypeCenter,
        ap.marked ? "Untag" : "Tag",
        recon_scene_wifi_detail_button_cb,
        app);
    furi_string_free(s);
    furi_string_free(reasons);
}

void recon_scene_wifi_detail_on_enter(void* context) {
    ReconApp* app = context;
    recon_scene_wifi_detail_render(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewWidget);
}

bool recon_scene_wifi_detail_on_event(void* context, SceneManagerEvent event) {
    ReconApp* app = context;
    if(event.type == SceneManagerEventTypeCustom && event.event == WifiDetailToggleTag) {
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        if(app->wifi_selected >= 0 && app->wifi_selected < (int)app->wifi_count) {
            app->wifi[app->wifi_selected].marked = !app->wifi[app->wifi_selected].marked;
        }
        furi_mutex_release(app->mutex);
        recon_scene_wifi_detail_render(app);
        return true;
    }
    return false;
}

void recon_scene_wifi_detail_on_exit(void* context) {
    ReconApp* app = context;
    widget_reset(app->widget);
}
