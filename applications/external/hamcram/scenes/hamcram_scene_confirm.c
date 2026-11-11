#include "../hamcram.h"

static void confirm_dialog_callback(DialogExResult result, void* context) {
    HamCram* app = context;
    uint32_t event = HamCramEventCenter;
    if(result == DialogExResultLeft) {
        event = HamCramEventLeft;
    } else if(result == DialogExResultRight) {
        event = HamCramEventRight;
    }
    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

/* Score the exam, fold the answers into the study stats and save */
static void confirm_grade_exam(HamCram* app) {
    Session* session = app->session;
    const PoolHeader* header = &app->pool->header;

    for(uint16_t i = 0; i < session->count; i++) {
        const SessionItem* item = &session->items[i];
        if(session_item_answered(item)) {
            progress_record(app->progress, item->question, session_item_correct(item));
        }
    }
    app->result.mode = SessionModeExam;
    app->result.correct = session->correct;
    app->result.total = session->count;
    app->result.need = header->pass_correct;
    app->result.passed = session->correct >= header->pass_correct;
    progress_record_exam(app->progress, session->correct, app->result.passed);
    hamcram_save_progress(app);
}

void hamcram_scene_confirm_on_enter(void* context) {
    HamCram* app = context;
    DialogEx* dialog = app->dialog;
    uint32_t kind = scene_manager_get_scene_state(app->scene_manager, HamCramSceneConfirm);

    if(kind == HamCramConfirmExam) {
        const Session* session = app->session;
        snprintf(
            app->line[0],
            sizeof(app->line[0]),
            "Answered %u of %u.\n%s",
            session->answered,
            session->count,
            session->answered < session->count ? "Blanks count as wrong." : "Ready to grade.");
        dialog_ex_set_header(dialog, "Finish exam?", 64, 2, AlignCenter, AlignTop);
        dialog_ex_set_text(dialog, app->line[0], 64, 30, AlignCenter, AlignCenter);
        dialog_ex_set_left_button_text(dialog, "Quit");
        dialog_ex_set_center_button_text(dialog, "Grade");
        /* Three buttons share 128 px: keep every label short */
        dialog_ex_set_right_button_text(dialog, "Back");
    } else {
        snprintf(
            app->line[0],
            sizeof(app->line[0]),
            "Erase %s stats\nand exam history?",
            app->pool->header.name);
        dialog_ex_set_header(dialog, "Reset progress?", 64, 2, AlignCenter, AlignTop);
        dialog_ex_set_text(dialog, app->line[0], 64, 30, AlignCenter, AlignCenter);
        dialog_ex_set_left_button_text(dialog, "Cancel");
        dialog_ex_set_right_button_text(dialog, "Reset");
    }
    dialog_ex_set_context(dialog, app);
    dialog_ex_set_result_callback(dialog, confirm_dialog_callback);
    view_dispatcher_switch_to_view(app->view_dispatcher, HamCramViewDialog);
}

bool hamcram_scene_confirm_on_event(void* context, SceneManagerEvent event) {
    HamCram* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;
    uint32_t kind = scene_manager_get_scene_state(app->scene_manager, HamCramSceneConfirm);

    if(kind == HamCramConfirmExam) {
        if(event.event == HamCramEventLeft) {
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, HamCramScenePool);
        } else if(event.event == HamCramEventCenter) {
            confirm_grade_exam(app);
            scene_manager_next_scene(app->scene_manager, HamCramSceneResult);
            hamcram_feedback_exam(app, app->result.passed);
        } else {
            scene_manager_previous_scene(app->scene_manager);
        }
    } else if(event.event != HamCramEventCenter) {
        if(event.event == HamCramEventRight) {
            progress_reset(app->progress);
            hamcram_save_progress(app);
        }
        scene_manager_previous_scene(app->scene_manager);
    }
    return true;
}

void hamcram_scene_confirm_on_exit(void* context) {
    HamCram* app = context;
    dialog_ex_reset(app->dialog);
}
