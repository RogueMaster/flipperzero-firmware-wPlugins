#include "../seader_i.h"
#include "../credential_sio_label.h"
#include <dolphin/dolphin.h>

#define TAG "SeaderSceneReadCardSuccess"

static bool seader_credential_is_picopass_sio_context(const SeaderCredential* credential) {
    return credential && (credential->type == SeaderCredentialTypePicopass ||
                          (credential->has_pacs_media_type &&
                           credential->pacs_media_type == SeaderPacsMediaTypePicopass));
}

void seader_scene_read_card_success_widget_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    furi_assert(context);
    Seader* seader = context;

    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(seader->view_dispatcher, result);
    }
}

void seader_scene_read_card_success_on_enter(void* context) {
    Seader* seader = context;
    SeaderCredential* credential = seader->credential;
    PluginWiegand* plugin = seader_wiegand_plugin_acquire(seader) ? seader->plugin_wiegand : NULL;
    Widget* widget = seader_get_widget(seader);
    if(!widget) {
        FURI_LOG_E(TAG, "Widget view unavailable");
        return;
    }

    if(!seader_temp_strings_ensure(seader, 4U)) {
        FURI_LOG_E(TAG, "Temp string allocation failed");
        seader_wiegand_plugin_release(seader);
        return;
    }
    FuriString* type_str = seader->temp_string1;
    FuriString* bitlength_str = seader->temp_string2;
    FuriString* credential_str = seader->temp_string3;
    FuriString* sio_str = seader->temp_string4;
    char sio_label[SEADER_TEXT_STORE_SIZE + 1] = {0};

    dolphin_deed(DolphinDeedNfcReadSuccess);

    // Send notification
    notification_message(seader->notifications, &sequence_success);

    furi_string_set(credential_str, "");
    furi_string_set(bitlength_str, "");
    furi_string_set(sio_str, "");
    if(credential->bit_length > 0) {
        furi_string_cat_printf(bitlength_str, "%d bit", credential->bit_length);
        furi_string_cat_printf(credential_str, "0x%llX", credential->credential);
        furi_string_set(type_str, seader_credential_get_type_label(credential));
    } else {
        furi_string_set(type_str, "Read error");
        furi_string_set(bitlength_str, seader->read_error[0] ? seader->read_error : "Read failed");

        seader_t_1_reset(seader->uart);
        seader_ccid_check_for_sam(seader->uart);
    }

    widget_add_button_element(
        widget, GuiButtonTypeLeft, "Retry", seader_scene_read_card_success_widget_callback, seader);

    if(credential->bit_length > 0) {
        widget_add_button_element(
            widget,
            GuiButtonTypeRight,
            "More",
            seader_scene_read_card_success_widget_callback,
            seader);
    } else {
        widget_add_button_element(
            widget,
            GuiButtonTypeRight,
            "Back",
            seader_scene_read_card_success_widget_callback,
            seader);
    }

    if(credential->bit_length > 0) {
        if(plugin) {
            size_t format_count = plugin->count(credential->bit_length, credential->credential);
            FURI_LOG_D(
                TAG,
                "Plugin present, bit_length=%d, format_count=%zu",
                credential->bit_length,
                format_count);
        } else {
            FURI_LOG_D(
                TAG, "Parse available without plugin bit_length=%d", credential->bit_length);
        }
        widget_add_button_element(
            seader->widget,
            GuiButtonTypeCenter,
            "Parse",
            seader_scene_read_card_success_widget_callback,
            seader);
    } else if(!plugin) {
        FURI_LOG_D(TAG, "Plugin=%p, bit_length=%d", plugin, credential->bit_length);
    }

    widget_add_string_element(
        widget, 64, 5, AlignCenter, AlignCenter, FontPrimary, furi_string_get_cstr(type_str));
    widget_add_string_element(
        widget,
        64,
        24,
        AlignCenter,
        AlignCenter,
        FontSecondary,
        furi_string_get_cstr(bitlength_str));
    widget_add_string_element(
        widget,
        64,
        36,
        AlignCenter,
        AlignCenter,
        FontSecondary,
        furi_string_get_cstr(credential_str));
    if(seader_sio_label_format(
           credential->sio[0] == 0x30,
           seader_credential_is_picopass_sio_context(credential),
           credential->sio_start_block,
           sio_label,
           sizeof(sio_label))) {
        if(strcmp(sio_label, "+SIO(?)") == 0) {
            FURI_LOG_E(TAG, "Unknown SIO start block: %d", credential->sio_start_block);
        }
        furi_string_set(sio_str, sio_label);
        widget_add_string_element(
            widget, 64, 48, AlignCenter, AlignCenter, FontSecondary, furi_string_get_cstr(sio_str));
    }

    // No need to free strings as they are reused from seader struct

    view_dispatcher_switch_to_view(seader->view_dispatcher, SeaderViewWidget);
}

bool seader_scene_read_card_success_on_event(void* context, SceneManagerEvent event) {
    Seader* seader = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeLeft) {
            consumed = seader_hf_request_teardown(seader, SeaderHfTeardownActionRestartRead);
        } else if(event.event == GuiButtonTypeRight) {
            if(seader->credential->bit_length > 0) {
                scene_manager_next_scene(seader->scene_manager, SeaderSceneCardMenu);
            } else {
                consumed = seader_hf_request_teardown(seader, SeaderHfTeardownActionSamPresent);
            }
            if(seader->credential->bit_length > 0) {
                consumed = true;
            }
        } else if(event.event == GuiButtonTypeCenter) {
            scene_manager_next_scene(seader->scene_manager, SeaderSceneFormats);
            consumed = true;
        } else if(event.event == SeaderWorkerEventHfTeardownComplete) {
            consumed = seader_hf_finish_teardown_action(seader);
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        consumed = seader_hf_request_teardown(seader, SeaderHfTeardownActionSamPresent);
    }
    return consumed;
}

void seader_scene_read_card_success_on_exit(void* context) {
    Seader* seader = context;

    // Clear view
    if(seader->widget) {
        widget_reset(seader->widget);
    }
    seader_temp_strings_release(seader, 4U);
    seader_wiegand_plugin_release(seader);
}
