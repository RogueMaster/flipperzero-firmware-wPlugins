#include "../clippy_app_i.h"
#include "scenes/clippy_scene.h"

void clippy_scene_usb_locked_on_enter(void* context) {
    ClippyApp* app = context;

    widget_add_icon_element(app->widget, 78, 0, &I_ActiveConnection_50x64);
    widget_add_string_multiline_element(
        app->widget, 3, 2, AlignLeft, AlignTop, FontPrimary, "Connection\nis active!");
    widget_add_string_multiline_element(
        app->widget,
        3,
        30,
        AlignLeft,
        AlignTop,
        FontSecondary,
        "Disconnect from\nPC or phone to\nuse this function.");

    view_dispatcher_switch_to_view(app->view_dispatcher, ClippyAppViewUsbLocked);
}

bool clippy_scene_usb_locked_on_event(void* context, SceneManagerEvent event) {
    ClippyApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        consumed = scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, ClippySceneCopyPasteSel);
    }

    return consumed;
}

void clippy_scene_usb_locked_on_exit(void* context) {
    ClippyApp* app = context;
    widget_reset(app->widget);
}
