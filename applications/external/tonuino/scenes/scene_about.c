#include "../application.h"
#include "scenes.h"
#include <stdio.h>

void tonuino_scene_about_on_enter(void* context) {
    TonuinoApp* app = context;

    text_box_reset(app->text_box);

    char about_text[512];
    snprintf(
        about_text,
        sizeof(about_text),
        "TonUINO Writer\n"
        "for Flipper Zero\n\n"
        "Version: %s\n"
        "Build: %d\n\n"
        "Autor: Thomas\n"
        "Kekeisen-Schanz\n\n"
        "https://thomaskekeisen.de\n"
        "https://bastelsaal.de",
        APP_VERSION,
        APP_BUILD_NUMBER);

    text_box_set_text(app->text_box, about_text);
    text_box_set_font(app->text_box, TextBoxFontText);
    text_box_set_focus(app->text_box, TextBoxFocusStart);

    view_dispatcher_switch_to_view(app->view_dispatcher, TonuinoViewTextBox);
}

bool tonuino_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void tonuino_scene_about_on_exit(void* context) {
    TonuinoApp* app = context;
    text_box_reset(app->text_box);
}
