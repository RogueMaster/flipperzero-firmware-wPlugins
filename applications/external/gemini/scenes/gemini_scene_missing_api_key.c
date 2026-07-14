#include "../gemini_app_i.h"

void gemini_scene_missing_api_key_on_enter(void* context) {
    GeminiApp* app = context;
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 0, 25, AlignLeft, AlignTop, FontPrimary, "MISSING API KEY");
    widget_add_string_element(
        app->widget, 0, 40, AlignLeft, AlignTop, FontSecondary, "apps_data/gemini_ia/key.txt");
    view_dispatcher_switch_to_view(app->view_dispatcher, GeminiViewWidget);
}

bool gemini_scene_missing_api_key_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false; // event not handled.
}

void gemini_scene_missing_api_key_on_exit(void* context) {
    UNUSED(context);
}
