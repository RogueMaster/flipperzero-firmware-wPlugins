#include "../flipper_wedge.h"

enum SubmenuIndex {
    SubmenuIndexSettings = 10,
};

void flipper_wedge_scene_menu_submenu_callback(void* context, uint32_t index) {
    FlipperWedge* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void flipper_wedge_scene_menu_on_enter(void* context) {
    FlipperWedge* app = context;

    // Keep display backlight on while in menu
    notification_message(app->notification, &sequence_display_backlight_enforce_on);

    submenu_add_item(
        app->submenu,
        "Settings",
        SubmenuIndexSettings,
        flipper_wedge_scene_menu_submenu_callback,
        app);

    submenu_set_selected_item(
        app->submenu, scene_manager_get_scene_state(app->scene_manager, FlipperWedgeSceneMenu));

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperWedgeViewIdMenu);
}

bool flipper_wedge_scene_menu_on_event(void* context, SceneManagerEvent event) {
    FlipperWedge* app = context;
    UNUSED(app);
    if(event.type == SceneManagerEventTypeBack) {
        //exit app
        scene_manager_stop(app->scene_manager);
        view_dispatcher_stop(app->view_dispatcher);
        return true;
    } else if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubmenuIndexSettings) {
            scene_manager_set_scene_state(
                app->scene_manager, FlipperWedgeSceneMenu, SubmenuIndexSettings);
            scene_manager_next_scene(app->scene_manager, FlipperWedgeSceneSettings);
            return true;
        }
    }
    return false;
}

void flipper_wedge_scene_menu_on_exit(void* context) {
    FlipperWedge* app = context;
    submenu_reset(app->submenu);

    // Return backlight to auto mode
    notification_message(app->notification, &sequence_display_backlight_enforce_auto);
}
