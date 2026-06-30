#include "../can_commander.h"

typedef enum {
    ProfilesLoadSmartInjection = 0,
    ProfilesLoadDbcConfig,
} ProfilesMenuIndex;

static void cancommander_scene_profiles_menu_callback(void* context, uint32_t index) {
    App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void cancommander_scene_profiles_menu_on_enter(void* context) {
    App* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Profiles");

    submenu_add_item(
        app->submenu,
        "Smart Injection Profiles",
        ProfilesLoadSmartInjection,
        cancommander_scene_profiles_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "DBC Decoding Profiles",
        ProfilesLoadDbcConfig,
        cancommander_scene_profiles_menu_callback,
        app);

    submenu_set_selected_item(
        app->submenu,
        scene_manager_get_scene_state(app->scene_manager, cancommander_scene_profiles_menu));

    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewSubmenu);
}

bool cancommander_scene_profiles_menu_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    if(event.type != SceneManagerEventTypeCustom) {
        return false;
    }

    scene_manager_set_scene_state(app->scene_manager, cancommander_scene_profiles_menu, event.event);

    switch(event.event) {
    case ProfilesLoadSmartInjection:
        scene_manager_next_scene(app->scene_manager, cancommander_scene_custom_inject_load_slots_menu);
        return true;

    case ProfilesLoadDbcConfig:
        scene_manager_next_scene(app->scene_manager, cancommander_scene_dbc_load_config_menu);
        return true;

    default:
        return false;
    }
}

void cancommander_scene_profiles_menu_on_exit(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
}
