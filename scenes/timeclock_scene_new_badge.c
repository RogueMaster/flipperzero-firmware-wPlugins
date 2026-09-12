// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#include "../timeclock.h"

// Unknown badge: offer to save it (-> name input) or cancel.

typedef enum {
    NewBadgeSave,
    NewBadgeCancel,
} NewBadgeIndex;

static void timeclock_scene_new_badge_submenu_callback(void* context, uint32_t index) {
    TimeClock* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void timeclock_scene_new_badge_on_enter(void* context) {
    TimeClock* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    // Show the detected UID in the header.
    submenu_set_header(submenu, app->scanned_uid);
    submenu_add_item(
        submenu, "Save badge", NewBadgeSave, timeclock_scene_new_badge_submenu_callback, app);
    submenu_add_item(
        submenu, "Cancel", NewBadgeCancel, timeclock_scene_new_badge_submenu_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, TimeClockViewSubmenu);
}

bool timeclock_scene_new_badge_on_event(void* context, SceneManagerEvent event) {
    TimeClock* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case NewBadgeSave:
            // 0 = create a new badge in the name-input scene.
            scene_manager_set_scene_state(app->scene_manager, TimeClockSceneNameInput, 0);
            scene_manager_next_scene(app->scene_manager, TimeClockSceneNameInput);
            consumed = true;
            break;
        case NewBadgeCancel:
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, TimeClockSceneMenu);
            consumed = true;
            break;
        default:
            break;
        }
    }

    return consumed;
}

void timeclock_scene_new_badge_on_exit(void* context) {
    UNUSED(context);
}
