#include "../chameleon_app_i.h"

static void chameleon_scene_slot_list_submenu_callback(void* context, uint32_t index) {
    ChameleonApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void chameleon_scene_slot_list_on_enter(void* context) {
    ChameleonApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);

    // Get slot information from device
    chameleon_app_get_slots_info(app);

    // Add slots to submenu
    for(uint8_t i = 0; i < 8; i++) {
        char slot_label[64];
        if(strlen(app->slots[i].nickname) > 0) {
            snprintf(slot_label, sizeof(slot_label), "Slot %d: %s", i, app->slots[i].nickname);
        } else {
            snprintf(slot_label, sizeof(slot_label), "Slot %d (Empty)", i);
        }
        submenu_add_item(submenu, slot_label, i, chameleon_scene_slot_list_submenu_callback, app);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, ChameleonViewSubmenu);
}

bool chameleon_scene_slot_list_on_event(void* context, SceneManagerEvent event) {
    ChameleonApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        app->active_slot = (uint8_t)event.event;
        scene_manager_next_scene(app->scene_manager, ChameleonSceneSlotConfig);
        consumed = true;
    }

    return consumed;
}

void chameleon_scene_slot_list_on_exit(void* context) {
    ChameleonApp* app = context;
    submenu_reset(app->submenu);
}
