#include "../cogs_mikai.h"

void cogs_mikai_scene_about_on_enter(void* context) {
    COGSMyKaiApp* app = context;
    Widget* widget = app->widget;

    widget_add_string_element(widget, 64, 5, AlignCenter, AlignTop, FontPrimary, "COGS MyKai");
    widget_add_string_element(widget, 64, 18, AlignCenter, AlignTop, FontSecondary, "v0.4");
    widget_add_string_element(widget, 64, 30, AlignCenter, AlignTop, FontSecondary, "COGES MyKey NFC");
    widget_add_string_element(widget, 64, 40, AlignCenter, AlignTop, FontSecondary, "Reader/Writer");
    widget_add_string_element(widget, 64, 52, AlignCenter, AlignTop, FontSecondary, "Based on libmikai, built by luhf");

    view_dispatcher_switch_to_view(app->view_dispatcher, COGSMyKaiViewWidget);
}

bool cogs_mikai_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void cogs_mikai_scene_about_on_exit(void* context) {
    COGSMyKaiApp* app = context;
    widget_reset(app->widget);
}
