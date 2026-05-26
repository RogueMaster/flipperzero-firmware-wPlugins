#include "../disn3y_toolbox_app.h"

enum DroidSubmenuIndex {
    DroidSubmenuIndexBroadcastPersonality,
    DroidSubmenuIndexBroadcastLocation,
    DroidSubmenuIndexAbout,
};

static void disn3y_toolbox_app_scene_droid_menu_submenu_callback(void* context, uint32_t index) {
    Disn3yToolboxApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void disn3y_toolbox_app_scene_droid_menu_on_enter(void* context) {
    Disn3yToolboxApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_set_header(submenu, "Droid Controller (beta)");
    submenu_add_item(
        submenu,
        "Personality Broadcaster",
        DroidSubmenuIndexBroadcastPersonality,
        disn3y_toolbox_app_scene_droid_menu_submenu_callback,
        app);
    submenu_add_item(
        submenu,
        "Location Broadcaster",
        DroidSubmenuIndexBroadcastLocation,
        disn3y_toolbox_app_scene_droid_menu_submenu_callback,
        app);
    submenu_add_item(
        submenu,
        "About",
        DroidSubmenuIndexAbout,
        disn3y_toolbox_app_scene_droid_menu_submenu_callback,
        app);

    submenu_set_selected_item(
        submenu,
        scene_manager_get_scene_state(app->scene_manager, Disn3yToolboxAppSceneDroidMenu));

    view_dispatcher_switch_to_view(app->view_dispatcher, Disn3yToolboxAppViewSubmenu);
}

bool disn3y_toolbox_app_scene_droid_menu_on_event(void* context, SceneManagerEvent event) {
    Disn3yToolboxApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(
            app->scene_manager, Disn3yToolboxAppSceneDroidMenu, event.event);
        if(event.event == DroidSubmenuIndexBroadcastPersonality) {
            scene_manager_next_scene(app->scene_manager, Disn3yToolboxAppSceneDroidPersonality);
            consumed = true;
        } else if(event.event == DroidSubmenuIndexBroadcastLocation) {
            scene_manager_next_scene(app->scene_manager, Disn3yToolboxAppSceneDroidLocation);
            consumed = true;
        } else if(event.event == DroidSubmenuIndexAbout) {
            scene_manager_next_scene(app->scene_manager, Disn3yToolboxAppSceneDroidAbout);
            consumed = true;
        }
    }

    return consumed;
}

void disn3y_toolbox_app_scene_droid_menu_on_exit(void* context) {
    Disn3yToolboxApp* app = context;
    submenu_reset(app->submenu);
}
