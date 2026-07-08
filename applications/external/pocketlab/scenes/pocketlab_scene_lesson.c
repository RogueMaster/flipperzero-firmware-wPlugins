#include "../pocketlab_i.h"

void pocketlab_scene_lesson_on_enter(void* context) {
    PocketLab* app = context;
    const PocketLabLab* lab = app->current_lab;
    furi_assert(lab);

    lesson_view_configure(
        app->lesson_view,
        lab,
        pocketlab_is_lab_completed(app, lab),
        app->state.sound != 0,
        app->notifications);

    view_dispatcher_switch_to_view(app->view_dispatcher, PocketLabViewLesson);
}

bool pocketlab_scene_lesson_on_event(void* context, SceneManagerEvent event) {
    PocketLab* app = context;

    if(event.type == SceneManagerEventTypeCustom &&
       event.event == PocketLabCustomEventLessonDone) {
        pocketlab_award_lab(app, app->current_lab);
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }

    return false;
}

void pocketlab_scene_lesson_on_exit(void* context) {
    UNUSED(context);
}
