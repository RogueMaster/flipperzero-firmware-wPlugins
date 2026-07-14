#include "../app_user.h"

void app_scene_device_no_connected_on_enter(void* context) {
    App* app = context;
    draw_device_no_connected(app);

    view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);
}

bool app_scene_device_no_connected_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    bool consumed = false;

    return consumed;
}

void app_scene_device_no_connected_on_exit(void* context) {
    App* app = context;
    widget_reset(app->widget);
}
