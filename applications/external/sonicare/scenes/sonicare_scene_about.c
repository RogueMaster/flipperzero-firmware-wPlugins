#include "../uk_mbirth_sonicare.h"
#include <gui/canvas.h>
#include <uk_mbirth_sonicare_icons.h>
#include <dolphin/dolphin.h>

void sonicare_scene_about_on_enter(void* context) {
    Sonicare* app = context;
    Widget* widget = app->widget;

    widget_add_icon_element(widget, 128-17, 0, &I_sonicare_brush);
    widget_add_string_element(widget, 0, 0, AlignLeft, AlignTop, FontPrimary, "Sonicare Head ID");
    widget_add_string_element(widget, 0, 12, AlignLeft, AlignTop, FontSecondary, "by mbirth.uk");
    widget_add_string_element(widget, 0, 24, AlignLeft, AlignTop, FontSecondary, "NFC unlock algo by atc1441");
    widget_add_icon_element(widget, 8, 35, &I_sonicare_qr);

    view_dispatcher_switch_to_view(app->view_dispatcher, SonicareViewWidget);
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
