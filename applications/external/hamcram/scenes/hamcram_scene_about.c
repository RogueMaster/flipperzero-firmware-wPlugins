#include "../hamcram.h"

void hamcram_scene_about_on_enter(void* context) {
    HamCram* app = context;
    FuriString* text = app->text;

    furi_string_printf(text, "\e#Ham Cram v%s\nby @NicPWNs (N1CPJ)\n\n", HAMCRAM_VERSION);
    hamcram_cat_wrapped(
        text,
        "Offline study for the US amateur radio license exams.\n\n"
        "\e#Controls\n"
        "Up/Down: scroll, then pick an answer\n"
        "OK: answer or continue\n"
        "Left/Right: previous or next question\n\n"
        "\e#Installed pools\n");
    for(uint8_t i = 0; i < app->pool_count; i++) {
        furi_string_cat_printf(text, "%s %s\n", app->pools[i].name, app->pools[i].years);
    }
    if(app->pool_count == 0) furi_string_cat_str(text, "None found\n");
    hamcram_cat_wrapped(
        text,
        "\nQuestions come from the public domain pools published by the "
        "NCVEC Question Pool Committee.\n\n"
        "Ham Cram is an independent project. It is not affiliated with or "
        "endorsed by the ARRL, NCVEC or the FCC.\n\n"
        "Open source, MIT license:\n"
        "github.com/NicPWNs/HamCram\n");

    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, furi_string_get_cstr(text));
    view_dispatcher_switch_to_view(app->view_dispatcher, HamCramViewWidget);
}

bool hamcram_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void hamcram_scene_about_on_exit(void* context) {
    HamCram* app = context;
    widget_reset(app->widget);
}
