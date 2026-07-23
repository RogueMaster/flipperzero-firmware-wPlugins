// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "../recon_app_i.h"
#include "../helpers/recon_report.h"

typedef enum {
    ReportItemSave,
    ReportItemClear,
} ReportItem;

static void recon_scene_reports_submenu_cb(void* context, uint32_t index) {
    ReconApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void recon_scene_reports_popup_cb(void* context) {
    ReconApp* app = context;
    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewSubmenu);
}

static void recon_scene_reports_build_menu(ReconApp* app) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    int marked = 0;
    for(size_t i = 0; i < app->flock_count; i++) {
        if(app->flock[i].marked) marked++;
    }
    furi_mutex_release(app->mutex);

    Submenu* submenu = app->submenu;
    submenu_reset(submenu);
    snprintf(app->text_store, RECON_TEXT_STORE, "Reports (%d marked)", marked);
    submenu_set_header(submenu, app->text_store);
    submenu_add_item(
        submenu, "Save Marked -> Report", ReportItemSave, recon_scene_reports_submenu_cb, app);
    submenu_add_item(
        submenu, "Clear All Marks", ReportItemClear, recon_scene_reports_submenu_cb, app);
}

void recon_scene_reports_on_enter(void* context) {
    ReconApp* app = context;
    recon_scene_reports_build_menu(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewSubmenu);
}

static void recon_scene_reports_show_popup(ReconApp* app, const char* header, const char* text) {
    Popup* popup = app->popup;
    popup_reset(popup);
    popup_set_header(popup, header, 64, 10, AlignCenter, AlignTop);
    popup_set_text(popup, text, 64, 30, AlignCenter, AlignTop);
    popup_set_context(popup, app);
    popup_set_callback(popup, recon_scene_reports_popup_cb);
    popup_set_timeout(popup, 2500);
    popup_enable_timeout(popup);
    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewPopup);
}

bool recon_scene_reports_on_event(void* context, SceneManagerEvent event) {
    ReconApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == ReportItemSave) {
            char path[128] = {0};
            bool ok = recon_report_save_flock(app, path, sizeof(path));
            if(app->settings.sound) {
                notification_message(app->notifications, ok ? &sequence_success : &sequence_error);
            }
            recon_scene_reports_show_popup(
                app,
                ok ? "Report Saved" : "Nothing to Save",
                ok ? "See apps_data/\nflipdeflock/reports" : "Mark detections first");
            consumed = true;
        } else if(event.event == ReportItemClear) {
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            for(size_t i = 0; i < app->flock_count; i++) {
                app->flock[i].marked = false;
            }
            furi_mutex_release(app->mutex);
            recon_scene_reports_build_menu(app);
            recon_scene_reports_show_popup(app, "Marks Cleared", "");
            consumed = true;
        }
    }
    return consumed;
}

void recon_scene_reports_on_exit(void* context) {
    ReconApp* app = context;
    popup_reset(app->popup);
    submenu_reset(app->submenu);
}
