#include "../gemini_app_i.h"

void gemini_scene_under_construction_on_enter(void* context) {
    GeminiApp* app = context;
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 0, 25, AlignLeft, AlignTop, FontPrimary, "UNDER CONSTRUCTION");
    view_dispatcher_switch_to_view(app->view_dispatcher, GeminiViewWidget);
}

bool gemini_scene_under_construction_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false; // event not handled.
}

void gemini_scene_under_construction_on_exit(void* context) {
    UNUSED(context);
}
