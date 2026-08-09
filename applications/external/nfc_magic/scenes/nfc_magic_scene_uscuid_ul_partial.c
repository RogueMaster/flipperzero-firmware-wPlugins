#include "../nfc_magic_app_i.h"

void nfc_magic_scene_uscuid_ul_partial_widget_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    NfcMagicApp* instance = context;
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(instance->view_dispatcher, result);
    }
}

void nfc_magic_scene_uscuid_ul_partial_on_enter(void* context) {
    NfcMagicApp* instance = context;
    Widget* widget = instance->widget;

    // Partial = some pages wrote, at least one didn't (the failed list may include the UID, since
    // it's written last) -> still a qualified success.
    notification_message(instance->notifications, &sequence_success);

    widget_add_string_element(widget, 64, 0, AlignCenter, AlignTop, FontPrimary, "Partial Write");

    FuriString* message = furi_string_alloc();
    furi_string_printf(
        message,
        "Written: %u/%u\nNot written: %u",
        instance->write_progress_current,
        instance->write_progress_total,
        instance->write_failed_count);
    widget_add_string_multiline_element(
        widget, 4, 20, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(message));
    furi_string_free(message);

    widget_add_button_element(
        widget,
        GuiButtonTypeLeft,
        "Finish",
        nfc_magic_scene_uscuid_ul_partial_widget_callback,
        instance);
    widget_add_button_element(
        widget,
        GuiButtonTypeRight,
        "Details",
        nfc_magic_scene_uscuid_ul_partial_widget_callback,
        instance);

    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcMagicAppViewWidget);
}

bool nfc_magic_scene_uscuid_ul_partial_on_event(void* context, SceneManagerEvent event) {
    NfcMagicApp* instance = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeRight) {
            scene_manager_next_scene(instance->scene_manager, NfcMagicSceneUscuidUlPartialDetails);
            consumed = true;
        } else if(event.event == GuiButtonTypeLeft) {
            consumed = scene_manager_search_and_switch_to_previous_scene(
                instance->scene_manager, NfcMagicSceneStart);
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        consumed = scene_manager_search_and_switch_to_previous_scene(
            instance->scene_manager, NfcMagicSceneStart);
    }
    return consumed;
}

void nfc_magic_scene_uscuid_ul_partial_on_exit(void* context) {
    NfcMagicApp* instance = context;
    widget_reset(instance->widget);
}
