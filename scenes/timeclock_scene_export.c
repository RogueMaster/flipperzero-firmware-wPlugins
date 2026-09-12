// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#include "../timeclock.h"

// Export menu: the CSV is the live punches.csv; JSON is generated on demand;
// clearing the history asks for confirmation first.

typedef enum {
    ExportCsv,
    ExportJson,
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
    submenu_set_header(submenu, "Export");
    submenu_add_item(
        submenu, "Export CSV", ExportCsv, timeclock_scene_export_submenu_callback, app);
    submenu_add_item(
        submenu, "Export JSON", ExportJson, timeclock_scene_export_submenu_callback, app);
    submenu_add_item(
        submenu, "Clear history", ExportClear, timeclock_scene_export_submenu_callback, app);
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
        case ExportCsv:
            snprintf(
                export_msg, sizeof(export_msg), "CSV file:\napps_data/timeclock/punches.csv");
            timeclock_scene_export_show_popup(app, "Export CSV");
            break;
        case ExportJson:
            if(tc_history_export_json()) {
                snprintf(
                    export_msg,
                    sizeof(export_msg),
                    "Saved:\napps_data/timeclock/export.json");
                timeclock_scene_export_show_popup(app, "Export JSON");
            } else {
                snprintf(export_msg, sizeof(export_msg), "Nothing to export");
                timeclock_scene_export_show_popup(app, "Export JSON");
            }
            break;
        case ExportClear: {
            // Ask for confirmation with a Yes/No widget.
            Widget* widget = app->widget;
            widget_reset(widget);
            widget_add_string_multiline_element(
                widget,
                64,
                14,
                AlignCenter,
                AlignTop,
                FontSecondary,
                "Clear all history?\nThis cannot be undone.");
            widget_add_button_element(
                widget, GuiButtonTypeLeft, "No", timeclock_scene_export_button_callback, app);
            widget_add_button_element(
                widget, GuiButtonTypeRight, "Yes", timeclock_scene_export_button_callback, app);
            view_dispatcher_switch_to_view(app->view_dispatcher, TimeClockViewWidget);
            break;
        }
        case CLEAR_YES:
            tc_history_clear();
            snprintf(export_msg, sizeof(export_msg), "History cleared");
            timeclock_scene_export_show_popup(app, "Done");
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
