#include "../nfc_magic_app_i.h"

void nfc_magic_scene_write_confirm_widget_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    NfcMagicApp* instance = context;

    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(instance->view_dispatcher, result);
    }
}

void nfc_magic_scene_write_confirm_on_enter(void* context) {
    NfcMagicApp* instance = context;
    Widget* widget = instance->widget;

    const bool iso15693_wipe =
        (instance->protocol == NfcMagicProtocolIso15693) && instance->iso15693_is_wipe_mode;
    const bool is_wipe = instance->uscuid_ul_is_wipe_mode || iso15693_wipe;

    const char* text;
    if(iso15693_wipe) {
        text = "Zeroes every data block on this card. The UID is left unchanged.";
    } else if(instance->uscuid_ul_is_wipe_mode) {
        text = "Blank factory dump: config &\npassword cleared, UID zeroed.";
    } else if(
        instance->protocol == NfcMagicProtocolIso15693 &&
        iso15693_poller_source_uses_gen1_blocks(
            nfc_device_get_data(instance->source_dev, NfcProtocolIso15693_3))) {
        // The source stores data in the gen1 backdoor blocks (56/57/62/63). A gen1 card would have
        // those overwritten with the UID, so warn specifically before the write.
        text =
            "File uses blocks 56/57/62/63. If this card is gen1 (not gen2), gen1 puts the UID in those blocks, so they can't be cloned.";
    } else {
        text =
            "Writing to this card will change manufacturer block. On some cards it may not be rewritten";
    }

    widget_add_string_element(
        widget, 3, 0, AlignLeft, AlignTop, FontPrimary, is_wipe ? "Wipe card?" : "Risky operation");
    widget_add_text_box_element(widget, 0, 13, 128, 54, AlignLeft, AlignTop, text, false);
    widget_add_button_element(
        widget,
        GuiButtonTypeCenter,
        "Continue",
        nfc_magic_scene_write_confirm_widget_callback,
        instance);
    widget_add_button_element(
        widget, GuiButtonTypeLeft, "Back", nfc_magic_scene_write_confirm_widget_callback, instance);

    // Setup and start worker
    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcMagicAppViewWidget);
}

bool nfc_magic_scene_write_confirm_on_event(void* context, SceneManagerEvent event) {
    NfcMagicApp* instance = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeLeft) {
            consumed = scene_manager_previous_scene(instance->scene_manager);
        } else if(event.event == GuiButtonTypeCenter) {
            scene_manager_next_scene(instance->scene_manager, NfcMagicSceneWrite);
            consumed = true;
        }
    }
    return consumed;
}

void nfc_magic_scene_write_confirm_on_exit(void* context) {
    NfcMagicApp* instance = context;

    widget_reset(instance->widget);
}
