#include "../nfc_magic_app_i.h"

void nfc_magic_scene_wrong_card_widget_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    NfcMagicApp* instance = context;
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(instance->view_dispatcher, result);
    }
}

void nfc_magic_scene_wrong_card_on_enter(void* context) {
    NfcMagicApp* instance = context;
    Widget* widget = instance->widget;

    notification_message(instance->notifications, &sequence_error);

    if(instance->source_uid_mismatch) {
        size_t source_uid_len = 0;
        nfc_device_get_uid(instance->source_dev, &source_uid_len);

        FuriString* message = furi_string_alloc();
        furi_string_printf(
            message,
            "Tag UID: %u bytes\nFile UID: %u bytes\nLengths must match.",
            (unsigned)instance->gen1_uid_len,
            (unsigned)source_uid_len);

        widget_add_string_element(
            widget, 1, 4, AlignLeft, AlignTop, FontPrimary, "UID Length Mismatch");
        widget_add_string_multiline_element(
            widget, 1, 17, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(message));

        furi_string_free(message);
    } else {
        widget_add_icon_element(widget, 73, 17, &I_DolphinCommon_56x48);
        widget_add_string_element(
            widget, 1, 4, AlignLeft, AlignTop, FontPrimary, "This is wrong card");
        widget_add_string_multiline_element(
            widget,
            1,
            17,
            AlignLeft,
            AlignTop,
            FontSecondary,
            "Writing this file is\nnot supported for\nthis magic card.");
    }
    widget_add_button_element(
        widget, GuiButtonTypeLeft, "Retry", nfc_magic_scene_wrong_card_widget_callback, instance);

    // Setup and start worker
    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcMagicAppViewWidget);
}

bool nfc_magic_scene_wrong_card_on_event(void* context, SceneManagerEvent event) {
    NfcMagicApp* instance = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeLeft) {
            consumed = scene_manager_previous_scene(instance->scene_manager);
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        consumed = scene_manager_search_and_switch_to_previous_scene(
            instance->scene_manager, NfcMagicSceneStart);
    }
    return consumed;
}

void nfc_magic_scene_wrong_card_on_exit(void* context) {
    NfcMagicApp* instance = context;

    widget_reset(instance->widget);
}
