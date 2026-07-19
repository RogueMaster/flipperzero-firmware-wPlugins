#include "../ir_share_app.h"

void ir_share_scene_menu_on_enter(void* context) {
    IrShareApp* app = context;

    // Menu is already set up in the main application file
    view_dispatcher_switch_to_view(app->view_dispatcher, IrShareViewIdMenu);
}

bool ir_share_scene_menu_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        // Events from submenus are already handled in submenu_callback
        consumed = true;
    }

    return consumed;
}

void ir_share_scene_menu_on_exit(void* context) {
    UNUSED(context);
}
