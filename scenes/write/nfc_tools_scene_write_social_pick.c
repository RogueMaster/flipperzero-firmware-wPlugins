#include "../../include/nfc_tools_i.h"

// Social network selection scene (alphabetically sorted submenu).
// The chosen index is stored in app->social_network_index, then the user
// proceeds to username input via NdefInput.

static void nfc_tools_scene_write_social_pick_callback(void* context, uint32_t index) {
    NfcToolsApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void nfc_tools_scene_write_social_pick_on_enter(void* context) {
    NfcToolsApp* app     = context;
    Submenu*     submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, NTS_HEADER_SOCIAL_NETWORK);

    for(uint8_t i = 0; i < nfc_tools_social_networks_count; i++) {
        submenu_add_item(
            submenu,
            nfc_tools_social_networks[i].name,
            i,
            nfc_tools_scene_write_social_pick_callback,
            app);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcToolsViewMainMenu);
}

bool nfc_tools_scene_write_social_pick_on_event(void* context, SceneManagerEvent event) {
    NfcToolsApp* app      = context;
    bool         consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        app->social_network_index = (uint8_t)event.event;
        scene_manager_next_scene(app->scene_manager, NfcToolsSceneIdWriteSocialUsername);
        consumed = true;
    }

    return consumed;
}

void nfc_tools_scene_write_social_pick_on_exit(void* context) {
    NfcToolsApp* app = context;
    submenu_reset(app->submenu);
}
