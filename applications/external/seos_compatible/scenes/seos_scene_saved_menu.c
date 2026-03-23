#include "../seos_i.h"

enum SubmenuIndex {
    SubmenuIndexEmulate,
    SubmenuIndexBLEEmulateCentral,
    SubmenuIndexBLEEmulatePeripheral,
    SubmenuIndexDelete,
    SubmenuIndexWrite,
    SubmenuIndexInfo,
};

void seos_scene_saved_menu_submenu_callback(void* context, uint32_t index) {
    Seos* seos = context;

    view_dispatcher_send_custom_event(seos->view_dispatcher, index);
}

void seos_scene_saved_menu_on_enter(void* context) {
    Seos* seos = context;
    Submenu* submenu = seos->submenu;

    submenu_add_item(
        submenu, "NFC Emulate", SubmenuIndexEmulate, seos_scene_saved_menu_submenu_callback, seos);

    if(seos->has_external_ble) {
        submenu_add_item(
            submenu,
            "BLE Emulate Central",
            SubmenuIndexBLEEmulateCentral,
            seos_scene_saved_menu_submenu_callback,
            seos);
    }
    submenu_add_item(
        submenu,
        "BLE Emulate Peripheral",
        SubmenuIndexBLEEmulatePeripheral,
        seos_scene_saved_menu_submenu_callback,
        seos);

    submenu_add_item(
        submenu, "Write", SubmenuIndexWrite, seos_scene_saved_menu_submenu_callback, seos);
    submenu_add_item(
        submenu, "Info", SubmenuIndexInfo, seos_scene_saved_menu_submenu_callback, seos);
    submenu_add_item(
        submenu, "Delete", SubmenuIndexDelete, seos_scene_saved_menu_submenu_callback, seos);

    submenu_set_selected_item(
        seos->submenu, scene_manager_get_scene_state(seos->scene_manager, SeosSceneSavedMenu));

    view_dispatcher_switch_to_view(seos->view_dispatcher, SeosViewMenu);
}

bool seos_scene_saved_menu_on_event(void* context, SceneManagerEvent event) {
    Seos* seos = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(seos->scene_manager, SeosSceneSavedMenu, event.event);

        if(event.event == SubmenuIndexEmulate) {
            scene_manager_set_scene_state(
                seos->scene_manager, SeosSceneSavedMenu, SubmenuIndexEmulate);
            scene_manager_next_scene(seos->scene_manager, SeosSceneEmulate);
            consumed = true;
        } else if(event.event == SubmenuIndexBLEEmulateCentral) {
            scene_manager_set_scene_state(
                seos->scene_manager, SeosSceneSavedMenu, SubmenuIndexBLEEmulateCentral);
            seos->flow_mode = FLOW_CRED;
            scene_manager_next_scene(seos->scene_manager, SeosSceneBleCentral);
            consumed = true;
        } else if(event.event == SubmenuIndexBLEEmulatePeripheral) {
            scene_manager_set_scene_state(
                seos->scene_manager, SeosSceneSavedMenu, SubmenuIndexBLEEmulatePeripheral);
            seos->flow_mode = FLOW_CRED;
            scene_manager_next_scene(seos->scene_manager, SeosSceneBlePeripheral);
            consumed = true;
        } else if(event.event == SubmenuIndexWrite) {
            scene_manager_set_scene_state(
                seos->scene_manager, SeosSceneSavedMenu, SubmenuIndexWrite);
            scene_manager_next_scene(seos->scene_manager, SeosSceneWrite);
            consumed = true;
        } else if(event.event == SubmenuIndexInfo) {
            scene_manager_set_scene_state(
                seos->scene_manager, SeosSceneSavedMenu, SubmenuIndexInfo);
            scene_manager_next_scene(seos->scene_manager, SeosSceneInfo);
            consumed = true;
        } else if(event.event == SubmenuIndexDelete) {
            scene_manager_set_scene_state(
                seos->scene_manager, SeosSceneSavedMenu, SubmenuIndexDelete);
            scene_manager_next_scene(seos->scene_manager, SeosSceneDelete);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        seos_credential_clear(seos->credential);
        scene_manager_search_and_switch_to_previous_scene(seos->scene_manager, SeosSceneMainMenu);
        consumed = true;
    }

    return consumed;
}

void seos_scene_saved_menu_on_exit(void* context) {
    Seos* seos = context;

    submenu_reset(seos->submenu);
}
