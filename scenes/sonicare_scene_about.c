#include "../uk_mbirth_sonicare.h"
#include <uk_mbirth_sonicare_icons.h>
#include <dolphin/dolphin.h>

void sonicare_scene_about_on_enter(void* context) {
    Sonicare* app = context;
    Widget* widget = app->widget;

    FuriString* display_text = furi_string_alloc();

    furi_string_cat(display_text, "Sonicare Brush Head ID\nby mbirth.uk");

    widget_add_icon_element(widget, 128-17, 0, &I_sonicare_brush);
    widget_add_text_scroll_element(widget, 0, 0, 128-17, 64, furi_string_get_cstr(display_text));
    widget_add_icon_element(widget, 8, 35, &I_sonicare_qr);

    view_dispatcher_switch_to_view(app->view_dispatcher, SonicareViewWidget);
    furi_string_free(display_text);
}

bool sonicare_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    bool consumed = false;
    return consumed;
}

void sonicare_scene_about_on_exit(void* context) {
    Sonicare* app = context;
    widget_reset(app->widget);
}
