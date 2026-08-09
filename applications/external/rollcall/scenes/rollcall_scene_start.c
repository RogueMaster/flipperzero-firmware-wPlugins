#include "../rollcall_i.h"

typedef enum {
    StartIndexCheck,
    StartIndexHunt,
    StartIndexSettings,
    StartIndexAbout,
} StartIndex;

static void rollcall_scene_start_submenu_cb(void* context, uint32_t index) {
    RollCallApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void rollcall_scene_start_on_enter(void* context) {
    RollCallApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "RollCall");
    submenu_add_item(
        submenu, "Run Health Check", StartIndexCheck, rollcall_scene_start_submenu_cb, app);
    submenu_add_item(
        submenu, "Find My Remote", StartIndexHunt, rollcall_scene_start_submenu_cb, app);
    submenu_add_item(
        submenu, "Settings", StartIndexSettings, rollcall_scene_start_submenu_cb, app);
    submenu_add_item(
        submenu, "How it works", StartIndexAbout, rollcall_scene_start_submenu_cb, app);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, RollCallSceneStart));

    view_dispatcher_switch_to_view(app->view_dispatcher, RollCallViewSubmenu);
}

bool rollcall_scene_start_on_event(void* context, SceneManagerEvent event) {
    RollCallApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, RollCallSceneStart, event.event);
        switch(event.event) {
        case StartIndexCheck:
            scene_manager_next_scene(app->scene_manager, RollCallSceneCapture);
            consumed = true;
            break;
        case StartIndexHunt:
            scene_manager_next_scene(app->scene_manager, RollCallSceneHunt);
            consumed = true;
            break;
        case StartIndexSettings:
            scene_manager_next_scene(app->scene_manager, RollCallSceneSettings);
            consumed = true;
            break;
        case StartIndexAbout:
            scene_manager_next_scene(app->scene_manager, RollCallSceneAbout);
            consumed = true;
            break;
        default:
            break;
        }
    }
    return consumed;
}

void rollcall_scene_start_on_exit(void* context) {
    RollCallApp* app = context;
    submenu_reset(app->submenu);
}
