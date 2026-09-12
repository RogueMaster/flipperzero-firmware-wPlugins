// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#include "../timeclock.h"

// Actions for a selected badge.

typedef enum {
    DetailRename,
    DetailReplace,
    DetailHistory,
    DetailDelete,
} DetailIndex;

static void timeclock_scene_badge_detail_submenu_callback(void* context, uint32_t index) {
    TimeClock* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void timeclock_scene_badge_detail_on_enter(void* context) {
    TimeClock* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    if(app->selected_index >= 0) {
        submenu_set_header(submenu, app->badges[app->selected_index].name);
    } else {
        submenu_set_header(submenu, "Badge");
    }
    submenu_add_item(
        submenu, "Rename", DetailRename, timeclock_scene_badge_detail_submenu_callback, app);
    submenu_add_item(
        submenu, "Replace chip", DetailReplace, timeclock_scene_badge_detail_submenu_callback, app);
    submenu_add_item(
        submenu, "View history", DetailHistory, timeclock_scene_badge_detail_submenu_callback, app);
    submenu_add_item(
        submenu, "Delete badge", DetailDelete, timeclock_scene_badge_detail_submenu_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, TimeClockViewSubmenu);
}

bool timeclock_scene_badge_detail_on_event(void* context, SceneManagerEvent event) {
    TimeClock* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        switch(event.event) {
        case DetailRename:
            // 1 = rename in the name-input scene.
            scene_manager_set_scene_state(app->scene_manager, TimeClockSceneNameInput, 1);
            scene_manager_next_scene(app->scene_manager, TimeClockSceneNameInput);
            break;
        case DetailReplace:
            // Bind a new chip to this collaborator (keeps name and history).
            app->replace_index = app->selected_index;
            app->scan_purpose = TcScanReplace;
            scene_manager_next_scene(app->scene_manager, TimeClockSceneScan);
            break;
        case DetailHistory:
            // 1 = filter the history by the selected badge.
            scene_manager_set_scene_state(app->scene_manager, TimeClockSceneHistory, 1);
            scene_manager_next_scene(app->scene_manager, TimeClockSceneHistory);
            break;
        case DetailDelete:
            scene_manager_next_scene(app->scene_manager, TimeClockSceneConfirmDelete);
            break;
        default:
            consumed = false;
            break;
        }
    }

    return consumed;
}

void timeclock_scene_badge_detail_on_exit(void* context) {
    UNUSED(context);
}
