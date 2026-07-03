#include "../disn3y_toolbox_app.h"

enum SubmenuIndex {
    SubmenuIndexKyberCrystals,
    SubmenuIndexMagicBand,
    SubmenuIndexDroidController,
    SubmenuIndexAbout,
};

static void disn3y_toolbox_app_scene_menu_submenu_callback(void* context, uint32_t index) {
    Disn3yToolboxApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void disn3y_toolbox_app_scene_menu_on_enter(void* context) {
    Disn3yToolboxApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_set_header(submenu, "Disn3y Toolbox");
    submenu_add_item(
        submenu,
        "Kyber Crystal Writer",
        SubmenuIndexKyberCrystals,
        disn3y_toolbox_app_scene_menu_submenu_callback,
        app);
    submenu_add_item(
        submenu,
        "MagicBand+ Beacon",
        SubmenuIndexMagicBand,
        disn3y_toolbox_app_scene_menu_submenu_callback,
        app);
    submenu_add_item(
        submenu,
        "Droid Controller (beta)",
        SubmenuIndexDroidController,
        disn3y_toolbox_app_scene_menu_submenu_callback,
        app);
    submenu_add_item(
        submenu, "About", SubmenuIndexAbout, disn3y_toolbox_app_scene_menu_submenu_callback, app);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, Disn3yToolboxAppSceneMenu));

    view_dispatcher_switch_to_view(app->view_dispatcher, Disn3yToolboxAppViewSubmenu);
}

bool disn3y_toolbox_app_scene_menu_on_event(void* context, SceneManagerEvent event) {
    Disn3yToolboxApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, Disn3yToolboxAppSceneMenu, event.event);
        if(event.event == SubmenuIndexKyberCrystals) {
            scene_manager_next_scene(app->scene_manager, Disn3yToolboxAppSceneKyberMenu);
            consumed = true;
        } else if(event.event == SubmenuIndexMagicBand) {
            scene_manager_next_scene(app->scene_manager, Disn3yToolboxAppSceneMbMenu);
            consumed = true;
        } else if(event.event == SubmenuIndexDroidController) {
            scene_manager_next_scene(app->scene_manager, Disn3yToolboxAppSceneDroidMenu);
            consumed = true;
        } else if(event.event == SubmenuIndexAbout) {
            scene_manager_next_scene(app->scene_manager, Disn3yToolboxAppSceneAbout);
            consumed = true;
        }
    }

    return consumed;
}

void disn3y_toolbox_app_scene_menu_on_exit(void* context) {
    Disn3yToolboxApp* app = context;
    submenu_reset(app->submenu);
}
