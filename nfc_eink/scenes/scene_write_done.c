#include "../nfc_eink_app_i.h"

void nfc_eink_scene_write_done_popup_callback(void* context) {
    NfcEinkApp* instance = context;
    view_dispatcher_send_custom_event(
        instance->view_dispatcher, NfcEinkAppCustomEventTimerExpired);
}

void nfc_eink_scene_write_done_on_enter(void* context) {
    NfcEinkApp* instance = context;

    // Setup view
    Popup* popup = instance->popup;
    popup_set_icon(popup, 48, 6, &I_DolphinDone_80x58);
    popup_set_header(popup, "Done!", 5, 22, AlignLeft, AlignBottom);
    popup_set_timeout(popup, 1500);
    popup_set_context(popup, instance);
    popup_set_callback(popup, nfc_eink_scene_write_done_popup_callback);
    popup_enable_timeout(popup);
    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcEinkViewPopup);
}

bool nfc_eink_scene_write_done_on_event(void* context, SceneManagerEvent event) {
    NfcEinkApp* instance = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == NfcEinkAppCustomEventTimerExpired) {
            consumed = scene_manager_search_and_switch_to_previous_scene(
                instance->scene_manager, NfcEinkAppSceneChooseScreen);
        }
    }
    return consumed;
}

void nfc_eink_scene_write_done_on_exit(void* context) {
    NfcEinkApp* instance = context;
    // Clear view
    popup_reset(instance->popup);
}
