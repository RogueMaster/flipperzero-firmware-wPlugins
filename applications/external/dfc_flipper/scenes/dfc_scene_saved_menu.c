#include "../dfc_i.h"

enum SubmenuIndex {
    SubmenuIndexEmulate,
    SubmenuIndexDelete,
    SubmenuIndexInfo,
};

void dfc_scene_saved_menu_submenu_callback(void* context, uint32_t index) {
    Dfc* dfc = context;

    view_dispatcher_send_custom_event(dfc->view_dispatcher, index);
}

void dfc_scene_saved_menu_on_enter(void* context) {
    Dfc* dfc = context;
    Submenu* submenu = dfc->submenu;

    submenu_add_item(
        submenu, "NFC Emulate", SubmenuIndexEmulate, dfc_scene_saved_menu_submenu_callback, dfc);

    submenu_add_item(
        submenu, "Info", SubmenuIndexInfo, dfc_scene_saved_menu_submenu_callback, dfc);
    submenu_add_item(
        submenu, "Delete", SubmenuIndexDelete, dfc_scene_saved_menu_submenu_callback, dfc);

    submenu_set_selected_item(
        dfc->submenu, scene_manager_get_scene_state(dfc->scene_manager, DfcSceneSavedMenu));

    view_dispatcher_switch_to_view(dfc->view_dispatcher, DfcViewMenu);
}

bool dfc_scene_saved_menu_on_event(void* context, SceneManagerEvent event) {
    Dfc* dfc = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(dfc->scene_manager, DfcSceneSavedMenu, event.event);

        if(event.event == SubmenuIndexEmulate) {
            scene_manager_set_scene_state(
                dfc->scene_manager, DfcSceneSavedMenu, SubmenuIndexEmulate);
            scene_manager_next_scene(dfc->scene_manager, DfcSceneEmulate);
            consumed = true;
        } else if(event.event == SubmenuIndexInfo) {
            scene_manager_set_scene_state(dfc->scene_manager, DfcSceneSavedMenu, SubmenuIndexInfo);
            scene_manager_next_scene(dfc->scene_manager, DfcSceneInfo);
            consumed = true;
        } else if(event.event == SubmenuIndexDelete) {
            scene_manager_set_scene_state(
                dfc->scene_manager, DfcSceneSavedMenu, SubmenuIndexDelete);
            scene_manager_next_scene(dfc->scene_manager, DfcSceneDelete);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        // Back goes one step, to the list this credential was chosen from. Falling
        // through to the default handling does that; jumping to the main menu here
        // skipped the list, so it could only be reached by starting again.
        dfc_credential_clear(dfc->credential);
    }

    return consumed;
}

void dfc_scene_saved_menu_on_exit(void* context) {
    Dfc* dfc = context;

    submenu_reset(dfc->submenu);
}
