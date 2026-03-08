#include "../seader_i.h"
#include <dolphin/dolphin.h>

#define TAG "SeaderSceneReadCardSuccess"

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
    PluginWiegand* plugin = seader->plugin_wiegand;
    Widget* widget = seader->widget;

    // Use reusable strings instead of allocating new ones
    FuriString* type_str = seader->temp_string1;
    FuriString* bitlength_str = seader->temp_string2;
    FuriString* credential_str = seader->temp_string3;
    FuriString* sio_str = seader->temp_string4;

    dolphin_deed(DolphinDeedNfcReadSuccess);

    // Send notification
    notification_message(seader->notifications, &sequence_success);

    furi_string_set(credential_str, "");
    furi_string_set(bitlength_str, "");
    furi_string_set(sio_str, "");
    if(credential->bit_length > 0) {
        furi_string_cat_printf(bitlength_str, "%d bit", credential->bit_length);
        furi_string_cat_printf(credential_str, "0x%llX", credential->credential);

        if(credential->type == SeaderCredentialTypeNone) {
            furi_string_set(type_str, "Unknown");
        } else if(credential->type == SeaderCredentialTypeVirtual) {
            furi_string_set(type_str, "Virtual");
        } else if(credential->type == SeaderCredentialType14A) {
            furi_string_set(type_str, "14443A");
        } else if(credential->type == SeaderCredentialTypePicopass) {
            furi_string_set(type_str, "Picopass");
        } else if(credential->type == SeaderCredentialTypeMifareClassic) {
            furi_string_set(type_str, "Mifare Classic");
        } else {
            furi_string_set(type_str, "");
        }
    } else {
        furi_string_set(type_str, "Read error");
        furi_string_set(bitlength_str, seader->read_error[0] ? seader->read_error : "Read failed");

        SeaderWorker* seader_worker = seader->worker;
        SeaderUartBridge* seader_uart = seader_worker->uart;
        seader_t_1_reset(seader->uart);
        seader_ccid_check_for_sam(seader_uart);
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

    if(plugin && credential->bit_length > 0) {
        size_t format_count = plugin->count(credential->bit_length, credential->credential);
        if(format_count > 0) {
            widget_add_button_element(
                seader->widget,
                GuiButtonTypeCenter,
                "Parse",
                seader_scene_read_card_success_widget_callback,
                seader);
        }
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
    if(credential->sio[0] == 0x30) {
        switch(credential->sio_start_block) {
        case 6:
            furi_string_set(sio_str, "+SIO(SE)");
            break;
        case 10:
            furi_string_set(sio_str, "+SIO(SR)");
            break;
        default:
            FURI_LOG_E(TAG, "Unknown SIO start block: %d", credential->sio_start_block);
            furi_string_set(sio_str, "+SIO(?)");
            break;
        }
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
            consumed = scene_manager_previous_scene(seader->scene_manager);
        } else if(event.event == GuiButtonTypeRight) {
            if(seader->credential->bit_length > 0) {
                scene_manager_next_scene(seader->scene_manager, SeaderSceneCardMenu);
            } else {
                scene_manager_search_and_switch_to_previous_scene(
                    seader->scene_manager, SeaderSceneSamPresent);
            }
            consumed = true;
        } else if(event.event == GuiButtonTypeCenter) {
            scene_manager_next_scene(seader->scene_manager, SeaderSceneFormats);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(
            seader->scene_manager, SeaderSceneSamPresent);
        consumed = true;
    }
    return consumed;
}

void seader_scene_read_card_success_on_exit(void* context) {
    Seader* seader = context;

    // Clear view
    widget_reset(seader->widget);
}
