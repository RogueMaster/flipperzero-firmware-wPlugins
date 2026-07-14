// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file xiaomi_filter_reset_scene_start.c
 * @brief Main menu scene.
 */
#include "../xiaomi_filter_reset_i.h"

typedef enum {
    SubmenuIndexReset,
    SubmenuIndexCheck,
    SubmenuIndexAbout,
} SubmenuIndex;

static void xiaomi_filter_reset_scene_start_submenu_callback(void* context, uint32_t index) {
    XiaomiFilterResetApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void xiaomi_filter_reset_scene_start_on_enter(void* context) {
    XiaomiFilterResetApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "Xiaomi Filter Reset");
    submenu_add_item(
        submenu,
        "Reset filter life",
        SubmenuIndexReset,
        xiaomi_filter_reset_scene_start_submenu_callback,
        app);
    submenu_add_item(
        submenu,
        "Check filter life",
        SubmenuIndexCheck,
        xiaomi_filter_reset_scene_start_submenu_callback,
        app);
    submenu_add_item(
        submenu, "About", SubmenuIndexAbout, xiaomi_filter_reset_scene_start_submenu_callback, app);
    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, XiaomiFilterResetSceneStart));

    view_dispatcher_switch_to_view(app->view_dispatcher, XiaomiFilterResetViewSubmenu);
}

bool xiaomi_filter_reset_scene_start_on_event(void* context, SceneManagerEvent event) {
    XiaomiFilterResetApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(
            app->scene_manager, XiaomiFilterResetSceneStart, event.event);
        if(event.event == SubmenuIndexReset) {
            app->pending_op = XiaomiFilterWorkerOpReset;
            scene_manager_next_scene(app->scene_manager, XiaomiFilterResetSceneScan);
            consumed = true;
        } else if(event.event == SubmenuIndexCheck) {
            app->pending_op = XiaomiFilterWorkerOpCheck;
            scene_manager_next_scene(app->scene_manager, XiaomiFilterResetSceneScan);
            consumed = true;
        } else if(event.event == SubmenuIndexAbout) {
            scene_manager_next_scene(app->scene_manager, XiaomiFilterResetSceneAbout);
            consumed = true;
        }
    }

    return consumed;
}

void xiaomi_filter_reset_scene_start_on_exit(void* context) {
    XiaomiFilterResetApp* app = context;
    submenu_reset(app->submenu);
}
