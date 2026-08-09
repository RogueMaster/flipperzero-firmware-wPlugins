#include "../nfc_magic_app_i.h"

void nfc_magic_scene_gen2_wipe_partial_details_on_enter(void* context) {
    NfcMagicApp* instance = context;
    Widget* widget = instance->widget;

    widget_add_string_element(
        widget,
        0,
        0,
        AlignLeft,
        AlignTop,
        FontPrimary,
        instance->gen2_poller_is_wipe_mode ? "Blocks not wiped" : "Blocks not written");

    FuriString* message = furi_string_alloc();
    // Bitmap is block-ordered, so emit set bits ascending as-is.
    for(uint16_t block = 0; block < instance->gen2_partial_blocks_total; block++) {
        if(instance->gen2_partial_failed_bitmap[block >> 3] & (1u << (block & 7u))) {
            furi_string_cat_printf(message, "%u ", block);
        }
    }

    widget_add_text_scroll_element(widget, 0, 13, 128, 51, furi_string_get_cstr(message));
    furi_string_free(message);

    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcMagicAppViewWidget);
}

bool nfc_magic_scene_gen2_wipe_partial_details_on_event(void* context, SceneManagerEvent event) {
    NfcMagicApp* instance = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        consumed = scene_manager_previous_scene(instance->scene_manager);
    }
    return consumed;
}

void nfc_magic_scene_gen2_wipe_partial_details_on_exit(void* context) {
    NfcMagicApp* instance = context;
    widget_reset(instance->widget);
}
