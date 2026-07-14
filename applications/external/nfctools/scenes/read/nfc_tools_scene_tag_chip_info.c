#include "../../include/nfc_tools_i.h"

// Displays raw chip metadata (Chip, Standard, UID, ATQA, SAK, memory).
// Reuses the content of app->info_str built by tag_info.

void nfc_tools_scene_tag_chip_info_on_enter(void* context) {
    NfcToolsApp* app = context;
    // app->info_str was already filled by tag_info before navigating here
    text_box_set_font(app->text_box, TextBoxFontText);
    text_box_set_text(app->text_box, furi_string_get_cstr(app->info_str));
    text_box_set_focus(app->text_box, TextBoxFocusStart);
    view_dispatcher_switch_to_view(app->view_dispatcher, NfcToolsViewTextBox);
}

bool nfc_tools_scene_tag_chip_info_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void nfc_tools_scene_tag_chip_info_on_exit(void* context) {
    NfcToolsApp* app = context;
    text_box_reset(app->text_box);
}
