#include "../dnie_reader.h"
#include "dnie_reader_scene.h"

void dnie_scene_about_on_enter(void* context) {
    DnieApp* app = context;
    Widget* widget = app->widget;

    widget_reset(widget);
    widget_add_string_element(widget, 64, 4, AlignCenter, AlignTop, FontPrimary, "IDentity v0.22");
    widget_add_string_multiline_element(
        widget,
        64,
        24,
        AlignCenter,
        AlignTop,
        FontSecondary,
        "ID, passports & eID\nPACE, BAC, photo, CSCA\nOnly your own docs");

    view_dispatcher_switch_to_view(app->view_dispatcher, DnieViewWidget);
}

bool dnie_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void dnie_scene_about_on_exit(void* context) {
    DnieApp* app = context;
    widget_reset(app->widget);
}
