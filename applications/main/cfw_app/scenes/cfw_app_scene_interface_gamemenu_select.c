#include "../cfw_app.h"

static void gamemenu_select_callback(void* context, uint32_t index) {
    CFWApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void cfw_app_scene_interface_gamemenu_select_on_enter(void* context) {
    CFWApp* app = context;
    bool selecting_start =
        scene_manager_get_scene_state(app->scene_manager, CFWAppSceneInterfaceGamemenuSelect);
    submenu_set_header(app->submenu, selecting_start ? "Starting Game:" : "Choose Game:");
    for(size_t i = 0; i < CharList_size(app->gamemenu_app_labels); i++) {
        submenu_add_item(
            app->submenu,
            *CharList_get(app->gamemenu_app_labels, i),
            i,
            gamemenu_select_callback,
            app);
    }
    submenu_set_selected_item(
        app->submenu, selecting_start ? cfw_settings.game_start_point : app->gamemenu_app_index);
    view_dispatcher_switch_to_view(app->view_dispatcher, CFWAppViewSubmenu);
}

bool cfw_app_scene_interface_gamemenu_select_on_event(void* context, SceneManagerEvent event) {
    CFWApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event < CharList_size(app->gamemenu_app_labels)) {
        if(scene_manager_get_scene_state(app->scene_manager, CFWAppSceneInterfaceGamemenuSelect)) {
            cfw_settings.game_start_point = event.event;
            app->save_settings = true;
        } else {
            app->gamemenu_app_index = event.event;
        }
        scene_manager_previous_scene(app->scene_manager);
    }
    return true;
}

void cfw_app_scene_interface_gamemenu_select_on_exit(void* context) {
    CFWApp* app = context;
    submenu_reset(app->submenu);
}
