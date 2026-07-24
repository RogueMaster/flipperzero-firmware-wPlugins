#include "../specter_i.h"
#include <stdio.h>

/* The logbook, straight off the SD card. Newest entries are at the bottom, which
 * is also where the text box starts - a sweep kit's most useful line is almost
 * always the last one written. */

void specter_scene_logbook_on_enter(void* context) {
    SpecterApp* app = context;

    text_box_reset(app->text_box);
    furi_string_reset(app->text_box_store);

    if(specter_log_read_tail(app->text_box_store)) {
        text_box_set_font(app->text_box, TextBoxFontText);
        text_box_set_focus(app->text_box, TextBoxFocusEnd);
    } else {
        furi_string_set(
            app->text_box_store,
            "Logbook is empty.\n\n"
            "Findings land here when you:\n"
            " - hold OK on the Sweep screen\n"
            " - press OK on Fingerprint\n"
            " - finish a Site Survey\n"
            " - catch a reader in Watch\n\n"
            "Turn Logging on in Settings.\n"
            "Saved on the SD card as\n"
            "apps_data/specter/logbook.txt\n"
            "and .csv for a spreadsheet.");
        text_box_set_font(app->text_box, TextBoxFontText);
        text_box_set_focus(app->text_box, TextBoxFocusStart);
    }

    text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
    view_dispatcher_switch_to_view(app->view_dispatcher, SpecterViewTextBox);
}

bool specter_scene_logbook_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void specter_scene_logbook_on_exit(void* context) {
    SpecterApp* app = context;
    text_box_reset(app->text_box);
    furi_string_reset(app->text_box_store);
}
