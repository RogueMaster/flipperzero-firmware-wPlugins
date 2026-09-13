// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#include "../timeclock.h"

// Entry point for every time-based view, so the main menu only needs one
// "History" button instead of separate Today/Week/Month/History buttons.
//   All        -> full raw punch log (History scene, filter code 0)
//   Today      -> today's summary (first-in/last-out/total/break/overtime)
//   This week  -> per-day totals for the current week
//   This month -> per-collaborator totals for the current month
// (History scene filter code 1 is reserved for the per-collaborator filter
// reached from Badges, not used here.)

typedef enum {
    HistoryMenuAll = 0,
    HistoryMenuToday,
    HistoryMenuWeek,
    HistoryMenuMonth,
} HistoryMenuIndex;

static void timeclock_scene_history_menu_submenu_callback(void* context, uint32_t index) {
    TimeClock* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void timeclock_scene_history_menu_on_enter(void* context) {
    TimeClock* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, tc_str(StrHistory));
    submenu_add_item(
        submenu, tc_str(StrAll), HistoryMenuAll, timeclock_scene_history_menu_submenu_callback, app);
    submenu_add_item(
        submenu,
        tc_str(StrToday),
        HistoryMenuToday,
        timeclock_scene_history_menu_submenu_callback,
        app);
    submenu_add_item(
        submenu,
        tc_str(StrThisWeek),
        HistoryMenuWeek,
        timeclock_scene_history_menu_submenu_callback,
        app);
    submenu_add_item(
        submenu,
        tc_str(StrThisMonth),
        HistoryMenuMonth,
        timeclock_scene_history_menu_submenu_callback,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, TimeClockViewSubmenu);
}

bool timeclock_scene_history_menu_on_event(void* context, SceneManagerEvent event) {
    TimeClock* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        switch(event.event) {
        case HistoryMenuAll:
            scene_manager_set_scene_state(app->scene_manager, TimeClockSceneHistory, 0);
            scene_manager_next_scene(app->scene_manager, TimeClockSceneHistory);
            break;
        case HistoryMenuToday:
            scene_manager_next_scene(app->scene_manager, TimeClockSceneToday);
            break;
        case HistoryMenuWeek:
            scene_manager_next_scene(app->scene_manager, TimeClockSceneWeek);
            break;
        case HistoryMenuMonth:
            scene_manager_next_scene(app->scene_manager, TimeClockSceneMonth);
            break;
        default:
            consumed = false;
            break;
        }
    }

    return consumed;
}

void timeclock_scene_history_menu_on_exit(void* context) {
    UNUSED(context);
}
