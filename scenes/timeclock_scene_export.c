// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#include "../timeclock.h"

// Export menu: the CSV is the live punches.csv; JSON is generated on demand;
// clearing the history asks for confirmation first.

typedef enum {
    ExportCsv,
    ExportJson,
    ExportBackup,
    ExportClear,
} ExportIndex;

#define CLEAR_YES 230
#define CLEAR_NO 231
#define POPUP_DONE 232

static char export_msg[128];

static void timeclock_scene_export_submenu_callback(void* context, uint32_t index) {
    TimeClock* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void timeclock_scene_export_popup_callback(void* context) {
    TimeClock* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, POPUP_DONE);
}

static void timeclock_scene_export_button_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    TimeClock* app = context;
    if(type != InputTypeShort) return;
    if(result == GuiButtonTypeLeft) {
        view_dispatcher_send_custom_event(app->view_dispatcher, CLEAR_NO);
    } else if(result == GuiButtonTypeRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, CLEAR_YES);
    }
}

static void timeclock_scene_export_show_menu(TimeClock* app) {
    Submenu* submenu = app->submenu;
    submenu_reset(submenu);
    submenu_set_header(submenu, tc_str(StrExport));
    submenu_add_item(
        submenu, tc_str(StrExportCsv), ExportCsv, timeclock_scene_export_submenu_callback, app);
    submenu_add_item(
        submenu, tc_str(StrExportJson), ExportJson, timeclock_scene_export_submenu_callback, app);
    submenu_add_item(
        submenu, tc_str(StrBackup), ExportBackup, timeclock_scene_export_submenu_callback, app);
    submenu_add_item(
        submenu, tc_str(StrClearHistory), ExportClear, timeclock_scene_export_submenu_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, TimeClockViewSubmenu);
}

static void timeclock_scene_export_show_popup(TimeClock* app, const char* header) {
    Popup* popup = app->popup;
    popup_reset(popup);
    popup_set_header(popup, header, 64, 8, AlignCenter, AlignTop);
    popup_set_text(popup, export_msg, 64, 30, AlignCenter, AlignTop);
    popup_set_callback(popup, timeclock_scene_export_popup_callback);
    popup_set_context(popup, app);
    popup_set_timeout(popup, 2500);
    popup_enable_timeout(popup);
    view_dispatcher_switch_to_view(app->view_dispatcher, TimeClockViewPopup);
}

void timeclock_scene_export_on_enter(void* context) {
    TimeClock* app = context;
    timeclock_scene_export_show_menu(app);
}

bool timeclock_scene_export_on_event(void* context, SceneManagerEvent event) {
    TimeClock* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        switch(event.event) {
        case ExportCsv: {
            char name[40];
            if(tc_export_csv_dated(name, sizeof(name))) {
                snprintf(export_msg, sizeof(export_msg), "%s\n(apps_data/timeclock)", name);
            } else {
                snprintf(export_msg, sizeof(export_msg), "%s", tc_str(StrNothingExport));
            }
            timeclock_scene_export_show_popup(app, tc_str(StrExportCsv));
            break;
        }
        case ExportBackup: {
            char loc[40];
            if(tc_backup_all(loc, sizeof(loc))) {
                snprintf(export_msg, sizeof(export_msg), "%s\n%s", tc_str(StrBackupDone), loc);
            } else {
                snprintf(export_msg, sizeof(export_msg), "%s", tc_str(StrNothingExport));
            }
            timeclock_scene_export_show_popup(app, tc_str(StrBackup));
            break;
        }
        case ExportJson:
            if(tc_history_export_json()) {
                snprintf(
                    export_msg,
                    sizeof(export_msg),
                    "Saved:\napps_data/timeclock/export.json");
                timeclock_scene_export_show_popup(app, tc_str(StrExportJson));
            } else {
                snprintf(export_msg, sizeof(export_msg), "%s", tc_str(StrNothingExport));
                timeclock_scene_export_show_popup(app, tc_str(StrExportJson));
            }
            break;
        case ExportClear: {
            // Ask for confirmation with a Yes/No widget.
            Widget* widget = app->widget;
            widget_reset(widget);
            widget_add_string_multiline_element(
                widget, 64, 14, AlignCenter, AlignTop, FontSecondary, tc_str(StrClearConfirm));
            widget_add_button_element(
                widget,
                GuiButtonTypeLeft,
                tc_str(StrNo),
                timeclock_scene_export_button_callback,
                app);
            widget_add_button_element(
                widget,
                GuiButtonTypeRight,
                tc_str(StrYes),
                timeclock_scene_export_button_callback,
                app);
            view_dispatcher_switch_to_view(app->view_dispatcher, TimeClockViewWidget);
            break;
        }
        case CLEAR_YES:
            tc_history_clear();
            snprintf(export_msg, sizeof(export_msg), "%s", tc_str(StrHistoryCleared));
            timeclock_scene_export_show_popup(app, tc_str(StrDone));
            break;
        case CLEAR_NO:
            timeclock_scene_export_show_menu(app);
            break;
        case POPUP_DONE:
            timeclock_scene_export_show_menu(app);
            break;
        default:
            consumed = false;
            break;
        }
    }

    return consumed;
}

void timeclock_scene_export_on_exit(void* context) {
    TimeClock* app = context;
    popup_reset(app->popup);
    widget_reset(app->widget);
}
