#include "../chameleon_app_i.h"

void chameleon_scene_about_on_enter(void* context) {
    ChameleonApp* app = context;
    Widget* widget = app->widget;

    widget_reset(widget);

    widget_add_text_scroll_element(
        widget,
        0,
        0,
        128,
        64,
        "Chameleon Ultra\nController\n\n"
        "Version: 1.0\n\n"
        "Control your Chameleon\n"
        "Ultra device via\n"
        "USB or Bluetooth\n\n"
        "Features:\n"
        "- Slot Management\n"
        "- Tag Reading\n"
        "- Tag Writing\n"
        "- Device Diagnostics\n\n"
        "GitHub:\n"
        "Chameleon_Flipper");

    view_dispatcher_switch_to_view(app->view_dispatcher, ChameleonViewWidget);
}

bool chameleon_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void chameleon_scene_about_on_exit(void* context) {
    ChameleonApp* app = context;
    widget_reset(app->widget);
}
