#include "../../include/nfc_tools_i.h"

// Search engine selection scene (alphabetically sorted submenu).
// The chosen index is stored in app->search_engine_index, then the user
// proceeds to keyword input via NdefInput.

static void nfc_tools_scene_write_search_pick_callback(void* context, uint32_t index) {
    NfcToolsApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void nfc_tools_scene_write_search_pick_on_enter(void* context) {
    NfcToolsApp* app     = context;
    Submenu*     submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, NTS_HEADER_SEARCH_ENGINE);

    for(uint8_t i = 0; i < nfc_tools_search_engines_count; i++) {
        submenu_add_item(
            submenu,
            nfc_tools_search_engines[i].name,
            i,
            nfc_tools_scene_write_search_pick_callback,
            app);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcToolsViewMainMenu);
}

bool nfc_tools_scene_write_search_pick_on_event(void* context, SceneManagerEvent event) {
    NfcToolsApp* app      = context;
    bool         consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        app->search_engine_index = (uint8_t)event.event;
        scene_manager_next_scene(app->scene_manager, NfcToolsSceneIdWriteSearchQuery);
        consumed = true;
    }

    return consumed;
}

void nfc_tools_scene_write_search_pick_on_exit(void* context) {
    NfcToolsApp* app = context;
    submenu_reset(app->submenu);
}
