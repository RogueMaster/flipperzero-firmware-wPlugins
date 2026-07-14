#include "../seos_i.h"
enum SubmenuIndex {
    SubmenuIndexBLEReaderScanner,
    SubmenuIndexBLECredScanner,
};

void seos_scene_scanner_menu_submenu_callback(void* context, uint32_t index) {
    Seos* seos = context;
    view_dispatcher_send_custom_event(seos->view_dispatcher, index);
}

void seos_scene_scanner_menu_on_enter(void* context) {
    Seos* seos = context;
    Submenu* submenu = seos->submenu;

    submenu_add_item(
        submenu,
        "Start BLE Reader Scanner",
        SubmenuIndexBLEReaderScanner,
        seos_scene_scanner_menu_submenu_callback,
        seos);
    submenu_add_item(
        submenu,
        "Start BLE Cred Scanner",
        SubmenuIndexBLECredScanner,
        seos_scene_scanner_menu_submenu_callback,
        seos);

    submenu_set_selected_item(
        seos->submenu, scene_manager_get_scene_state(seos->scene_manager, SeosSceneMainMenu));

    view_dispatcher_switch_to_view(seos->view_dispatcher, SeosViewMenu);
}

bool seos_scene_scanner_menu_on_event(void* context, SceneManagerEvent event) {
    Seos* seos = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubmenuIndexBLEReaderScanner) {
            scene_manager_set_scene_state(
                seos->scene_manager, SeosSceneMainMenu, SubmenuIndexBLEReaderScanner);
            seos->flow_mode = FLOW_READER_SCANNER;
            scene_manager_next_scene(seos->scene_manager, SeosSceneBleCentral);
            consumed = true;
        } else if(event.event == SubmenuIndexBLECredScanner) {
            scene_manager_set_scene_state(
                seos->scene_manager, SeosSceneMainMenu, SubmenuIndexBLECredScanner);
            seos->flow_mode = FLOW_CRED_SCANNER;
            scene_manager_next_scene(seos->scene_manager, SeosSceneBleCentral);
            consumed = true;
        }
    }

    return consumed;
}

void seos_scene_scanner_menu_on_exit(void* context) {
    Seos* seos = context;
    submenu_reset(seos->submenu);
}
