#include "../cfw_app.h"

extern const char* const menu_style_names[MenuStyleCount];

static void gamemenu_style_callback(void* context, uint32_t index) {
    CFWApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void cfw_app_scene_interface_gamemenu_style_on_enter(void* context) {
    CFWApp* app = context;
    submenu_set_header(app->submenu, "Game Menu Style:");
    for(size_t i = 0; i < MenuStyleCount; i++) {
        submenu_add_item(app->submenu, menu_style_names[i], i, gamemenu_style_callback, app);
    }
    submenu_set_selected_item(app->submenu, cfw_settings.game_menu_style);
    view_dispatcher_switch_to_view(app->view_dispatcher, CFWAppViewSubmenu);
}

bool cfw_app_scene_interface_gamemenu_style_on_event(void* context, SceneManagerEvent event) {
    CFWApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event < MenuStyleCount) {
        cfw_settings.game_menu_style = event.event;
        app->save_settings = true;
        scene_manager_previous_scene(app->scene_manager);
    }
    return true;
}

void cfw_app_scene_interface_gamemenu_style_on_exit(void* context) {
    CFWApp* app = context;
    submenu_reset(app->submenu);
}
