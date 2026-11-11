#include "../subghz_i.h"
#include "../helpers/subghz_custom_event.h"

#include <lib/subghz/blocks/generic.h>
#include <lib/subghz/protocols/nice_flor_s.h>
#include <lib/subghz/protocols/public_api.h>

#define TAG "SubGhzSceneSignalInspector"

// Classify the decoder protocol type. A rolling-code label alone is not a
// security assessment of the implementation.
static const char* subghz_scene_signal_inspector_security_line(SubGhzProtocolType type) {
    switch(type) {
    case SubGhzProtocolTypeStatic:
        return "Code type: Fixed";
    case SubGhzProtocolTypeDynamic:
        return "Code type: Rolling";
    case SubGhzProtocolTypeRAW:
    case SubGhzProtocolTypeBinRAW:
        return "Code type: RAW capture";
    default:
        return "Code type: Unknown";
    }
}

// Soft-button callback: the Right key routes into the shared save flow. Mirrors
// how the receiver Info screen wires its own "Save" button.
static void
    subghz_scene_signal_inspector_callback(GuiButtonType result, InputType type, void* context) {
    furi_assert(context);
    SubGhz* subghz = context;
    if((result == GuiButtonTypeRight) && (type == InputTypeShort)) {
        view_dispatcher_send_custom_event(
            subghz->view_dispatcher, SubGhzCustomEventSceneSignalInspectorSave);
    }
}

// Read-only detail view for a captured Sub-GHz signal (issue #449).
// Shows the full metadata of the selected history record. Nothing is
// transmitted - all data comes from the history getters and from the
// decoder that has already parsed the captured signal.
void subghz_scene_signal_inspector_on_enter(void* context) {
    SubGhz* subghz = context;

    // The Widget is shared between scenes and still holds the Info screen
    // elements (Info is only suspended, not exited, when we open this scene).
    widget_reset(subghz->widget);

    FuriString* text = furi_string_alloc();
    bool show_save = false;

    // Reload the decoder for the currently selected record. This is the same
    // read-only helper the Info screen uses to render its widget.
    if(subghz_scene_receiver_info_update_parser(subghz)) {
        const char* protocol_name =
            subghz_history_get_protocol_name(subghz->history, subghz->idx_menu_chosen);
        uint32_t frequency =
            subghz_history_get_frequency(subghz->history, subghz->idx_menu_chosen);
        SubGhzRadioPreset* preset =
            subghz_history_get_radio_preset(subghz->history, subghz->idx_menu_chosen);
        uint16_t repeats = subghz_history_get_repeats(subghz->history, subghz->idx_menu_chosen);
        uint32_t hash = subghz_history_get_hash_data(subghz->history, subghz->idx_menu_chosen);
        DateTime datetime = subghz_history_get_datetime(subghz->history, subghz->idx_menu_chosen);

        // Fixed-vs-rolling classification, read from the decoder's protocol type.
        SubGhzProtocolDecoderBase* decoder = subghz_txrx_get_decoder(subghz->txrx);
        const char* security_line =
            subghz_scene_signal_inspector_security_line(decoder->protocol->type);

        // Header block: capture metadata that is not part of the decoded string.
        furi_string_printf(
            text,
            "\e#Signal Details\n"
            "Protocol: %s\n"
            "Frequency: %lu Hz\n"
            "Preset: %s\n"
            "Repeats: %d\n"
            "Hash: 0x%08lX\n"
            "Time: %04d-%02d-%02d\n"
            "      %02d:%02d:%02d\n"
            "%s\n"
            "\e#Decoded\n",
            protocol_name,
            frequency,
            furi_string_get_cstr(preset->name),
            repeats,
            hash,
            datetime.year,
            datetime.month,
            datetime.day,
            datetime.hour,
            datetime.minute,
            datetime.second,
            security_line);

        // Protocol-formatted payload (bit length, Key/data, serial, button,
        // counter, seed, ... exactly as each protocol exposes them).
        subghz_protocol_decoder_base_get_string(decoder, text);

        // Offer Save only for signals the existing save path can serialize.
        bool raw_only = (strcmp(decoder->protocol->name, SUBGHZ_PROTOCOL_NICE_FLOR_S_NAME) == 0) &&
                        subghz_protocol_decoder_nice_flor_s_is_o_code_no_ic(decoder);
        show_save = !raw_only && subghz_txrx_protocol_is_serializable(subghz->txrx);
    } else {
        furi_string_set(text, "Error history parse.");
    }

    // Leave a button bar at the bottom when the Save soft-button is shown.
    widget_add_text_scroll_element(
        subghz->widget, 0, 0, 128, show_save ? 52 : 64, furi_string_get_cstr(text));

    if(show_save) {
        widget_add_button_element(
            subghz->widget,
            GuiButtonTypeRight,
            "Save",
            subghz_scene_signal_inspector_callback,
            subghz);
    }

    furi_string_free(text);

    view_dispatcher_switch_to_view(subghz->view_dispatcher, SubGhzViewIdWidget);
}

bool subghz_scene_signal_inspector_on_event(void* context, SceneManagerEvent event) {
    SubGhz* subghz = context;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubGhzCustomEventSceneSignalInspectorSave) {
            // Reuse the receiver Info "Save" flow verbatim: stop the radio,
            // reload the decoder for the selected record, then hand off to the
            // shared SubGhzSceneSaveName scene that persists the .sub file via
            // subghz_save_protocol_to_file(). No new save logic is introduced.
            subghz->state_notifications = SubGhzNotificationStateIDLE;
            subghz_txrx_hopper_set_state(subghz->txrx, SubGhzHopperStateOFF);
            subghz_txrx_stop(subghz->txrx);

            if(!subghz_scene_receiver_info_update_parser(subghz)) {
                return false;
            }

            SubGhzProtocolDecoderBase* decoder = subghz_txrx_get_decoder(subghz->txrx);
            bool raw_only =
                (strcmp(decoder->protocol->name, SUBGHZ_PROTOCOL_NICE_FLOR_S_NAME) == 0) &&
                subghz_protocol_decoder_nice_flor_s_is_o_code_no_ic(decoder);
            if(!raw_only && subghz_txrx_protocol_is_serializable(subghz->txrx)) {
                subghz_file_name_clear(subghz);

                subghz->save_datetime =
                    subghz_history_get_datetime(subghz->history, subghz->idx_menu_chosen);
                subghz->save_datetime_set = true;
                scene_manager_next_scene(subghz->scene_manager, SubGhzSceneSaveName);
            }
            return true;
        }
    }
    // Read-only view otherwise: the Back key is handled by the global navigation
    // callback, which pops this scene and returns to the Info screen.
    return false;
}

void subghz_scene_signal_inspector_on_exit(void* context) {
    SubGhz* subghz = context;
    widget_reset(subghz->widget);
}
