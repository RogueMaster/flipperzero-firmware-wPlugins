#include "../gauntlet_i.h"

static void gauntlet_solve_input_cb(void* context) {
    GauntletApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, GauntletEventTextCommit);
}

void gauntlet_scene_solve_on_enter(void* context) {
    GauntletApp* app = context;
    TextInput* ti = app->text_input;

    text_input_reset(ti);
    text_input_set_header_text(ti, "Enter the flag");
    text_input_set_result_callback(
        ti, gauntlet_solve_input_cb, app, app->text_buf, sizeof(app->text_buf), false);

    view_dispatcher_switch_to_view(app->view_dispatcher, GauntletViewTextInput);
}

bool gauntlet_scene_solve_on_event(void* context, SceneManagerEvent event) {
    GauntletApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom && event.event == GauntletEventTextCommit) {
        CtfChallenge* c = gauntlet_current_challenge(app);
        bool correct = c && c->answer_hash != 0 && ctf_check_answer(c, app->text_buf);
        app->last_correct = correct;
        app->last_points = 0;

        if(correct) {
            uint16_t award = c->points;
            if(app->hint_used && !app->hints_free) {
                int a = (int)c->points - GAUNTLET_HINT_PENALTY;
                award = (uint16_t)(a < 10 ? 10 : a);
            }
            if(ctf_progress_mark_solved(app->progress, c->id, award)) {
                app->last_points = award;
                ctf_store_progress_save(app->storage, app->progress);
            }
            gauntlet_notify_correct(app);
        } else {
            gauntlet_notify_wrong(app);
        }

        scene_manager_next_scene(app->scene_manager, GauntletSceneResult);
        consumed = true;
    }
    return consumed;
}

void gauntlet_scene_solve_on_exit(void* context) {
    GauntletApp* app = context;
    text_input_reset(app->text_input);
}
