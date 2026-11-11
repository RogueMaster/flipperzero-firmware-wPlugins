#include "../hamcram.h"

static void result_button_callback(GuiButtonType button, InputType type, void* context) {
    HamCram* app = context;
    if(type != InputTypeShort) return;
    uint32_t event = HamCramEventCenter;
    if(button == GuiButtonTypeLeft) {
        event = HamCramEventLeft;
    } else if(button == GuiButtonTypeRight) {
        event = HamCramEventRight;
    }
    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

/* Return to the menu the session was started from */
static void result_leave(HamCram* app) {
    static const uint32_t menus[] = {HamCramSceneGroups, HamCramSceneTopics, HamCramScenePool};
    if(app->result.mode == SessionModeExam) {
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, HamCramScenePool);
    } else {
        scene_manager_search_and_switch_to_previous_scene_one_of(
            app->scene_manager, menus, COUNT_OF(menus));
    }
}

void hamcram_scene_result_on_enter(void* context) {
    HamCram* app = context;
    Widget* widget = app->widget;
    const HamCramResult* result = &app->result;
    unsigned percent = result->total ? (unsigned)result->correct * 100 / result->total : 0;
    const char* title;

    snprintf(
        app->line[0],
        sizeof(app->line[0]),
        "%u of %u correct (%u%%)",
        result->correct,
        result->total,
        percent);

    if(result->mode == SessionModeExam) {
        title = result->passed ? "PASSED" : "NOT PASSED";
        snprintf(app->line[1], sizeof(app->line[1]), "%u needed to pass", result->need);
        if(result->correct < result->total) {
            widget_add_button_element(
                widget, GuiButtonTypeLeft, "Missed", result_button_callback, app);
        }
        widget_add_button_element(widget, GuiButtonTypeRight, "Done", result_button_callback, app);
    } else {
        ProgressSummary summary;
        progress_summarize(
            app->progress,
            app->pool,
            0,
            app->pool->header.num_questions,
            app->settings.skip_figures,
            &summary);
        title = result->mode == SessionModeReview ? "Review done" : "Session done";
        snprintf(app->line[1], sizeof(app->line[1]), "%u left to review", summary.review);
        widget_add_button_element(widget, GuiButtonTypeCenter, "OK", result_button_callback, app);
    }
    widget_add_string_element(widget, 64, 3, AlignCenter, AlignTop, FontPrimary, title);
    widget_add_string_element(widget, 64, 20, AlignCenter, AlignTop, FontSecondary, app->line[0]);
    widget_add_string_element(widget, 64, 32, AlignCenter, AlignTop, FontSecondary, app->line[1]);

    view_dispatcher_switch_to_view(app->view_dispatcher, HamCramViewWidget);
}

bool hamcram_scene_result_on_event(void* context, SceneManagerEvent event) {
    HamCram* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        result_leave(app);
        return true;
    }
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == HamCramEventLeft && app->result.mode == SessionModeExam) {
        /* Walk through the questions that were missed or left blank */
        if(session_start_exam_review(app->session)) {
            scene_manager_next_scene(app->scene_manager, HamCramSceneQuiz);
        }
    } else {
        result_leave(app);
    }
    return true;
}

void hamcram_scene_result_on_exit(void* context) {
    HamCram* app = context;
    widget_reset(app->widget);
}
