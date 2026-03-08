#include "../can_commander.h"

typedef enum {
    MainMenuTools = 0,
    MainMenuProfiles,
    MainMenuSettings,
    MainMenuAbout,
} MainMenuIndex;

static void cancommander_scene_main_menu_callback(void* context, uint32_t index) {
    App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void cancommander_scene_main_menu_on_enter(void* context) {
    App* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "CAN Commander");

    submenu_add_item(app->submenu, "Tools", MainMenuTools, cancommander_scene_main_menu_callback, app);
    submenu_add_item(app->submenu, "Profiles", MainMenuProfiles, cancommander_scene_main_menu_callback, app);
    submenu_add_item(
        app->submenu, "Settings", MainMenuSettings, cancommander_scene_main_menu_callback, app);
    submenu_add_item(app->submenu, "About", MainMenuAbout, cancommander_scene_main_menu_callback, app);

    submenu_set_selected_item(
        app->submenu, scene_manager_get_scene_state(app->scene_manager, cancommander_scene_main_menu));

    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewSubmenu);
}

bool cancommander_scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    if(event.type != SceneManagerEventTypeCustom) {
        return false;
    }

    scene_manager_set_scene_state(app->scene_manager, cancommander_scene_main_menu, event.event);

    switch(event.event) {
    case MainMenuTools:
        scene_manager_next_scene(app->scene_manager, cancommander_scene_tools_menu);
        return true;

    case MainMenuProfiles:
        scene_manager_next_scene(app->scene_manager, cancommander_scene_profiles_menu);
        return true;

    case MainMenuSettings:
        scene_manager_next_scene(app->scene_manager, cancommander_scene_debug_menu);
        return true;

    case MainMenuAbout:
        app_set_status(
            app,
            "CAN Commander\nVersion %s\nMade by\nMatthew KuKanich\n\nwww.cancommander.com",
            PROGRAM_VERSION);
        scene_manager_next_scene(app->scene_manager, cancommander_scene_status);
        return true;

    default:
        return false;
    }
}

void cancommander_scene_main_menu_on_exit(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
}
