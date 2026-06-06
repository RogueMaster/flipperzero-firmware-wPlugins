#include "../disn3y_toolbox_app.h"

enum KyberSubmenuIndex {
    KyberSubmenuIndexSeries1,
    KyberSubmenuIndexSeries2,
    KyberSubmenuIndexSeriesCheck,
    KyberSubmenuIndexAbout,
};

static void disn3y_toolbox_app_scene_kyber_menu_submenu_callback(void* context, uint32_t index) {
    Disn3yToolboxApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void disn3y_toolbox_app_scene_kyber_menu_on_enter(void* context) {
    Disn3yToolboxApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_set_header(submenu, "Kyber Crystal");
    submenu_add_item(
        submenu,
        "Series 1 Writer",
        KyberSubmenuIndexSeries1,
        disn3y_toolbox_app_scene_kyber_menu_submenu_callback,
        app);
    submenu_add_item(
        submenu,
        "Series 2 Writer (beta)",
        KyberSubmenuIndexSeries2,
        disn3y_toolbox_app_scene_kyber_menu_submenu_callback,
        app);
    submenu_add_item(
        submenu,
        "Crystal Checker",
        KyberSubmenuIndexSeriesCheck,
        disn3y_toolbox_app_scene_kyber_menu_submenu_callback,
        app);
    submenu_add_item(
        submenu,
        "About",
        KyberSubmenuIndexAbout,
        disn3y_toolbox_app_scene_kyber_menu_submenu_callback,
        app);

    submenu_set_selected_item(
        submenu,
        scene_manager_get_scene_state(app->scene_manager, Disn3yToolboxAppSceneKyberMenu));

    view_dispatcher_switch_to_view(app->view_dispatcher, Disn3yToolboxAppViewSubmenu);
}

bool disn3y_toolbox_app_scene_kyber_menu_on_event(void* context, SceneManagerEvent event) {
    Disn3yToolboxApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(
            app->scene_manager, Disn3yToolboxAppSceneKyberMenu, event.event);
        if(event.event == KyberSubmenuIndexSeries1) {
            scene_manager_next_scene(app->scene_manager, Disn3yToolboxAppSceneKyberSelectorS1);
            consumed = true;
        } else if(event.event == KyberSubmenuIndexSeries2) {
            scene_manager_next_scene(app->scene_manager, Disn3yToolboxAppSceneKyberSelectorS2);
            consumed = true;
        } else if(event.event == KyberSubmenuIndexSeriesCheck) {
            scene_manager_next_scene(app->scene_manager, Disn3yToolboxAppSceneKyberSeriesCheck);
            consumed = true;
        } else if(event.event == KyberSubmenuIndexAbout) {
            scene_manager_next_scene(app->scene_manager, Disn3yToolboxAppSceneKyberAbout);
            consumed = true;
        }
    }

    return consumed;
}

void disn3y_toolbox_app_scene_kyber_menu_on_exit(void* context) {
    Disn3yToolboxApp* app = context;
    submenu_reset(app->submenu);
}
