#include "../nfc_share_app.h"

void nfc_share_scene_menu_on_enter(void* context) {
    NfcShareApp* app = context;

    // Menu is already set up in the main application file
    view_dispatcher_switch_to_view(app->view_dispatcher, NfcShareViewIdMenu);
}

bool nfc_share_scene_menu_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        // Events from submenus are already handled in submenu_callback
        consumed = true;
    }

    return consumed;
}

void nfc_share_scene_menu_on_exit(void* context) {
    UNUSED(context);
}
