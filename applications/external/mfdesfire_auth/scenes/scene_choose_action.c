#include "mfdesfire_auth_i.h"

// Enum pro submenu položky
typedef enum {
    MfDesSubmenuSetIV,
    MfDesSubmenuSetKey,
    MfDesSubmenuAuthenticate,
} MfDesSubmenuIndex;

static void mfdes_submenu_callback(void* context, uint32_t index) {
    MfDesApp* instance = context;
    view_dispatcher_send_custom_event(instance->view_dispatcher, index);
}

void desfire_app_scene_choose_action_on_enter(void* context) {
    MfDesApp* instance = context;
    Submenu* submenu = instance->submenu;

    // Nastavení Submenu
    submenu_add_item(
        submenu, "Set Initial Vector", MfDesSubmenuSetIV, mfdes_submenu_callback, instance);
    submenu_add_item(submenu, "Set Key", MfDesSubmenuSetKey, mfdes_submenu_callback, instance);
    submenu_add_item(
        submenu,
        "Legacy Authetication",
        MfDesSubmenuAuthenticate,
        mfdes_submenu_callback,
        instance);

    view_dispatcher_switch_to_view(instance->view_dispatcher, MfDesAppViewSubmenu);
}

bool desfire_app_scene_choose_action_on_event(void* context, SceneManagerEvent event) {
    MfDesApp* instance = context;
    SceneManager* scene_manager = instance->scene_manager;

    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        const uint32_t event_index = event.event;
        if(event_index == MfDesSubmenuSetIV) {
            scene_manager_next_scene(scene_manager, DesfireAppSceneSetVector);
            consumed = true;
        } else if(event_index == MfDesSubmenuSetKey) {
            scene_manager_next_scene(scene_manager, DesfireAppSceneSetKey);
            consumed = true;

        } else if(event_index == MfDesSubmenuAuthenticate) {
            scene_manager_next_scene(scene_manager, DesfireAppSceneEmulate);
            consumed = true;
        }
    }

    return consumed;
}

void desfire_app_scene_choose_action_on_exit(void* context) {
    MfDesApp* instance = context;
    submenu_reset(instance->submenu);
}
