#include "../specter_i.h"
#include "../helpers/log_filter.h"
#include <stdio.h>

/* The logbook, straight off the SD card. Newest entries are at the bottom, which
 * is also where the text box starts - a sweep kit's most useful line is almost
 * always the last one written. */

void specter_scene_logbook_on_enter(void* context) {
    SpecterApp* app = context;

    text_box_reset(app->text_box);
    furi_string_reset(app->text_box_store);

    uint32_t filter_index = scene_manager_get_scene_state(app->scene_manager, SpecterSceneLogbook);
    const char* filter_type = specter_log_filter_type(filter_index);

    if(specter_log_read_tail(app->text_box_store)) {
        if(filter_type) {
            /* The filtered copy can only ever be shorter than the source. */
            size_t cap = furi_string_size(app->text_box_store) + 1u;
            char* filtered = malloc(cap);
            size_t kept = specter_log_filter(
                furi_string_get_cstr(app->text_box_store), filter_type, filtered, cap);
            if(kept) {
                furi_string_set(app->text_box_store, filtered);
            } else {
                furi_string_printf(
                    app->text_box_store,
                    "No \"%s\"\nin the recent log.\n\nPress BACK to pick a\ndifferent filter.",
                    specter_log_filter_label(filter_index));
            }
            free(filtered);
        }
        text_box_set_font(app->text_box, TextBoxFontText);
        text_box_set_focus(app->text_box, TextBoxFocusEnd);
    } else {
        /* Hard-wrapped to 21 characters: three of the old lines ran past the
         * box and wrapped mid-phrase. The old text also told everyone to turn
         * Logging on - including the people who already had - so the one
         * screen meant to explain an empty logbook gave the wrong reason. */
        furi_string_set(app->text_box_store, "");
        if(!app->settings.logging) {
            furi_string_cat_str(
                app->text_box_store,
                "Saving is OFF in\n"
                "Settings - turn it on\n\n");
        }
        furi_string_cat_str(
            app->text_box_store,
            "Logbook is empty.\n\n"
            "Saved findings appear\n"
            "here. To save one:\n\n"
            " Sweep: hold OK\n"
            " Fingerprint: OK\n"
            " Survey: automatic\n"
            " Watch: automatic\n\n"
            "Files on the SD card:\n"
            " apps_data/specter/\n"
            " logbook.txt + .csv");
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
