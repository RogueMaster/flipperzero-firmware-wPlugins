#include "../faraday_i.h"

void faraday_scene_results_on_enter(void* context) {
    FaradayApp* app = context;
    Widget* widget = app->widget;

    widget_reset(widget);

    FuriString* text = furi_string_alloc();
    uint8_t n = fdy_store_results_render(text, 20);

    if(n == 0) {
        furi_string_cat_printf(
            text,
            "\e#No results yet\e#\n\n"
            "Finish a Sub-GHz or NFC test and\n"
            "it lands here automatically.\n\n"
            "Handy for comparing two or three\n"
            "pouches before you buy one.\n");
    } else {
        furi_string_cat_printf(
            text,
            "\e#Saved to\e#\n%s\n\nCopy it off with qFlipper to\ncompare pouches later.\n",
            fdy_store_results_path());
    }

    /* widget copies the string into its own element */
    widget_add_text_scroll_element(widget, 0, 0, 128, 64, furi_string_get_cstr(text));
    furi_string_free(text);

    view_dispatcher_switch_to_view(app->view_dispatcher, FaradayViewAbout);
}

bool faraday_scene_results_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void faraday_scene_results_on_exit(void* context) {
    FaradayApp* app = context;
    widget_reset(app->widget);
}
