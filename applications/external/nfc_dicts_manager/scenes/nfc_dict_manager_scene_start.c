#include "../nfc_dict_manager.h"
#include "../nfc_dict_manager_utils.h"

void nfc_dict_manager_scene_start_on_enter(void* context) {
    NfcDictManager* app = context;

    submenu_reset(app->submenu);
    submenu_add_item(app->submenu, "Backup dict", 0, nfc_dict_manager_submenu_callback, app);
    submenu_add_item(app->submenu, "Select dict", 1, nfc_dict_manager_submenu_callback, app);
    submenu_add_item(app->submenu, "Merge dicts", 2, nfc_dict_manager_submenu_callback, app);
    submenu_add_item(app->submenu, "Optimize dict", 3, nfc_dict_manager_submenu_callback, app);
    submenu_add_item(app->submenu, "Edit dict (WIP)", 4, nfc_dict_manager_submenu_callback, app);
    submenu_add_item(app->submenu, "About", 5, nfc_dict_manager_submenu_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcDictManagerViewSubmenu);
}

bool nfc_dict_manager_scene_start_on_event(void* context, SceneManagerEvent event) {
    NfcDictManager* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case 0: // backup dict
            scene_manager_next_scene(app->scene_manager, NfcDictManagerSceneBackup);
            consumed = true;
            break;
        case 1: // select dict
            scene_manager_next_scene(app->scene_manager, NfcDictManagerSceneSelect);
            consumed = true;
            break;
        case 2: // merge dicts
            scene_manager_next_scene(app->scene_manager, NfcDictManagerSceneMerge);
            consumed = true;
            break;
        case 3: // optimize dict
            scene_manager_next_scene(app->scene_manager, NfcDictManagerSceneOptimize);
            consumed = true;
            break;
        case 4: // edit dict (WIP)
            // TODO: edite single keys (add/edit/remove), search key in dict...
            consumed = true;
            break;
        case 5: // about
            scene_manager_next_scene(app->scene_manager, NfcDictManagerSceneAbout);
            consumed = true;
            break;
        }
    }

    return consumed;
}

void nfc_dict_manager_scene_start_on_exit(void* context) {
    NfcDictManager* app = context;
    submenu_reset(app->submenu);
}
