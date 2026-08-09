#include "../nfc_magic_app_i.h"
#include <lib/nfc/protocols/iso15693_3/iso15693_3.h>

void nfc_magic_scene_iso15693_write_confirm_widget_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    NfcMagicApp* instance = context;

    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(instance->view_dispatcher, result);
    }
}

void nfc_magic_scene_iso15693_write_confirm_on_enter(void* context) {
    NfcMagicApp* instance = context;
    Widget* widget = instance->widget;

    // 8 spaced bytes overflow the 128px width and wrap, which would push the warning off-screen, so
    // show the UID compactly (two 4-byte groups) on one line and keep each warning line short. The
    // text box fits ~4 lines at FontSecondary; every line below stays within the width.
    FuriString* temp_str = furi_string_alloc();
    for(size_t i = 0; i < ISO15693_3_UID_SIZE; ++i) {
        furi_string_cat_printf(temp_str, "%02X", instance->iso15693_target_uid[i]);
        if(i == 3) furi_string_push_back(temp_str, ' ');
    }
    furi_string_cat_str(
        temp_str, "\nOnly magic ISO15693.\ngen1 can overwrite\ndata on normal tags.");

    widget_add_string_element(widget, 3, 0, AlignLeft, AlignTop, FontPrimary, "Write UID?");
    widget_add_text_box_element(
        widget, 0, 13, 128, 38, AlignLeft, AlignTop, furi_string_get_cstr(temp_str), false);
    widget_add_button_element(
        widget,
        GuiButtonTypeCenter,
        "Write",
        nfc_magic_scene_iso15693_write_confirm_widget_callback,
        instance);
    widget_add_button_element(
        widget,
        GuiButtonTypeLeft,
        "Back",
        nfc_magic_scene_iso15693_write_confirm_widget_callback,
        instance);

    furi_string_free(temp_str);

    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcMagicAppViewWidget);
}

bool nfc_magic_scene_iso15693_write_confirm_on_event(void* context, SceneManagerEvent event) {
    NfcMagicApp* instance = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeLeft) {
            consumed = scene_manager_previous_scene(instance->scene_manager);
        } else if(event.event == GuiButtonTypeCenter) {
            scene_manager_next_scene(instance->scene_manager, NfcMagicSceneIso15693Write);
            consumed = true;
        }
    }
    return consumed;
}

void nfc_magic_scene_iso15693_write_confirm_on_exit(void* context) {
    NfcMagicApp* instance = context;
    widget_reset(instance->widget);
}
