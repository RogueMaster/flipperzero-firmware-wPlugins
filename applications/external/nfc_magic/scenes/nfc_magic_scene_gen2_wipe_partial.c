#include "../nfc_magic_app_i.h"

void nfc_magic_scene_gen2_wipe_partial_widget_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    NfcMagicApp* instance = context;
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(instance->view_dispatcher, result);
    }
}

void nfc_magic_scene_gen2_wipe_partial_on_enter(void* context) {
    NfcMagicApp* instance = context;
    Widget* widget = instance->widget;

    // Partial = every block was written/cleared except those that couldn't be (no key for the
    // sector, a read-only block, or a write that NAKed). Shared by the Gen2 wipe and clone flows;
    // the verb tracks which one ran.
    notification_message(instance->notifications, &sequence_success);

    const bool is_wipe = instance->gen2_poller_is_wipe_mode;
    widget_add_string_element(
        widget,
        64,
        0,
        AlignCenter,
        AlignTop,
        FontPrimary,
        is_wipe ? "Partial Wipe" : "Partial Write");

    FuriString* message = furi_string_alloc();
    furi_string_printf(
        message,
        "%s %u/%u blocks\nNot %s: %u",
        is_wipe ? "Wiped" : "Written",
        instance->gen2_partial_blocks_total - instance->gen2_partial_failed_count,
        instance->gen2_partial_blocks_total,
        is_wipe ? "wiped" : "written",
        instance->gen2_partial_failed_count);
    widget_add_string_multiline_element(
        widget, 4, 20, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(message));
    furi_string_free(message);

    widget_add_button_element(
        widget,
        GuiButtonTypeLeft,
        "Finish",
        nfc_magic_scene_gen2_wipe_partial_widget_callback,
        instance);
    widget_add_button_element(
        widget,
        GuiButtonTypeRight,
        "Details",
        nfc_magic_scene_gen2_wipe_partial_widget_callback,
        instance);

    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcMagicAppViewWidget);
}

bool nfc_magic_scene_gen2_wipe_partial_on_event(void* context, SceneManagerEvent event) {
    NfcMagicApp* instance = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeRight) {
            scene_manager_next_scene(instance->scene_manager, NfcMagicSceneGen2WipePartialDetails);
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

void nfc_magic_scene_gen2_wipe_partial_on_exit(void* context) {
    NfcMagicApp* instance = context;
    widget_reset(instance->widget);
}
