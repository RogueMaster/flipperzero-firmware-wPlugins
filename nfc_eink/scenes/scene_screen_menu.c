#include "../nfc_eink_app_i.h"

#define TAG "NfcEinkSceneScreenMenu"

typedef enum {
    SubmenuIndexShow,
    SubmenuIndexSave,
    SubmenuIndexWrite,
    SubmenuIndexDelete,
    SubmenuIndexInfo,
} SubmenuIndex;

static void nfc_eink_scene_screen_menu_submenu_callback(void* context, uint32_t index) {
    NfcEinkApp* instance = context;
    view_dispatcher_send_custom_event(instance->view_dispatcher, index);
}

void nfc_eink_scene_screen_menu_on_enter(void* context) {
    NfcEinkApp* instance = context;
    Submenu* submenu = instance->submenu;

    bool after_emulation =
        scene_manager_has_previous_scene(instance->scene_manager, NfcEinkAppSceneEmulate);
    if(!after_emulation) {
        submenu_add_item(
            submenu,
            "Write",
            SubmenuIndexWrite,
            nfc_eink_scene_screen_menu_submenu_callback,
            instance);
        submenu_add_item(
            submenu,
            "Delete",
            SubmenuIndexDelete,
            nfc_eink_scene_screen_menu_submenu_callback,
            instance);
    } else {
        submenu_add_item(
            submenu,
            "Save",
            SubmenuIndexSave,
            nfc_eink_scene_screen_menu_submenu_callback,
            instance);
    }
    submenu_add_item(
        submenu, "Show", SubmenuIndexShow, nfc_eink_scene_screen_menu_submenu_callback, instance);
    submenu_add_item(
        submenu, "Info", SubmenuIndexInfo, nfc_eink_scene_screen_menu_submenu_callback, instance);

    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcEinkViewMenu);
}

static NfcEinkAppScene nfc_eink_scene_get_next_by_submenu_index(SubmenuIndex index) {
    switch(index) {
    case SubmenuIndexShow:
        return NfcEinkAppSceneShowImage;
    case SubmenuIndexSave:
        return NfcEinkAppSceneSaveName;
    case SubmenuIndexWrite:
        return NfcEinkAppSceneChooseScreen;
    case SubmenuIndexInfo:
        return NfcEinkAppSceneInfo;
    case SubmenuIndexDelete:
        return NfcEinkAppSceneDelete;
    default:
        FURI_LOG_E(TAG, "Unknown scene index %d", index);
        furi_crash();
    }
}

bool nfc_eink_scene_screen_menu_on_event(void* context, SceneManagerEvent event) {
    NfcEinkApp* instance = context;
    SceneManager* scene_manager = instance->scene_manager;

    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        const uint32_t submenu_index = event.event;
        NfcEinkAppScene scene = nfc_eink_scene_get_next_by_submenu_index(submenu_index);
        scene_manager_next_scene(scene_manager, scene);
        consumed = true;
    } else if(event.type == SceneManagerEventTypeBack) {
        if(scene_manager_has_previous_scene(instance->scene_manager, NfcEinkAppSceneEmulate))
            scene_manager_next_scene(instance->scene_manager, NfcEinkAppSceneExitConfirm);
        else
            scene_manager_previous_scene(instance->scene_manager);
        consumed = true;
    }

    return consumed;
}

void nfc_eink_scene_screen_menu_on_exit(void* context) {
    NfcEinkApp* instance = context;
    submenu_reset(instance->submenu);
}
