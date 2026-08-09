#include "../nfc_magic_app_i.h"

void nfc_magic_scene_wipe_fail_widget_callback(GuiButtonType result, InputType type, void* context) {
    NfcMagicApp* instance = context;

    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(instance->view_dispatcher, result);
    }
}

void nfc_magic_scene_wipe_fail_on_enter(void* context) {
    NfcMagicApp* instance = context;

    Widget* widget = instance->widget;
    notification_message(instance->notifications, &sequence_error);

    uint32_t reason =
        scene_manager_get_scene_state(instance->scene_manager, NfcMagicSceneWipeFail);
    const char* message = (reason == NfcMagicWipeFailReasonNoKeys) ?
                              "No keys found,\nnothing to wipe" :
                              "Something went\nwrong while wiping";

    widget_add_icon_element(widget, 83, 22, &I_WarningDolphinFlip_45x42);
    widget_add_string_element(widget, 64, 0, AlignCenter, AlignTop, FontPrimary, "Failed to Wipe");
    widget_add_string_multiline_element(
        widget, 0, 13, AlignLeft, AlignTop, FontSecondary, message);

    widget_add_button_element(
        widget, GuiButtonTypeLeft, "Retry", nfc_magic_scene_wipe_fail_widget_callback, instance);
    widget_add_button_element(
        widget, GuiButtonTypeRight, "Exit", nfc_magic_scene_wipe_fail_widget_callback, instance);

    // Setup and start worker
    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcMagicAppViewWidget);
}

bool nfc_magic_scene_wipe_fail_on_event(void* context, SceneManagerEvent event) {
    NfcMagicApp* instance = context;

    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeLeft) {
            consumed = scene_manager_previous_scene(instance->scene_manager);
        } else if(event.event == GuiButtonTypeRight) {
            consumed = scene_manager_search_and_switch_to_previous_scene(
                instance->scene_manager, NfcMagicSceneStart);
        }
    }
    return consumed;
}

void nfc_magic_scene_wipe_fail_on_exit(void* context) {
    NfcMagicApp* instance = context;

    // Back to the default so a later mid-wipe failure isn't mislabeled "no keys".
    scene_manager_set_scene_state(
        instance->scene_manager, NfcMagicSceneWipeFail, NfcMagicWipeFailReasonGeneric);
    widget_reset(instance->widget);
}
