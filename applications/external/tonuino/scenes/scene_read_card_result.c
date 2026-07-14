#include "../application.h"
#include "scenes.h"
#include <stdio.h>

static const char* mode_names[] = {
    "",
    "Hoerspiel (Random)",
    "Album (Complete)",
    "Party (Shuffle)",
    "Einzel (Single)",
    "Hoerbuch (Progress)",
    "Admin",
    "Hoerspiel Von-Bis",
    "Album Von-Bis",
    "Party Von-Bis",
    "Hoerbuch Einzel",
    "Repeat Last",
};

void tonuino_scene_read_card_result_on_enter(void* context) {
    TonuinoApp* app = context;

    text_box_reset(app->text_box);

    char mode_str[64];
    if(app->card_data.mode >= 1 && app->card_data.mode <= ModeRepeatLast) {
        snprintf(mode_str, sizeof(mode_str), "%s", mode_names[app->card_data.mode]);
    } else {
        snprintf(mode_str, sizeof(mode_str), "Unknown (%d)", app->card_data.mode);
    }

    snprintf(
        app->read_text_buffer,
        sizeof(app->read_text_buffer),
        "Card Read:\n\n"
        "Box ID: %02X %02X %02X %02X\n"
        "Version: %d\n"
        "Folder: %d\n"
        "Mode: %s (%d)\n"
        "Special 1: %d\n"
        "Special 2: %d\n\n"
        "Raw Data:\n"
        "%02X %02X %02X %02X %02X %02X %02X %02X\n"
        "%02X %02X %02X %02X %02X %02X %02X %02X",
        app->card_data.box_id[0],
        app->card_data.box_id[1],
        app->card_data.box_id[2],
        app->card_data.box_id[3],
        app->card_data.version,
        app->card_data.folder,
        mode_str,
        app->card_data.mode,
        app->card_data.special1,
        app->card_data.special2,
        app->card_data.box_id[0],
        app->card_data.box_id[1],
        app->card_data.box_id[2],
        app->card_data.box_id[3],
        app->card_data.version,
        app->card_data.folder,
        app->card_data.mode,
        app->card_data.special1,
        app->card_data.special2,
        app->card_data.reserved[0],
        app->card_data.reserved[1],
        app->card_data.reserved[2],
        app->card_data.reserved[3],
        app->card_data.reserved[4],
        app->card_data.reserved[5],
        app->card_data.reserved[6]);

    text_box_set_text(app->text_box, app->read_text_buffer);
    text_box_set_font(app->text_box, TextBoxFontText);
    text_box_set_focus(app->text_box, TextBoxFocusStart);

    view_dispatcher_switch_to_view(app->view_dispatcher, TonuinoViewTextBox);
}

bool tonuino_scene_read_card_result_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void tonuino_scene_read_card_result_on_exit(void* context) {
    TonuinoApp* app = context;
    text_box_reset(app->text_box);
}
