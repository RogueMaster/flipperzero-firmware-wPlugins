#include "src/core/cfs_app_i.h"

void cfs_scene_about_on_enter(void* context) {
    CfsApp* app = context;
    Widget* widget = app->widget;
    widget_reset(widget);

    widget_add_string_element(widget, 64, 2, AlignCenter, AlignTop, FontPrimary, "CFS Tag Writer");

    const char* text =
        "Read, write & clone\n"
        "CFS filament spool tags\n"
        "(MIFARE Classic 1K).\n"
        "\n"
        "Known limit: batch is\n"
        "fixed.\n"
        "Clone only your own tags.\n"
        "\n"
        "MIT License\n"
        "by P. Schattenberg\n"
        "v1.0";
    widget_add_text_scroll_element(widget, 0, 16, 128, 48, text);

    view_dispatcher_switch_to_view(app->view_dispatcher, CfsViewWidget);
}

bool cfs_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void cfs_scene_about_on_exit(void* context) {
    CfsApp* app = context;
    widget_reset(app->widget);
}
