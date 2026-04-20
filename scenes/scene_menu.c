#include "../disney_toolbox_app.h"

enum SubmenuIndex {
    SubmenuIndexKyberCrystals,
    SubmenuIndexMagicBand,
    SubmenuIndexDroidController,
    SubmenuIndexAbout,
};

static void disney_toolbox_app_scene_menu_submenu_callback(void* context, uint32_t index) {
    DisneyToolboxApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void disney_toolbox_app_scene_menu_on_enter(void* context) {
    DisneyToolboxApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_set_header(submenu, "Disney Toolbox");
    submenu_add_item(submenu, "Kyber Crystal Writer", SubmenuIndexKyberCrystals,
                     disney_toolbox_app_scene_menu_submenu_callback, app);
    submenu_add_item(submenu, "MagicBand+ Beacon", SubmenuIndexMagicBand,
                     disney_toolbox_app_scene_menu_submenu_callback, app);
    submenu_add_item(submenu, "Droid Controller (beta)", SubmenuIndexDroidController,
                     disney_toolbox_app_scene_menu_submenu_callback, app);
    submenu_add_item(submenu, "About", SubmenuIndexAbout,
                     disney_toolbox_app_scene_menu_submenu_callback, app);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, DisneyToolboxAppSceneMenu));

    view_dispatcher_switch_to_view(app->view_dispatcher, DisneyToolboxAppViewSubmenu);
}

bool disney_toolbox_app_scene_menu_on_event(void* context, SceneManagerEvent event) {
    DisneyToolboxApp* app = context;
    bool consumed = false;

    if (event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, DisneyToolboxAppSceneMenu, event.event);
        if (event.event == SubmenuIndexKyberCrystals) {
            scene_manager_next_scene(app->scene_manager, DisneyToolboxAppSceneKyberMenu);
            consumed = true;
        } else if (event.event == SubmenuIndexMagicBand) {
            scene_manager_next_scene(app->scene_manager, DisneyToolboxAppSceneMbMenu);
            consumed = true;
        } else if (event.event == SubmenuIndexDroidController) {
            scene_manager_next_scene(app->scene_manager, DisneyToolboxAppSceneDroidMenu);
            consumed = true;
        } else if (event.event == SubmenuIndexAbout) {
            scene_manager_next_scene(app->scene_manager, DisneyToolboxAppSceneAbout);
            consumed = true;
        }
    }

    return consumed;
}

void disney_toolbox_app_scene_menu_on_exit(void* context) {
    DisneyToolboxApp* app = context;
    submenu_reset(app->submenu);
}
