#include "mfdesfire_auth_i.h"

static void mfdesapp_byte_input_key_callback(void* context) {
    furi_assert(context);
    MfDesApp* instance = context;

    // mfdesfire_auth_byte_input_callback(instance, MfDesAppByteInputIV);
    view_dispatcher_send_custom_event(instance->view_dispatcher, MfDesAppByteInputKey);
}

void desfire_app_scene_set_key_on_enter(void* context) {
    MfDesApp* instance = context;

    byte_input_set_header_text(instance->byte_input, "Set Key:");
    byte_input_set_result_callback( // Tohle je callback na to kdyz dam save. Kdyz klikam zpet tak se vola jiny.
        instance->byte_input,
        mfdesapp_byte_input_key_callback,
        NULL,
        instance,
        instance->key,
        KEY_SIZE);

    view_dispatcher_switch_to_view(instance->view_dispatcher, MfDesAppViewByteInput);
}

bool desfire_app_scene_set_key_on_event(void* context, SceneManagerEvent event) {
    MfDesApp* instance = context;
    SceneManager* scene_manager = instance->scene_manager;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        const uint32_t event_index = event.event;
        if(event_index == MfDesAppByteInputKey) {
            mfdesfire_auth_save_settings(instance, MfDesAppByteInputKey);
            // scene_manager_next_scene(scene_manager, MfDesAppViewSubmenu);
            consumed = scene_manager_search_and_switch_to_previous_scene(
                scene_manager, MfDesAppViewSubmenu);
            // consumed = true;
        }
    }

    return consumed;
}

void desfire_app_scene_set_key_on_exit(void* context) {
    UNUSED(context);
}
