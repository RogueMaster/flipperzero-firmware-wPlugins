// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#include "../timeclock.h"

// List of registered badges plus a "New badge" entry (which starts a scan).

static void timeclock_scene_badge_list_submenu_callback(void* context, uint32_t index) {
    TimeClock* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void timeclock_scene_badge_list_on_enter(void* context) {
    TimeClock* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "Badges");

    for(size_t i = 0; i < app->badge_count; i++) {
        submenu_add_item(
            submenu,
            app->badges[i].name,
            (uint32_t)i,
            timeclock_scene_badge_list_submenu_callback,
            app);
    }
    // "New badge" uses the index just past the last badge.
    submenu_add_item(
        submenu,
        "+ New badge",
        (uint32_t)app->badge_count,
        timeclock_scene_badge_list_submenu_callback,
        app);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, TimeClockSceneBadgeList));

    view_dispatcher_switch_to_view(app->view_dispatcher, TimeClockViewSubmenu);
}

bool timeclock_scene_badge_list_on_event(void* context, SceneManagerEvent event) {
    TimeClock* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, TimeClockSceneBadgeList, event.event);
        consumed = true;
        if(event.event < app->badge_count) {
            app->selected_index = (int)event.event;
            scene_manager_next_scene(app->scene_manager, TimeClockSceneBadgeDetail);
        } else {
            // Register a new badge via a scan.
            scene_manager_next_scene(app->scene_manager, TimeClockSceneScan);
        }
    }

    return consumed;
}

void timeclock_scene_badge_list_on_exit(void* context) {
    UNUSED(context);
}
