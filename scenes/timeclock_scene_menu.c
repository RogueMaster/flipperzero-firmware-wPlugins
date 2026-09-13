// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#include "../timeclock.h"

typedef enum {
    MenuIndexWork,
    MenuIndexBadges,
    MenuIndexPunch,
    MenuIndexOverview,
    MenuIndexHistory,
    MenuIndexExport,
    MenuIndexSettings,
    MenuIndexAbout,
} MenuIndex;

static void timeclock_scene_menu_submenu_callback(void* context, uint32_t index) {
    TimeClock* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void timeclock_scene_menu_on_enter(void* context) {
    TimeClock* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "Staff Time Clock");
    submenu_add_item(
        submenu, tc_str(StrWorkMode), MenuIndexWork, timeclock_scene_menu_submenu_callback, app);
    submenu_add_item(
        submenu, tc_str(StrBadges), MenuIndexBadges, timeclock_scene_menu_submenu_callback, app);
    submenu_add_item(
        submenu, tc_str(StrPunch), MenuIndexPunch, timeclock_scene_menu_submenu_callback, app);
    submenu_add_item(
        submenu,
        tc_str(StrOverview),
        MenuIndexOverview,
        timeclock_scene_menu_submenu_callback,
        app);
    submenu_add_item(
        submenu, tc_str(StrHistory), MenuIndexHistory, timeclock_scene_menu_submenu_callback, app);
    submenu_add_item(
        submenu, tc_str(StrExport), MenuIndexExport, timeclock_scene_menu_submenu_callback, app);
    submenu_add_item(
        submenu, tc_str(StrSettings), MenuIndexSettings, timeclock_scene_menu_submenu_callback, app);
    submenu_add_item(
        submenu, tc_str(StrAbout), MenuIndexAbout, timeclock_scene_menu_submenu_callback, app);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, TimeClockSceneMenu));

    view_dispatcher_switch_to_view(app->view_dispatcher, TimeClockViewSubmenu);
}

bool timeclock_scene_menu_on_event(void* context, SceneManagerEvent event) {
    TimeClock* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, TimeClockSceneMenu, event.event);
        consumed = true;
        switch(event.event) {
        case MenuIndexPunch:
            app->scan_purpose = TcScanPunch;
            scene_manager_next_scene(app->scene_manager, TimeClockSceneScan);
            break;
        case MenuIndexWork:
            scene_manager_next_scene(app->scene_manager, TimeClockSceneWork);
            break;
        case MenuIndexBadges:
            scene_manager_next_scene(app->scene_manager, TimeClockSceneBadgeList);
            break;
        case MenuIndexOverview:
            scene_manager_next_scene(app->scene_manager, TimeClockSceneOverview);
            break;
        case MenuIndexHistory:
            scene_manager_next_scene(app->scene_manager, TimeClockSceneHistoryMenu);
            break;
        case MenuIndexExport:
            scene_manager_next_scene(app->scene_manager, TimeClockSceneExport);
            break;
        case MenuIndexSettings:
            scene_manager_next_scene(app->scene_manager, TimeClockSceneSettings);
            break;
        case MenuIndexAbout:
            scene_manager_next_scene(app->scene_manager, TimeClockSceneAbout);
            break;
        default:
            consumed = false;
            break;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        // Back at the menu always tries to leave the app: if a PIN is set it
        // asks for it first (same flow as Settings -> Exit), otherwise it
        // closes immediately.
        if(app->config.pin_enabled) {
            app->pin_mode = TcPinModeVerifyExit;
            scene_manager_next_scene(app->scene_manager, TimeClockScenePinSet);
            consumed = true;
        } else {
            consumed = false; // allow the app to close
        }
    }

    return consumed;
}

void timeclock_scene_menu_on_exit(void* context) {
    UNUSED(context);
}
