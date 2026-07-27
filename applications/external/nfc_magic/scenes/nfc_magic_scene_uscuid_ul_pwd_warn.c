#include "../nfc_magic_app_i.h"

void nfc_magic_scene_uscuid_ul_pwd_warn_widget_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    NfcMagicApp* instance = context;
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(instance->view_dispatcher, result);
    }
}

void nfc_magic_scene_uscuid_ul_pwd_warn_on_enter(void* context) {
    NfcMagicApp* instance = context;
    Widget* widget = instance->widget;

    notification_message(instance->notifications, &sequence_blink_start_yellow);

    widget_add_string_element(widget, 64, 0, AlignCenter, AlignTop, FontPrimary, "Risky Action!");

    // 3 lines max: a 4th collides with the bottom buttons on the 64px screen.
    const uint8_t* pwd = instance->byte_input_store;
    snprintf(
        instance->text_store,
        sizeof(instance->text_store),
        "PWD: %02X %02X %02X %02X\nWrong password can\nblock the tag!",
        pwd[0],
        pwd[1],
        pwd[2],
        pwd[3]);
    widget_add_string_multiline_element(
        widget, 0, 15, AlignLeft, AlignTop, FontSecondary, instance->text_store);

    widget_add_button_element(
        widget,
        GuiButtonTypeLeft,
        "Cancel",
        nfc_magic_scene_uscuid_ul_pwd_warn_widget_callback,
        instance);
    widget_add_button_element(
        widget,
        GuiButtonTypeRight,
        "Proceed",
        nfc_magic_scene_uscuid_ul_pwd_warn_widget_callback,
        instance);

    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcMagicAppViewWidget);
}

bool nfc_magic_scene_uscuid_ul_pwd_warn_on_event(void* context, SceneManagerEvent event) {
    NfcMagicApp* instance = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeRight) {
            // Arm auth: commit the captured password.
            memcpy(instance->uscuid_ul_password, instance->byte_input_store, USCUID_UL_PWD_SIZE);
            instance->uscuid_ul_password_set = true;
            consumed = scene_manager_search_and_switch_to_previous_scene(
                instance->scene_manager, NfcMagicSceneUscuidUlMenu);
        } else if(event.event == GuiButtonTypeLeft) {
            consumed = scene_manager_search_and_switch_to_previous_scene(
                instance->scene_manager, NfcMagicSceneUscuidUlMenu);
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        // Back returns to the editor to fix the password (Cancel discards to the menu).
        consumed = scene_manager_previous_scene(instance->scene_manager);
    }
    return consumed;
}

void nfc_magic_scene_uscuid_ul_pwd_warn_on_exit(void* context) {
    NfcMagicApp* instance = context;
    notification_message(instance->notifications, &sequence_blink_stop);
    widget_reset(instance->widget);
}
