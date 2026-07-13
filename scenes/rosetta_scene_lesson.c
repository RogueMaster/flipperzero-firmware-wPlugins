#include "../rosetta_i.h"

void rosetta_scene_lesson_on_enter(void* context) {
    RosettaApp* app = context;
    lesson_view_set_protocol(app->lesson_view, app->protocol);
    view_dispatcher_switch_to_view(app->view_dispatcher, RosettaViewLesson);
}

bool rosetta_scene_lesson_on_event(void* context, SceneManagerEvent event) {
    RosettaApp* app = context;
    if(event.type == SceneManagerEventTypeTick) {
        lesson_view_tick(app->lesson_view);
        return true;
    }
    return false;
}

void rosetta_scene_lesson_on_exit(void* context) {
    UNUSED(context);
}
