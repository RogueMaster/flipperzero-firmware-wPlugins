// Copyright (c) 2024 Aaron Janeiro Stone
// Licensed under the MIT license.
// See the LICENSE.txt file in the project root for details.

#include "../fencing_testbox.h"

void main_menu_scene_callback(void* context, uint32_t index) {
    FencingTestboxScene scene_id = index;
    FURI_LOG_D(TAG, "Main menu callback %d", scene_id);
    FencingTestboxApp* app = (FencingTestboxApp*)context;
    furi_assert(app);

    switch(scene_id) {
    case FencingTestboxSceneWiring:
    case FencingTestboxSceneTestbox:
        scene_manager_handle_custom_event(app->scene_manager, scene_id);
        break;

    default:
        break;
    }
}

bool main_menu_scene_on_event(void* context, SceneManagerEvent index) {
    FURI_LOG_D(TAG, "Main menu event %d", (int)index.event);
    FencingTestboxApp* app = (FencingTestboxApp*)context;
    furi_assert(app);

    if(index.type == SceneManagerEventTypeBack) {
        FURI_LOG_D(TAG, "Exiting app from main menu");
        view_dispatcher_stop(app->view_dispatcher); // Signal exit
        return true;
    }

    switch(index.event) {
    case FencingTestboxSceneWiring:
    case FencingTestboxSceneTestbox:
        scene_manager_next_scene(app->scene_manager, index.event);
        return true;

    default:
        return false;
    }
}

void main_menu_scene_on_enter(void* context) {
    FURI_LOG_D(TAG, "Main menu enter");
    FencingTestboxApp* app = (FencingTestboxApp*)context;
    submenu_reset(app->submenu);

    submenu_set_header(app->submenu, "Fencing Testbox");

    submenu_add_item(
        app->submenu, "Wiring", FencingTestboxSceneWiring, main_menu_scene_callback, app);
    submenu_add_item(
        app->submenu, "Testbox", FencingTestboxSceneTestbox, main_menu_scene_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, FencingTestboxSceneMainMenu);
}

void main_menu_scene_on_exit(void* context) {
    FURI_LOG_D(TAG, "Main menu exit");
    FencingTestboxApp* app = (FencingTestboxApp*)context;
    submenu_reset(app->submenu);
}
