#include "../ghosttag_i.h"

void ghosttag_scene_about_on_enter(void* context) {
    GhostTagApp* app = context;
    Widget* widget = app->widget;

    widget_reset(widget);
    widget_add_icon_element(widget, 2, 2, &I_ghost_10px);
    widget_add_string_element(
        widget, 16, 2, AlignLeft, AlignTop, FontPrimary, "GhostTag v" GHOSTTAG_VERSION);
    widget_add_string_element(
        widget, 16, 14, AlignLeft, AlignTop, FontSecondary, "BLE anti-stalking");

    widget_add_text_scroll_element(
        widget,
        0,
        26,
        128,
        38,
        "Finds AirTags, Tiles & Samsung\n"
        "SmartTags that travel with you\n"
        "and warns when one is tailing\n"
        "you.\n"
        " \n"
        "Needs the WiFi/BLE devboard\n"
        "(ESP32) flashed with GhostTag\n"
        "firmware. Pairs over UART.\n"
        " \n"
        "A privacy tool - use it only on\n"
        "yourself or with consent.\n"
        " \n"
        "by at0m-b0mb\n"
        "github.com/at0m-b0mb");

    view_dispatcher_switch_to_view(app->view_dispatcher, GhostTagViewAbout);
}

bool ghosttag_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void ghosttag_scene_about_on_exit(void* context) {
    GhostTagApp* app = context;
    widget_reset(app->widget);
}
