#include "flippass_scene_text_view.h"

#include "../flippass.h"
#include "flippass_scene.h"

#include <stdio.h>

void flippass_scene_text_view_show(App* app, const char* title, const char* body, uint32_t return_scene) {
    furi_assert(app);

    snprintf(app->text_view_title, sizeof(app->text_view_title), "%s", title != NULL ? title : "");
    furi_string_set_str(app->text_view_body, body != NULL ? body : "");
    app->text_view_return_scene = return_scene;
}

void flippass_scene_text_view_on_enter(void* context) {
    App* app = context;

    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 4, AlignCenter, AlignTop, FontPrimary, app->text_view_title);
    widget_add_text_scroll_element(
        app->widget, 0, 14, 128, 49, furi_string_get_cstr(app->text_view_body));
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewWidget);
}

bool flippass_scene_text_view_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        if(!scene_manager_search_and_switch_to_previous_scene(
               app->scene_manager, app->text_view_return_scene)) {
            scene_manager_previous_scene(app->scene_manager);
        }
        return true;
    }

    return false;
}

void flippass_scene_text_view_on_exit(void* context) {
    App* app = context;
    widget_reset(app->widget);
}
