#include "../nfc_magic_app_i.h"

void nfc_magic_scene_uscuid_ul_partial_details_on_enter(void* context) {
    NfcMagicApp* instance = context;
    Widget* widget = instance->widget;

    widget_add_string_element(widget, 0, 0, AlignLeft, AlignTop, FontPrimary, "Not Written");

    FuriString* message = furi_string_alloc();
    // Bitmap is page-ordered, so emit set bits ascending as-is.
    for(uint16_t page = 0; page < instance->write_progress_total; page++) {
        if(instance->write_failed_bitmap[page >> 3] & (1u << (page & 7u))) {
            furi_string_cat_printf(message, "%u ", page);
        }
    }

    widget_add_text_scroll_element(widget, 0, 13, 128, 51, furi_string_get_cstr(message));
    furi_string_free(message);

    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcMagicAppViewWidget);
}

bool nfc_magic_scene_uscuid_ul_partial_details_on_event(void* context, SceneManagerEvent event) {
    NfcMagicApp* instance = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        consumed = scene_manager_previous_scene(instance->scene_manager);
    }
    return consumed;
}

void nfc_magic_scene_uscuid_ul_partial_details_on_exit(void* context) {
    NfcMagicApp* instance = context;
    widget_reset(instance->widget);
}
