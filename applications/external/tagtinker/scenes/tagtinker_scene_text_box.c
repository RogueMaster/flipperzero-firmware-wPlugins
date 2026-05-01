#include "../tagtinker_app.h"

void tagtinker_scene_text_box_on_enter(void* ctx) {
    TagTinkerApp* app = ctx;
    view_dispatcher_switch_to_view(app->view_dispatcher, TagTinkerViewTextBox);
}

bool tagtinker_scene_text_box_on_event(void* ctx, SceneManagerEvent event) {
    TagTinkerApp* app = ctx;
    bool handled = false;

    if(event.type == SceneManagerEventTypeBack) {
        handled = scene_manager_previous_scene(app->scene_manager);
    }

    return handled;
}

void tagtinker_scene_text_box_on_exit(void* ctx) {
    TagTinkerApp* app = ctx;
    text_box_reset(app->text_box);
}
