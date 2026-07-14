#include "mfdesfire_auth_i.h"

static void mfdes_popup_callback(void* context) {
    MfDesApp* instance = context;
    view_dispatcher_send_custom_event(instance->view_dispatcher, MfDesAppCustomExit);
}

void desfire_app_scene_emulate_on_enter(void* context) {
    MfDesApp* instance = context;
    Popup* popup = instance->popup;

    popup_set_header(popup, "Emulating...", 64, 20, AlignCenter, AlignCenter);
    popup_set_text(popup, "Please wait", 64, 40, AlignCenter, AlignCenter);
    popup_set_icon(popup, 0, 0, NULL); // No icon
    popup_set_timeout(popup, 3000); // 3 seconds timeout
    popup_set_context(popup, instance);
    popup_set_callback(popup, mfdes_popup_callback);
    popup_enable_timeout(popup);

    view_dispatcher_switch_to_view(instance->view_dispatcher, MfDesAppViewPopupAuth);
}

bool desfire_app_scene_emulate_on_event(void* context, SceneManagerEvent event) {
    MfDesApp* instance = context;
    UNUSED(event);
    SceneManager* scene_manager = instance->scene_manager;

    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        uint32_t event_index = event.event;
        if(event_index == MfDesAppCustomExit) {
            consumed = scene_manager_search_and_switch_to_previous_scene(
                scene_manager, MfDesAppViewSubmenu);
        }
    }

    return consumed;
}

void desfire_app_scene_emulate_on_exit(void* context) {
    MfDesApp* instance = context;

    // Clear view
    popup_reset(instance->popup);
}
