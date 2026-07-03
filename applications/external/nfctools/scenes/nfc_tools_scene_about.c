#include "../include/nfc_tools_i.h"

void nfc_tools_scene_about_on_enter(void* context) {
    NfcToolsApp* app = context;

    furi_string_reset(app->info_str);
    furi_string_cat_str(app->info_str, NTS_ABOUT_APP_NAME);
    furi_string_cat_str(app->info_str, "Version: " NFC_TOOLS_VERSION "\n");
    furi_string_cat_str(app->info_str, NTS_ABOUT_DEVELOPER);
    furi_string_cat_str(app->info_str, NTS_ABOUT_WEBSITE);

    text_box_set_text(app->text_box, furi_string_get_cstr(app->info_str));
    text_box_set_focus(app->text_box, TextBoxFocusStart);

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcToolsViewTextBox);
}

bool nfc_tools_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void nfc_tools_scene_about_on_exit(void* context) {
    NfcToolsApp* app = context;
    text_box_reset(app->text_box);
}
