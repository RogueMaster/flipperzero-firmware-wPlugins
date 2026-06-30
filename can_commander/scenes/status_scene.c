#include "../can_commander.h"

void cancommander_scene_status_on_enter(void* context) {
    App* app = context;

    widget_reset(app->widget);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, furi_string_get_cstr(app->status_text));

    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewWidget);
}

bool cancommander_scene_status_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void cancommander_scene_status_on_exit(void* context) {
    App* app = context;
    widget_reset(app->widget);
}
