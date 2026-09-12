// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#include "../timeclock.h"

// Restore: list the timestamped backups and, after a confirmation, replace the
// current badges + punches with a chosen backup.

#define RESTORE_MAX 32
#define RESTORE_CONFIRM_YES 280u
#define RESTORE_CONFIRM_NO 281u
#define RESTORE_DONE 282u

static char stamps[RESTORE_MAX][24];
static size_t stamp_count;
static char chosen[24];

static void timeclock_scene_restore_submenu_callback(void* context, uint32_t index) {
    TimeClock* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void timeclock_scene_restore_button_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    TimeClock* app = context;
    if(type != InputTypeShort) return;
    if(result == GuiButtonTypeLeft) {
        view_dispatcher_send_custom_event(app->view_dispatcher, RESTORE_CONFIRM_NO);
    } else if(result == GuiButtonTypeRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, RESTORE_CONFIRM_YES);
    }
}

static void timeclock_scene_restore_popup_callback(void* context) {
    TimeClock* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, RESTORE_DONE);
}

static void timeclock_scene_restore_show_list(TimeClock* app) {
    stamp_count = tc_backup_list(stamps, RESTORE_MAX);

    if(stamp_count == 0) {
        Popup* popup = app->popup;
        popup_reset(popup);
        popup_set_header(popup, tc_str(StrRestore), 64, 8, AlignCenter, AlignTop);
        popup_set_text(popup, tc_str(StrNoBackups), 64, 32, AlignCenter, AlignTop);
        popup_set_callback(popup, timeclock_scene_restore_popup_callback);
        popup_set_context(popup, app);
        popup_set_timeout(popup, 1800);
        popup_enable_timeout(popup);
        view_dispatcher_switch_to_view(app->view_dispatcher, TimeClockViewPopup);
        return;
    }

    Submenu* submenu = app->submenu;
    submenu_reset(submenu);
    submenu_set_header(submenu, tc_str(StrRestore));
    for(size_t i = 0; i < stamp_count; i++) {
        submenu_add_item(
            submenu, stamps[i], (uint32_t)i, timeclock_scene_restore_submenu_callback, app);
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, TimeClockViewSubmenu);
}

void timeclock_scene_restore_on_enter(void* context) {
    TimeClock* app = context;
    timeclock_scene_restore_show_list(app);
}

bool timeclock_scene_restore_on_event(void* context, SceneManagerEvent event) {
    TimeClock* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        if(event.event == RESTORE_DONE) {
            scene_manager_previous_scene(app->scene_manager);
        } else if(event.event == RESTORE_CONFIRM_NO) {
            timeclock_scene_restore_show_list(app);
        } else if(event.event == RESTORE_CONFIRM_YES) {
            bool ok = tc_backup_restore(chosen);
            timeclock_reload_badges(app);
            if(ok) {
                timeclock_notify_success(app);
            } else {
                timeclock_notify_error(app);
            }
            Popup* popup = app->popup;
            popup_reset(popup);
            popup_set_header(
                popup,
                ok ? tc_str(StrRestoreDone) : tc_str(StrNoBackups),
                64,
                8,
                AlignCenter,
                AlignTop);
            popup_set_text(popup, chosen, 64, 32, AlignCenter, AlignTop);
            popup_set_callback(popup, timeclock_scene_restore_popup_callback);
            popup_set_context(popup, app);
            popup_set_timeout(popup, 1800);
            popup_enable_timeout(popup);
            view_dispatcher_switch_to_view(app->view_dispatcher, TimeClockViewPopup);
        } else if(event.event < stamp_count) {
            // A backup was selected: confirm before overwriting.
            strncpy(chosen, stamps[event.event], sizeof(chosen) - 1);
            chosen[sizeof(chosen) - 1] = '\0';

            Widget* widget = app->widget;
            widget_reset(widget);
            widget_add_string_multiline_element(
                widget, 64, 12, AlignCenter, AlignTop, FontSecondary, tc_str(StrRestoreConfirm));
            widget_add_button_element(
                widget, GuiButtonTypeLeft, tc_str(StrNo), timeclock_scene_restore_button_callback, app);
            widget_add_button_element(
                widget,
                GuiButtonTypeRight,
                tc_str(StrYes),
                timeclock_scene_restore_button_callback,
                app);
            view_dispatcher_switch_to_view(app->view_dispatcher, TimeClockViewWidget);
        } else {
            consumed = false;
        }
    }

    return consumed;
}

void timeclock_scene_restore_on_exit(void* context) {
    TimeClock* app = context;
    popup_reset(app->popup);
    widget_reset(app->widget);
}
