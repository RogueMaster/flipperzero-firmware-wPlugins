#include "../disney_toolbox_app.h"

enum DroidSubmenuIndex {
    DroidSubmenuIndexBroadcastPersonality,
    DroidSubmenuIndexBroadcastLocation,
    DroidSubmenuIndexAbout,
};

static void disney_toolbox_app_scene_droid_menu_submenu_callback(void* context, uint32_t index) {
    DisneyToolboxApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void disney_toolbox_app_scene_droid_menu_on_enter(void* context) {
    DisneyToolboxApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_set_header(submenu, "Droid Controller (beta)");
    submenu_add_item(submenu, "Personality Broadcaster", DroidSubmenuIndexBroadcastPersonality,
                     disney_toolbox_app_scene_droid_menu_submenu_callback, app);
    submenu_add_item(submenu, "Location Broadcaster", DroidSubmenuIndexBroadcastLocation,
                     disney_toolbox_app_scene_droid_menu_submenu_callback, app);
    submenu_add_item(submenu, "About", DroidSubmenuIndexAbout,
                     disney_toolbox_app_scene_droid_menu_submenu_callback, app);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, DisneyToolboxAppSceneDroidMenu));

    view_dispatcher_switch_to_view(app->view_dispatcher, DisneyToolboxAppViewSubmenu);
}

bool disney_toolbox_app_scene_droid_menu_on_event(void* context, SceneManagerEvent event) {
    DisneyToolboxApp* app = context;
    bool consumed = false;

    if (event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, DisneyToolboxAppSceneDroidMenu,
                                      event.event);
        if (event.event == DroidSubmenuIndexBroadcastPersonality) {
            scene_manager_next_scene(app->scene_manager, DisneyToolboxAppSceneDroidPersonality);
            consumed = true;
        } else if (event.event == DroidSubmenuIndexBroadcastLocation) {
            scene_manager_next_scene(app->scene_manager, DisneyToolboxAppSceneDroidLocation);
            consumed = true;
        } else if (event.event == DroidSubmenuIndexAbout) {
            scene_manager_next_scene(app->scene_manager, DisneyToolboxAppSceneDroidAbout);
            consumed = true;
        }
    }

    return consumed;
}

void disney_toolbox_app_scene_droid_menu_on_exit(void* context) {
    DisneyToolboxApp* app = context;
    submenu_reset(app->submenu);
}
