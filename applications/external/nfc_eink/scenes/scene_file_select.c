#include "../nfc_eink_app_i.h"

void nfc_eink_scene_file_select_on_enter(void* context) {
    NfcEinkApp* instance = context;

    NfcEinkLoadResult result = nfc_eink_load_from_file_select(instance);
    if(result == NfcEinkLoadResultSuccess) {
        scene_manager_next_scene(instance->scene_manager, NfcEinkAppSceneScreenMenu);
    } else if(result == NfcEinkLoadResultFailed) {
        instance->last_error = NfcEinkScreenErrorUnsupportedScreen;
        scene_manager_next_scene(instance->scene_manager, NfcEinkAppSceneError);
        notification_message(instance->notifications, &sequence_error);
    } else if(result == NfcEinkLoadResultCanceled) {
        scene_manager_previous_scene(instance->scene_manager);
    }
}

bool nfc_eink_scene_file_select_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    bool consumed = false;
    return consumed;
}

void nfc_eink_scene_file_select_on_exit(void* context) {
    UNUSED(context);
}
