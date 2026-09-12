// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#include "../timeclock.h"

// Confirm badge deletion. The history (punches.csv) is intentionally kept:
// past punches remain, referencing the now-unregistered UID.

#define CONFIRM_YES 220
#define CONFIRM_NO 221

static char detail_line[80];

static void timeclock_scene_confirm_delete_button_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    TimeClock* app = context;
    if(type != InputTypeShort) return;
    if(result == GuiButtonTypeLeft) {
        view_dispatcher_send_custom_event(app->view_dispatcher, CONFIRM_NO);
    } else if(result == GuiButtonTypeRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, CONFIRM_YES);
    }
}

void timeclock_scene_confirm_delete_on_enter(void* context) {
    TimeClock* app = context;
    Widget* widget = app->widget;

    widget_reset(widget);
    widget_add_string_element(
        widget, 64, 6, AlignCenter, AlignTop, FontPrimary, tc_str(StrDeleteQ));

    if(app->selected_index >= 0) {
        Badge* b = &app->badges[app->selected_index];
        snprintf(detail_line, sizeof(detail_line), "%s\nUID: %s", b->name, b->uid);
    } else {
        snprintf(detail_line, sizeof(detail_line), "?");
    }
    widget_add_string_multiline_element(
        widget, 64, 26, AlignCenter, AlignTop, FontSecondary, detail_line);

    widget_add_button_element(
        widget,
        GuiButtonTypeLeft,
        tc_str(StrNo),
        timeclock_scene_confirm_delete_button_callback,
        app);
    widget_add_button_element(
        widget,
        GuiButtonTypeRight,
        tc_str(StrYes),
        timeclock_scene_confirm_delete_button_callback,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, TimeClockViewWidget);
}

bool timeclock_scene_confirm_delete_on_event(void* context, SceneManagerEvent event) {
    TimeClock* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == CONFIRM_YES) {
            int idx = app->selected_index;
            if(idx >= 0 && (size_t)idx < app->badge_count) {
                // Remove the badge by shifting the tail down.
                for(size_t i = (size_t)idx; i + 1 < app->badge_count; i++) {
                    app->badges[i] = app->badges[i + 1];
                }
                app->badge_count--;
                tc_badges_save(app->badges, app->badge_count);
            }
            app->selected_index = -1;
            // Reset the list selection so it stays in range.
            scene_manager_set_scene_state(app->scene_manager, TimeClockSceneBadgeList, 0);
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, TimeClockSceneBadgeList);
            consumed = true;
        } else if(event.event == CONFIRM_NO) {
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
        }
    }

    return consumed;
}

void timeclock_scene_confirm_delete_on_exit(void* context) {
    TimeClock* app = context;
    widget_reset(app->widget);
}
