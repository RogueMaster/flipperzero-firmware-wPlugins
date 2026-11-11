#include "../cfw_app.h"

void cfw_app_scene_misc_credits_on_enter(void* context) {
    CFWApp* app = context;
    Widget* widget = app->widget;
    widget_reset(widget);

    widget_add_string_element(widget, 64, 2, AlignCenter, AlignTop, FontPrimary, "Credits");
    widget_add_text_scroll_element(
        widget,
        0,
        15,
        128,
        49,
        "RogueMaster Firmware\n"
        "By RogueMaster\n"
        "\n"
        "Built on the official\n"
        "Flipper Zero firmware,\n"
        "with contributions from\n"
        "Unleashed, Momentum,\n"
        "and the community.\n"
        "\n"
        "Thanks to all firmware,\n"
        "app and asset authors,\n"
        "contributors and\n"
        "supporters.");

    view_dispatcher_switch_to_view(app->view_dispatcher, CFWAppViewWidget);
}

bool cfw_app_scene_misc_credits_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void cfw_app_scene_misc_credits_on_exit(void* context) {
    CFWApp* app = context;
    widget_reset(app->widget);
}
