#include "../dfc_i.h"
#include "../dfc_credential_storage.h"

enum SubmenuIndex {
    SubmenuIndexUpdate,
    SubmenuIndexSaveCopy,
    SubmenuIndexSave,
    SubmenuIndexDiscard,
};

static void dfc_scene_mutation_save_submenu_callback(void* context, uint32_t index) {
    Dfc* dfc = context;
    view_dispatcher_send_custom_event(dfc->view_dispatcher, index);
}

static void dfc_scene_mutation_save_return(Dfc* dfc) {
    dfc->emulate_from_blank = false;
    if(!furi_string_empty(dfc_credential_storage_load_path())) {
        scene_manager_search_and_switch_to_previous_scene(dfc->scene_manager, DfcSceneSavedMenu);
    } else {
        scene_manager_search_and_switch_to_previous_scene(dfc->scene_manager, DfcSceneMainMenu);
    }
}

void dfc_scene_mutation_save_on_enter(void* context) {
    Dfc* dfc = context;
    Submenu* submenu = dfc->submenu;

    if(!furi_string_empty(dfc_credential_storage_load_path())) {
        submenu_add_item(
            submenu,
            "Update File",
            SubmenuIndexUpdate,
            dfc_scene_mutation_save_submenu_callback,
            dfc);
        submenu_add_item(
            submenu,
            "Save Copy",
            SubmenuIndexSaveCopy,
            dfc_scene_mutation_save_submenu_callback,
            dfc);
    } else {
        submenu_add_item(
            submenu, "Save", SubmenuIndexSave, dfc_scene_mutation_save_submenu_callback, dfc);
    }

    submenu_add_item(
        submenu, "Discard", SubmenuIndexDiscard, dfc_scene_mutation_save_submenu_callback, dfc);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(dfc->scene_manager, DfcSceneMutationSave));
    view_dispatcher_switch_to_view(dfc->view_dispatcher, DfcViewMenu);
}

bool dfc_scene_mutation_save_on_event(void* context, SceneManagerEvent event) {
    Dfc* dfc = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(dfc->scene_manager, DfcSceneMutationSave, event.event);
        if(event.event == SubmenuIndexUpdate) {
            if(dfc_credential_save_loaded(dfc->credential)) {
                scene_manager_next_scene(dfc->scene_manager, DfcSceneSaveSuccess);
            } else {
                dfc_scene_mutation_save_return(dfc);
            }
            consumed = true;
        } else if(event.event == SubmenuIndexSaveCopy || event.event == SubmenuIndexSave) {
            scene_manager_next_scene(dfc->scene_manager, DfcSceneSaveName);
            consumed = true;
        } else if(event.event == SubmenuIndexDiscard) {
            dfc_credential_clear_dirty(dfc->credential);
            dfc_scene_mutation_save_return(dfc);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        consumed = scene_manager_previous_scene(dfc->scene_manager);
    }

    return consumed;
}

void dfc_scene_mutation_save_on_exit(void* context) {
    Dfc* dfc = context;
    submenu_reset(dfc->submenu);
}
