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

// "Ultralight"/"Classic" family of a magic-card protocol; NULL for Gen4 (universal).
static const char* nfc_magic_wrong_card_target_family(NfcMagicProtocol protocol) {
    if(protocol == NfcMagicProtocolUscuidUl || protocol == NfcMagicProtocolUscuidUlNotDetected) {
        return "Ultralight";
    }
    if(protocol == NfcMagicProtocolGen1 || protocol == NfcMagicProtocolGen2 ||
       protocol == NfcMagicProtocolClassic) {
        return "Classic";
    }
    return NULL;
}

// "Ultralight"/"Classic" family of a source dump; NULL for other protocols.
static const char* nfc_magic_wrong_card_dump_family(NfcProtocol protocol) {
    if(protocol == NfcProtocolMfUltralight) return "Ultralight";
    if(protocol == NfcProtocolMfClassic) return "Classic";
    return NULL;
}

void nfc_magic_scene_wrong_card_on_enter(void* context) {
    NfcMagicApp* instance = context;
    Widget* widget = instance->widget;

    notification_message(instance->notifications, &sequence_error);

    const char* card_family = nfc_magic_wrong_card_target_family(instance->protocol);
    const char* dump_family =
        nfc_magic_wrong_card_dump_family(nfc_device_get_protocol(instance->source_dev));

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
    } else if(
        instance->protocol == NfcMagicProtocolUscuidUl && instance->uscuid_ul_data.type_known &&
        nfc_device_get_protocol(instance->source_dev) == NfcProtocolMfUltralight) {
        // Name the base type that governs matching (a UL21 card takes any UL21 dump, Ultra
        // or not) so we don't send the user hunting for an over-specific "(Ultra)" dump.
        const MfUltralightData* mfu_data =
            nfc_device_get_data(instance->source_dev, NfcProtocolMfUltralight);
        FuriString* message = furi_string_alloc();
        furi_string_printf(
            message,
            "%s magic cards\ndon't accept\n%s dumps.",
            uscuid_ul_type_name(instance->uscuid_ul_data.type),
            uscuid_ul_type_name(mfu_data->type));

        widget_add_string_element(widget, 1, 4, AlignLeft, AlignTop, FontPrimary, "Type Mismatch");
        widget_add_string_multiline_element(
            widget, 1, 17, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(message));

        furi_string_free(message);
    } else if(card_family && dump_family && strcmp(card_family, dump_family) != 0) {
        // Cross-family: e.g. a Mifare Classic dump onto an Ultralight magic card.
        FuriString* message = furi_string_alloc();
        furi_string_printf(
            message, "%s magic cards\ndon't accept\n%s dumps.", card_family, dump_family);

        widget_add_string_element(widget, 1, 4, AlignLeft, AlignTop, FontPrimary, "Type Mismatch");
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
