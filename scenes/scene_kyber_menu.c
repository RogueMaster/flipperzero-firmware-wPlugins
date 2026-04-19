#include "../disney_toolbox_app.h"

enum KyberSubmenuIndex {
    KyberSubmenuIndexSeries1,
    KyberSubmenuIndexSeries2,
    KyberSubmenuIndexSeriesCheck,
    KyberSubmenuIndexAbout,
};

static void disney_toolbox_app_scene_kyber_menu_submenu_callback(void* context, uint32_t index) {
    DisneyToolboxApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void disney_toolbox_app_scene_kyber_menu_on_enter(void* context) {
    DisneyToolboxApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_set_header(submenu, "Kyber Crystal");
    submenu_add_item(submenu, "Series 1 Writer", KyberSubmenuIndexSeries1,
                     disney_toolbox_app_scene_kyber_menu_submenu_callback, app);
    submenu_add_item(submenu, "Series 2 Writer (beta)", KyberSubmenuIndexSeries2,
                     disney_toolbox_app_scene_kyber_menu_submenu_callback, app);
    submenu_add_item(submenu, "Crystal Checker", KyberSubmenuIndexSeriesCheck,
                     disney_toolbox_app_scene_kyber_menu_submenu_callback, app);
    submenu_add_item(submenu, "About", KyberSubmenuIndexAbout,
                     disney_toolbox_app_scene_kyber_menu_submenu_callback, app);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, DisneyToolboxAppSceneKyberMenu));

    view_dispatcher_switch_to_view(app->view_dispatcher, DisneyToolboxAppViewSubmenu);
}

bool disney_toolbox_app_scene_kyber_menu_on_event(void* context, SceneManagerEvent event) {
    DisneyToolboxApp* app = context;
    bool consumed = false;

    if (event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, DisneyToolboxAppSceneKyberMenu,
                                      event.event);
        if (event.event == KyberSubmenuIndexSeries1) {
            scene_manager_next_scene(app->scene_manager, DisneyToolboxAppSceneKyberSelectorS1);
            consumed = true;
        } else if (event.event == KyberSubmenuIndexSeries2) {
            scene_manager_next_scene(app->scene_manager, DisneyToolboxAppSceneKyberSelectorS2);
            consumed = true;
        } else if (event.event == KyberSubmenuIndexSeriesCheck) {
            scene_manager_next_scene(app->scene_manager, DisneyToolboxAppSceneKyberSeriesCheck);
            consumed = true;
        } else if (event.event == KyberSubmenuIndexAbout) {
            scene_manager_next_scene(app->scene_manager, DisneyToolboxAppSceneKyberAbout);
            consumed = true;
        }
    }

    return consumed;
}

void disney_toolbox_app_scene_kyber_menu_on_exit(void* context) {
    DisneyToolboxApp* app = context;
    submenu_reset(app->submenu);
}
