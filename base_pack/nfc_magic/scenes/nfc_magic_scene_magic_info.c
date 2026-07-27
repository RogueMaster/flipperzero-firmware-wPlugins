#include "../nfc_magic_app_i.h"
#include "magic/nfc_magic_scanner.h"

void nfc_magic_scene_magic_info_widget_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    NfcMagicApp* instance = context;

    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(instance->view_dispatcher, result);
    }
}

void nfc_magic_scene_magic_info_on_enter(void* context) {
    NfcMagicApp* instance = context;
    Widget* widget = instance->widget;

    notification_message(instance->notifications, &sequence_success);

    FuriString* message = furi_string_alloc();

    if(instance->protocol == NfcMagicProtocolClassic) {
        widget_add_string_element(
            widget, 0, 0, AlignLeft, AlignTop, FontPrimary, "Magic Not Confirmed");
        // Hand-wrapped: the text box breaks mid-word, so keep each line short.
        furi_string_printf(
            message,
            "Not a magic tag, or\nsector 0 key is non-standard.\nTry writing to confirm.");
    } else if(instance->protocol == NfcMagicProtocolUscuidUlNotDetected) {
        widget_add_string_element(
            widget, 0, 0, AlignLeft, AlignTop, FontPrimary, "Magic Not Detected");
        // Hand-wrapped: the text box breaks mid-word, so keep each line short.
        furi_string_printf(
            message, "Not a magic Ultralight,\nor an unsupported type.\nTry writing to confirm.");
    } else if(instance->protocol == NfcMagicProtocolIso15693) {
        // Any ISO15693 tag reads here; magic-ness is only proven at write time.
        widget_add_string_element(
            widget, 0, 0, AlignLeft, AlignTop, FontPrimary, "ISO15693 / NfcV");
        furi_string_printf(message, "Magic candidate.\nConfirmed when you\nwrite to it.");
    } else {
        widget_add_string_element(
            widget, 0, 0, AlignLeft, AlignTop, FontPrimary, "Magic card detected!");
        furi_string_printf(
            message, "Magic Type: %s", nfc_magic_protocols_get_name(instance->protocol));

        const char* detail = NULL;
        if(instance->protocol == NfcMagicProtocolGen2) {
            detail = gen2_type_get_detail(instance->gen2_type);
        } else if(instance->protocol == NfcMagicProtocolGen1) {
            if(instance->gen1_uid_len == 4) {
                detail = "4-byte UID";
            } else if(instance->gen1_uid_len == 7) {
                detail = "7-byte UID";
            }
        } else if(instance->protocol == NfcMagicProtocolUscuidUl) {
            detail = uscuid_ul_get_variant_name(&instance->uscuid_ul_data);
        }
        if(detail) {
            furi_string_cat_printf(message, "\n%s", detail);
        }
        // Detection route on its own line for a confirmed USCUID-UL.
        if(instance->protocol == NfcMagicProtocolUscuidUl &&
           instance->uscuid_ul_data.is_uscuid_ul) {
            const char* detection = "ATS";
            if(instance->uscuid_ul_data.wakeup == UscuidUlWakeupA) {
                detection = "Backdoor (0x40)";
            } else if(instance->uscuid_ul_data.wakeup == UscuidUlWakeupB) {
                detection = "Backdoor (0x20)";
            }
            furi_string_cat_printf(message, "\nDetection: %s", detection);
        }
    }
    widget_add_text_box_element(
        widget, 0, 10, 128, 54, AlignLeft, AlignTop, furi_string_get_cstr(message), false);

    if(instance->protocol == NfcMagicProtocolGen4) {
        gen4_copy(instance->gen4_data, nfc_magic_scanner_get_gen4_data(instance->scanner));

        furi_string_printf(
            message,
            "Revision: %02X %02X\n",
            instance->gen4_data->revision.data[3],
            instance->gen4_data->revision.data[4]);

        widget_add_string_element(
            widget, 0, 20, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(message));

        furi_string_printf(
            message,
            "Configured As:\n%s",
            gen4_get_configuration_name(&instance->gen4_data->config));

        widget_add_string_multiline_element(
            widget, 0, 30, AlignLeft, AlignTop, FontSecondary, furi_string_get_cstr(message));
    }

    widget_add_button_element(
        widget, GuiButtonTypeLeft, "Retry", nfc_magic_scene_magic_info_widget_callback, instance);

    // "More" -> Write menu (writable), or the raw-config view (confirmed but unrecognised
    // preset). UL-C and not-confirmed hide it. NotDetected keeps the default (write-anyway).
    bool show_more = true;
    if(instance->protocol == NfcMagicProtocolUscuidUl) {
        show_more =
            uscuid_ul_data_is_writable(&instance->uscuid_ul_data) ||
            (instance->uscuid_ul_data.is_uscuid_ul && !instance->uscuid_ul_data.type_known);
    }
    if(show_more) {
        widget_add_button_element(
            widget,
            GuiButtonTypeRight,
            "More",
            nfc_magic_scene_magic_info_widget_callback,
            instance);
    }

    furi_string_free(message);

    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcMagicAppViewWidget);
}

bool nfc_magic_scene_magic_info_on_event(void* context, SceneManagerEvent event) {
    NfcMagicApp* instance = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeLeft) {
            consumed = scene_manager_previous_scene(instance->scene_manager);
        } else if(event.event == GuiButtonTypeRight) {
            if(instance->protocol == NfcMagicProtocolGen1) {
                scene_manager_next_scene(instance->scene_manager, NfcMagicSceneGen1Menu);
                consumed = true;
            } else if(instance->protocol == NfcMagicProtocolGen4) {
                scene_manager_next_scene(instance->scene_manager, NfcMagicSceneGen4Menu);
                consumed = true;
            } else if(instance->protocol == NfcMagicProtocolGen2) {
                scene_manager_next_scene(instance->scene_manager, NfcMagicSceneGen2Menu);
                consumed = true;
            } else if(instance->protocol == NfcMagicProtocolUscuidUl) {
                if(uscuid_ul_data_is_writable(&instance->uscuid_ul_data)) {
                    scene_manager_next_scene(instance->scene_manager, NfcMagicSceneUscuidUlMenu);
                } else {
                    // Confirmed but unrecognised preset -> show the raw config.
                    scene_manager_next_scene(instance->scene_manager, NfcMagicSceneUscuidUlCfg);
                }
                consumed = true;
            } else if(instance->protocol == NfcMagicProtocolUscuidUlNotDetected) {
                scene_manager_next_scene(instance->scene_manager, NfcMagicSceneUscuidUlMenu);
                consumed = true;
            } else if(instance->protocol == NfcMagicProtocolClassic) {
                scene_manager_next_scene(instance->scene_manager, NfcMagicSceneMfClassicMenu);
                consumed = true;
            } else if(instance->protocol == NfcMagicProtocolIso15693) {
                scene_manager_next_scene(instance->scene_manager, NfcMagicSceneIso15693);
                consumed = true;
            }
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        consumed = scene_manager_search_and_switch_to_previous_scene(
            instance->scene_manager, NfcMagicSceneStart);
    }
    return consumed;
}

void nfc_magic_scene_magic_info_on_exit(void* context) {
    NfcMagicApp* instance = context;

    widget_reset(instance->widget);
}
