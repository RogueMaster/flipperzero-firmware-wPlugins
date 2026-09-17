// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#include "../timeclock.h"

// Startup warning shown (on top of PinUnlock/Menu) when the Flipper's RTC
// date is older than this release, which means the clock was never set (or
// got reset after the battery died). Every date/time this app writes depends
// on the RTC, so this is worth an explicit, must-dismiss screen rather than
// a line buried in a text view. Not security: Back also dismisses it.

#define DATE_WARNING_OK 295u

static void timeclock_scene_date_warning_button_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    TimeClock* app = context;
    if(type != InputTypeShort) return;
    if(result == GuiButtonTypeRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, DATE_WARNING_OK);
    }
}

void timeclock_scene_date_warning_on_enter(void* context) {
    TimeClock* app = context;

    Widget* widget = app->widget;
    widget_reset(widget);
    widget_add_string_element(
        widget, 64, 6, AlignCenter, AlignTop, FontPrimary, tc_str(StrClockNotSet));
    widget_add_string_multiline_element(
        widget, 64, 20, AlignCenter, AlignTop, FontSecondary, tc_str(StrDateWrongText));
    widget_add_button_element(
        widget,
        GuiButtonTypeRight,
        tc_str(StrDone),
        timeclock_scene_date_warning_button_callback,
        app);
    view_dispatcher_switch_to_view(app->view_dispatcher, TimeClockViewWidget);
}

bool timeclock_scene_date_warning_on_event(void* context, SceneManagerEvent event) {
    TimeClock* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom && event.event == DATE_WARNING_OK) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void timeclock_scene_date_warning_on_exit(void* context) {
    TimeClock* app = context;
    widget_reset(app->widget);
}
