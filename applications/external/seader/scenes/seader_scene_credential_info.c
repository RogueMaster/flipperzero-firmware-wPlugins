#include "../seader_i.h"
#include "../credential_sio_label.h"
#include <dolphin/dolphin.h>

#define TAG "SeaderCredentialInfoScene"

static bool seader_credential_is_picopass_sio_context(const SeaderCredential* credential) {
    return credential && (credential->type == SeaderCredentialTypePicopass ||
                          (credential->has_pacs_media_type &&
                           credential->pacs_media_type == SeaderPacsMediaTypePicopass));
}

void seader_scene_credential_info_widget_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    Seader* seader = context;
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(seader->view_dispatcher, result);
    }
}

void seader_scene_credential_info_on_enter(void* context) {
    Seader* seader = context;
    SeaderCredential* credential = seader->credential;
    seader_wiegand_plugin_acquire(seader);
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

    furi_string_set(credential_str, "");
    furi_string_set(bitlength_str, "");
    furi_string_set(sio_str, "");
    if(credential->bit_length > 0) {
        furi_string_cat_printf(bitlength_str, "%d bit", credential->bit_length);
        furi_string_cat_printf(credential_str, "0x%llX", credential->credential);
        furi_string_set(type_str, seader_credential_get_type_label(credential));
    }

    widget_add_button_element(
        seader->widget,
        GuiButtonTypeLeft,
        "Back",
        seader_scene_credential_info_widget_callback,
        seader);

    if(credential->bit_length > 0) {
        widget_add_button_element(
            seader->widget,
            GuiButtonTypeCenter,
            "Parse",
            seader_scene_credential_info_widget_callback,
            seader);
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
        furi_string_set(sio_str, sio_label);
        widget_add_string_element(
            widget, 64, 48, AlignCenter, AlignCenter, FontSecondary, furi_string_get_cstr(sio_str));
    }

    // No need to free strings as they are reused from seader struct

    view_dispatcher_switch_to_view(seader->view_dispatcher, SeaderViewWidget);
}

bool seader_scene_credential_info_on_event(void* context, SceneManagerEvent event) {
    Seader* seader = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeLeft) {
            consumed = scene_manager_previous_scene(seader->scene_manager);
        } else if(event.event == GuiButtonTypeCenter) {
            scene_manager_next_scene(seader->scene_manager, SeaderSceneFormats);
            consumed = true;
        } else if(event.event == SeaderCustomEventViewExit) {
            view_dispatcher_switch_to_view(seader->view_dispatcher, SeaderViewWidget);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        view_dispatcher_switch_to_view(seader->view_dispatcher, SeaderViewWidget);
        consumed = true;
    }
    return consumed;
}

void seader_scene_credential_info_on_exit(void* context) {
    Seader* seader = context;

    // Clear views
    if(seader->widget) {
        widget_reset(seader->widget);
    }
    seader_temp_strings_release(seader, 4U);
    seader_wiegand_plugin_release(seader);
}
