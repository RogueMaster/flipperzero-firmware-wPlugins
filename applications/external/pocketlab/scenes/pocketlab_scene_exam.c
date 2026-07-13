#include "../pocketlab_i.h"

void pocketlab_scene_exam_on_enter(void* context) {
    PocketLab* app = context;
    exam_view_configure(
        app->exam_view, app->state.completed_mask, app->state.sound != 0, app->notifications);
    view_dispatcher_switch_to_view(app->view_dispatcher, PocketLabViewExam);
}

#define POCKETLAB_QUIZ_XP_PER_CORRECT 5

bool pocketlab_scene_exam_on_event(void* context, SceneManagerEvent event) {
    PocketLab* app = context;
    if(event.type == SceneManagerEventTypeCustom && event.event == PocketLabCustomEventExamDone) {
        // Reward XP for the quiz too (capped at POCKETLAB_XP_MAX).
        const uint8_t score = exam_view_get_score(app->exam_view);
        if(score > 0) {
            pocketlab_add_xp(app, (uint32_t)score * POCKETLAB_QUIZ_XP_PER_CORRECT);
            pocketlab_storage_save(&app->state);
        }
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    return false;
}

void pocketlab_scene_exam_on_exit(void* context) {
    UNUSED(context);
}
