#include "../warden_i.h"

typedef enum {
    StartIndexGrade,
    StartIndexSettings,
    StartIndexAbout,
} StartIndex;

static void warden_scene_start_submenu_cb(void* context, uint32_t index) {
    WardenApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void warden_scene_start_on_enter(void* context) {
    WardenApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "Warden");
    submenu_add_item(
        submenu, "Grade a Card", StartIndexGrade, warden_scene_start_submenu_cb, app);
    submenu_add_item(
        submenu, "Settings", StartIndexSettings, warden_scene_start_submenu_cb, app);
    submenu_add_item(submenu, "About", StartIndexAbout, warden_scene_start_submenu_cb, app);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, WardenSceneStart));

    view_dispatcher_switch_to_view(app->view_dispatcher, WardenViewSubmenu);
}

bool warden_scene_start_on_event(void* context, SceneManagerEvent event) {
    WardenApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, WardenSceneStart, event.event);
        switch(event.event) {
        case StartIndexGrade:
            scene_manager_next_scene(app->scene_manager, WardenSceneScan);
            consumed = true;
            break;
        case StartIndexSettings:
            scene_manager_next_scene(app->scene_manager, WardenSceneSettings);
            consumed = true;
            break;
        case StartIndexAbout:
            scene_manager_next_scene(app->scene_manager, WardenSceneAbout);
            consumed = true;
            break;
        default:
            break;
        }
    }
    return consumed;
}

void warden_scene_start_on_exit(void* context) {
    WardenApp* app = context;
    submenu_reset(app->submenu);
}
