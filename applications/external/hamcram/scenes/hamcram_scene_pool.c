#include "../hamcram.h"

enum {
    PoolIndexExam,
    PoolIndexStudy,
    PoolIndexReview,
    PoolIndexProgress,
    PoolIndexReset,
};

void hamcram_scene_pool_on_enter(void* context) {
    HamCram* app = context;
    Submenu* submenu = app->submenu;
    const PoolHeader* header = &app->pool->header;

    ProgressSummary summary;
    progress_summarize(
        app->progress, app->pool, 0, header->num_questions, app->settings.skip_figures, &summary);

    snprintf(app->line[0], sizeof(app->line[0]), "%s %s", header->name, header->years);
    submenu_set_header(submenu, app->line[0]);

    furi_string_printf(app->text, "Practice Exam (%u Qs)", header->exam_questions);
    submenu_add_item(
        submenu, furi_string_get_cstr(app->text), PoolIndexExam, hamcram_submenu_callback, app);
    submenu_add_item(submenu, "Study by Topic", PoolIndexStudy, hamcram_submenu_callback, app);
    furi_string_printf(app->text, "Review Missed (%u)", summary.review);
    submenu_add_item(
        submenu, furi_string_get_cstr(app->text), PoolIndexReview, hamcram_submenu_callback, app);
    submenu_add_item(submenu, "Progress", PoolIndexProgress, hamcram_submenu_callback, app);
    submenu_add_item(submenu, "Reset Progress", PoolIndexReset, hamcram_submenu_callback, app);
    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, HamCramScenePool));

    view_dispatcher_switch_to_view(app->view_dispatcher, HamCramViewSubmenu);
}

bool hamcram_scene_pool_on_event(void* context, SceneManagerEvent event) {
    HamCram* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        /* Leaving the pool: write progress out and release it */
        hamcram_close_pool(app);
        return false;
    }
    if(event.type != SceneManagerEventTypeCustom) return false;

    scene_manager_set_scene_state(app->scene_manager, HamCramScenePool, event.event);
    switch(event.event) {
    case PoolIndexExam:
        if(session_start_exam(
               app->session, app->pool, app->settings.skip_figures, app->settings.shuffle)) {
            scene_manager_next_scene(app->scene_manager, HamCramSceneQuiz);
        }
        break;
    case PoolIndexStudy:
        scene_manager_set_scene_state(app->scene_manager, HamCramSceneTopics, 0);
        scene_manager_next_scene(app->scene_manager, HamCramSceneTopics);
        break;
    case PoolIndexReview:
        if(session_start_review(
               app->session,
               app->pool,
               app->progress,
               app->settings.skip_figures,
               app->settings.shuffle)) {
            scene_manager_next_scene(app->scene_manager, HamCramSceneQuiz);
        } else {
            hamcram_show_message(
                app, "Nothing to review", "Questions you miss will\nbe collected here.");
        }
        break;
    case PoolIndexProgress:
        scene_manager_next_scene(app->scene_manager, HamCramSceneProgress);
        break;
    case PoolIndexReset:
        scene_manager_set_scene_state(
            app->scene_manager, HamCramSceneConfirm, HamCramConfirmReset);
        scene_manager_next_scene(app->scene_manager, HamCramSceneConfirm);
        break;
    default:
        break;
    }
    return true;
}

void hamcram_scene_pool_on_exit(void* context) {
    HamCram* app = context;
    submenu_reset(app->submenu);
}
