// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#include "../timeclock.h"

// Filter chooser shown before the history list. The item index is the History
// scene's filter code: 0 = all, 2 = today, 3 = this week (1 is reserved for the
// per-collaborator filter reached from Badges).

static void timeclock_scene_history_menu_submenu_callback(void* context, uint32_t index) {
    TimeClock* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void timeclock_scene_history_menu_on_enter(void* context) {
    TimeClock* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, tc_str(StrHistory));
    submenu_add_item(submenu, tc_str(StrAll), 0, timeclock_scene_history_menu_submenu_callback, app);
    submenu_add_item(submenu, tc_str(StrToday), 2, timeclock_scene_history_menu_submenu_callback, app);
    submenu_add_item(
        submenu, tc_str(StrThisWeek), 3, timeclock_scene_history_menu_submenu_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, TimeClockViewSubmenu);
}

bool timeclock_scene_history_menu_on_event(void* context, SceneManagerEvent event) {
    TimeClock* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, TimeClockSceneHistory, event.event);
        scene_manager_next_scene(app->scene_manager, TimeClockSceneHistory);
        consumed = true;
    }

    return consumed;
}

void timeclock_scene_history_menu_on_exit(void* context) {
    UNUSED(context);
}
