#include "../gauntlet_i.h"

static void gauntlet_result_cb(void* context, ResultEvent event) {
    UNUSED(event);
    GauntletApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, GauntletEventResultContinue);
}

void gauntlet_scene_result_on_enter(void* context) {
    GauntletApp* app = context;
    result_view_set_callback(app->result_view, gauntlet_result_cb, app);
    result_view_set(
        app->result_view,
        app->last_correct,
        app->last_points,
        app->progress->score,
        ctf_rank(app->progress->score));
    view_dispatcher_switch_to_view(app->view_dispatcher, GauntletViewResult);
}

bool gauntlet_scene_result_on_event(void* context, SceneManagerEvent event) {
    GauntletApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeTick) {
        result_view_tick(app->result_view);
        consumed = true;
    } else if(
        (event.type == SceneManagerEventTypeCustom &&
         event.event == GauntletEventResultContinue) ||
        event.type == SceneManagerEventTypeBack) {
        /* whatever happened, return to the challenge list (refreshes solved marks) */
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, GauntletSceneBrowse);
        consumed = true;
    }
    return consumed;
}

void gauntlet_scene_result_on_exit(void* context) {
    UNUSED(context);
}
