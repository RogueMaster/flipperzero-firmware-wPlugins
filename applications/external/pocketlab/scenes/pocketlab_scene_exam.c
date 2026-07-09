#include "../pocketlab_i.h"

void pocketlab_scene_exam_on_enter(void* context) {
    PocketLab* app = context;
    exam_view_configure(
        app->exam_view, app->state.completed_mask, app->state.sound != 0, app->notifications);
    view_dispatcher_switch_to_view(app->view_dispatcher, PocketLabViewExam);
}

bool pocketlab_scene_exam_on_event(void* context, SceneManagerEvent event) {
    PocketLab* app = context;
    if(event.type == SceneManagerEventTypeCustom && event.event == PocketLabCustomEventExamDone) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    return false;
}

void pocketlab_scene_exam_on_exit(void* context) {
    UNUSED(context);
}
