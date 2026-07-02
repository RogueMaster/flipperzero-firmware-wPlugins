#include "../flipper_share_app.h"

void flipper_share_scene_menu_on_enter(void* context) {
    FlipperShareApp* app = context;

    // Menu is already set up in the main application file
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperShareViewIdMenu);
}

bool flipper_share_scene_menu_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        // Events from submenus are already handled in submenu_callback
        consumed = true;
    }

    return consumed;
}

void flipper_share_scene_menu_on_exit(void* context) {
    UNUSED(context);
}
