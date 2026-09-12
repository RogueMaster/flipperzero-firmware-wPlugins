// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#include "../timeclock.h"

// Known badge recognized: choose IN / OUT (or Cancel). In auto mode the
// suggested action (opposite of the last punch) is preselected, but the user
// can still correct it manually.

typedef enum {
    ActionIn,
    ActionOut,
    ActionCancel,
} ActionIndex;

#define SCENE_EVENT_DONE 200

static char confirm_msg[64];

static void timeclock_scene_badge_action_submenu_callback(void* context, uint32_t index) {
    TimeClock* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void timeclock_scene_badge_action_popup_callback(void* context) {
    TimeClock* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, SCENE_EVENT_DONE);
}

static void timeclock_scene_badge_action_record(TimeClock* app, TcEventType type) {
    Badge* b = &app->badges[app->found_index];

    char date[TC_DT_MAX];
    char time[8];
    char dt[TC_DT_MAX];
    tc_now_date(date, sizeof(date));
    tc_now_time(time, sizeof(time));
    tc_now_datetime(dt, sizeof(dt));

    tc_history_append(date, time, b->name, b->uid, type);

    b->last_event = type;
    strncpy(b->last_used, dt, sizeof(b->last_used) - 1);
    b->last_used[sizeof(b->last_used) - 1] = '\0';
    tc_badges_save(app->badges, app->badge_count);

    timeclock_notify_punch(app, type);

    snprintf(confirm_msg, sizeof(confirm_msg), "%s\n%s at %s", b->name, tc_event_str(type), time);

    Popup* popup = app->popup;
    popup_reset(popup);
    popup_set_header(popup, "Saved", 64, 8, AlignCenter, AlignTop);
    popup_set_text(popup, confirm_msg, 64, 30, AlignCenter, AlignTop);
    popup_set_callback(popup, timeclock_scene_badge_action_popup_callback);
    popup_set_context(popup, app);
    popup_set_timeout(popup, 1500);
    popup_enable_timeout(popup);
    view_dispatcher_switch_to_view(app->view_dispatcher, TimeClockViewPopup);
}

void timeclock_scene_badge_action_on_enter(void* context) {
    TimeClock* app = context;
    Submenu* submenu = app->submenu;

    Badge* b = &app->badges[app->found_index];

    submenu_reset(submenu);
    submenu_set_header(submenu, b->name);
    submenu_add_item(
        submenu, "IN", ActionIn, timeclock_scene_badge_action_submenu_callback, app);
    submenu_add_item(
        submenu, "OUT", ActionOut, timeclock_scene_badge_action_submenu_callback, app);
    submenu_add_item(
        submenu, "Cancel", ActionCancel, timeclock_scene_badge_action_submenu_callback, app);

    // Auto mode: preselect the opposite of the last punch.
    if(app->config.auto_mode) {
        submenu_set_selected_item(submenu, (b->last_event == TcEventIn) ? ActionOut : ActionIn);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, TimeClockViewSubmenu);
}

bool timeclock_scene_badge_action_on_event(void* context, SceneManagerEvent event) {
    TimeClock* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case ActionIn:
            timeclock_scene_badge_action_record(app, TcEventIn);
            consumed = true;
            break;
        case ActionOut:
            timeclock_scene_badge_action_record(app, TcEventOut);
            consumed = true;
            break;
        case ActionCancel:
            scene_manager_search_and_switch_to_previous_scene(app->scene_manager, TimeClockSceneMenu);
            consumed = true;
            break;
        case SCENE_EVENT_DONE:
            scene_manager_search_and_switch_to_previous_scene(app->scene_manager, TimeClockSceneMenu);
            consumed = true;
            break;
        default:
            break;
        }
    }

    return consumed;
}

void timeclock_scene_badge_action_on_exit(void* context) {
    TimeClock* app = context;
    popup_reset(app->popup);
}
