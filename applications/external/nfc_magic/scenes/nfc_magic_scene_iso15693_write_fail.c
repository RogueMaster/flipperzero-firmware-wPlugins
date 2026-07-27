#include "../nfc_magic_app_i.h"

void nfc_magic_scene_iso15693_write_fail_widget_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    NfcMagicApp* instance = context;

    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(instance->view_dispatcher, result);
    }
}

void nfc_magic_scene_iso15693_write_fail_on_enter(void* context) {
    NfcMagicApp* instance = context;
    Widget* widget = instance->widget;

    const uint32_t reason =
        scene_manager_get_scene_state(instance->scene_manager, NfcMagicSceneIso15693WriteFail);
    const bool card_lost = (reason == NfcMagicIso15693WriteFailReasonCardLost);
    const bool partial = (reason == NfcMagicIso15693WriteFailReasonPartial);
    const bool over_capacity = (reason == NfcMagicIso15693WriteFailReasonOverCapacity);

    // Partial and over-capacity are soft (successful) outcomes; card-lost / not-magic are errors.
    notification_message(
        instance->notifications, (partial || over_capacity) ? &sequence_success : &sequence_error);

    if(over_capacity) {
        // The clone matched the source, but the card ended up advertising more blocks than it
        // physically holds (the extra source blocks were empty, so nothing was lost). Success, but
        // worth flagging: a reader that probes the top blocks sees them error/zero, and you can't
        // store real data there.
        const uint16_t advertised = instance->iso15693_clone_blocks_total;
        const uint16_t extra = instance->iso15693_clone_over_capacity;
        const uint16_t physical = (advertised > extra) ? (uint16_t)(advertised - extra) :
                                                         advertised;
        FuriString* text = furi_string_alloc();
        furi_string_cat_printf(
            text,
            "Copy matches source. Card holds %u of %u blocks; the top %u were empty. It now "
            "advertises more than it physically has.",
            physical,
            advertised,
            extra);
        widget_add_string_element(
            widget, 3, 0, AlignLeft, AlignTop, FontPrimary, "Clone complete");
        widget_add_text_box_element(
            widget, 0, 14, 128, 40, AlignLeft, AlignTop, furi_string_get_cstr(text), false);
        furi_string_free(text);
    } else if(partial) {
        // Full-width text box (no icon) so the detail can wrap. Partial means some NON-EMPTY source
        // blocks wouldn't write -- that data is past the card's real capacity (or the blocks are
        // locked), so it couldn't be cloned. Name the blocks. (Empty blocks that don't fit lose
        // nothing and never reach here -- they stay a clean Success.)
        FuriString* text = furi_string_alloc();
        furi_string_cat_str(
            text, instance->iso15693_is_wipe_mode ? "Wiped.\n" : "UID + data cloned.\n");
        if(instance->iso15693_clone_failed_count > 0) {
            furi_string_cat_printf(
                text,
                instance->iso15693_is_wipe_mode ? "%u block(s) wouldn't clear: " :
                                                  "%u block(s) didn't fit the card: ",
                instance->iso15693_clone_failed_count);
            uint16_t shown = 0;
            for(uint16_t block = 0; block < 256; block++) {
                if(instance->iso15693_clone_failed_bitmap[block / 8] & (1u << (block % 8))) {
                    if(shown >= 20) {
                        furi_string_cat_str(text, "...");
                        break;
                    }
                    furi_string_cat_printf(text, "%u ", block);
                    shown++;
                }
            }
            furi_string_push_back(text, '\n');
        }
        if(instance->iso15693_clone_used_gen1) {
            // The clone fell back to the gen1 method, which stamps the UID/commit into blocks
            // 56/57/62/63 -- so those no longer match the source. (gen1 is not hardware-validated.)
            furi_string_cat_str(
                text, "gen1 method: blocks 56/57/62/63 hold the UID, not your file's data.");
        }
        widget_add_string_element(
            widget,
            3,
            0,
            AlignLeft,
            AlignTop,
            FontPrimary,
            instance->iso15693_is_wipe_mode ? "Wipe partial" : "Clone partial");
        widget_add_text_box_element(
            widget, 0, 14, 128, 38, AlignLeft, AlignTop, furi_string_get_cstr(text), false);
        furi_string_free(text);
    } else {
        const char* message = card_lost ?
                                  "Card removed\nbefore the write\ncould finish." :
                                  "Not a magic tag.\nThis card doesn't\nsupport UID write.";
        widget_add_icon_element(widget, 83, 22, &I_WarningDolphinFlip_45x42);
        widget_add_string_element(
            widget, 64, 0, AlignCenter, AlignTop, FontPrimary, "Write failed");
        widget_add_string_multiline_element(
            widget, 0, 13, AlignLeft, AlignTop, FontSecondary, message);
    }

    // Only a lost card is worth retrying; a rejected/partial write would just repeat.
    if(card_lost) {
        widget_add_button_element(
            widget,
            GuiButtonTypeLeft,
            "Retry",
            nfc_magic_scene_iso15693_write_fail_widget_callback,
            instance);
    }
    widget_add_button_element(
        widget,
        GuiButtonTypeRight,
        "OK",
        nfc_magic_scene_iso15693_write_fail_widget_callback,
        instance);

    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcMagicAppViewWidget);
}

bool nfc_magic_scene_iso15693_write_fail_on_event(void* context, SceneManagerEvent event) {
    NfcMagicApp* instance = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeLeft) {
            // Retry: back to the write scene, which re-runs the write on enter.
            consumed = scene_manager_previous_scene(instance->scene_manager);
        } else if(event.event == GuiButtonTypeRight) {
            consumed = scene_manager_search_and_switch_to_previous_scene(
                instance->scene_manager, NfcMagicSceneIso15693);
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        consumed = scene_manager_search_and_switch_to_previous_scene(
            instance->scene_manager, NfcMagicSceneIso15693);
    }
    return consumed;
}

void nfc_magic_scene_iso15693_write_fail_on_exit(void* context) {
    NfcMagicApp* instance = context;

    // Reset to the default reason so a later failure isn't mislabelled.
    scene_manager_set_scene_state(
        instance->scene_manager,
        NfcMagicSceneIso15693WriteFail,
        NfcMagicIso15693WriteFailReasonNotMagic);
    widget_reset(instance->widget);
}
