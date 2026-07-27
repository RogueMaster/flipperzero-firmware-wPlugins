#include "../nfc_magic_app_i.h"

void nfc_magic_scene_uscuid_ul_auth_fail_widget_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    NfcMagicApp* instance = context;
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(instance->view_dispatcher, result);
    }
}

void nfc_magic_scene_uscuid_ul_auth_fail_on_enter(void* context) {
    NfcMagicApp* instance = context;
    Widget* widget = instance->widget;

    notification_message(instance->notifications, &sequence_error);

    widget_add_icon_element(widget, 83, 22, &I_WarningDolphinFlip_45x42);
    widget_add_string_element(widget, 64, 0, AlignCenter, AlignTop, FontPrimary, "Auth Failed");
    // No retry on purpose: re-sending a wrong password burns the tag's AUTHLIM. The user
    // goes back to re-enter the password (or just re-present the card if it slipped).
    widget_add_string_multiline_element(
        widget,
        0,
        13,
        AlignLeft,
        AlignTop,
        FontSecondary,
        "Wrong password,\nor card moved.\nTag not modified.");
    widget_add_button_element(
        widget,
        GuiButtonTypeLeft,
        "Finish",
        nfc_magic_scene_uscuid_ul_auth_fail_widget_callback,
        instance);

    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcMagicAppViewWidget);
}

bool nfc_magic_scene_uscuid_ul_auth_fail_on_event(void* context, SceneManagerEvent event) {
    NfcMagicApp* instance = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeLeft) {
            consumed = scene_manager_search_and_switch_to_previous_scene(
                instance->scene_manager, NfcMagicSceneUscuidUlMenu);
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        consumed = scene_manager_search_and_switch_to_previous_scene(
            instance->scene_manager, NfcMagicSceneUscuidUlMenu);
    }
    return consumed;
}

void nfc_magic_scene_uscuid_ul_auth_fail_on_exit(void* context) {
    NfcMagicApp* instance = context;
    widget_reset(instance->widget);
}
