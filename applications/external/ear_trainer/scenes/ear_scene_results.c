#include "../ear_trainer_i.h"
#include "ear_scene.h"

typedef enum {
    ResultsRetry = 20,
    ResultsContinue,
} ResultsEvent;

static void results_button_callback(GuiButtonType result, InputType type, void* context) {
    EarTrainerApp* app = context;
    if(type != InputTypeShort) return;
    view_dispatcher_send_custom_event(
        app->view_dispatcher, result == GuiButtonTypeLeft ? ResultsRetry : ResultsContinue);
}

void ear_scene_results_on_enter(void* context) {
    EarTrainerApp* app = context;
    QuizState* q = &app->quiz;
    Widget* widget = app->widget;

    widget_reset(widget);

    widget_add_string_element(
        widget,
        64,
        12,
        AlignCenter,
        AlignBottom,
        FontPrimary,
        q->passed ? "Level clear" : "Try again");

    char line[40];
    snprintf(line, sizeof(line), "%u of %u correct", q->correct, q->total_questions);
    widget_add_string_element(widget, 64, 26, AlignCenter, AlignCenter, FontSecondary, line);

    if(q->passed) {
        char stars[8] = {0};
        for(uint8_t s = 0; s < 3; s++)
            stars[s] = (s < q->stars) ? '*' : '.';
        snprintf(line, sizeof(line), "stars %s", stars);
    } else if(q->challenge && q->mistakes > CHALLENGE_MAX_MISS) {
        snprintf(line, sizeof(line), "out of lives");
    } else {
        snprintf(line, sizeof(line), "need %u to pass", q->pass_score);
    }
    widget_add_string_element(widget, 64, 38, AlignCenter, AlignCenter, FontSecondary, line);

    bool has_next = q->passed && (app->level + 1 < LEVEL_COUNT);
    widget_add_button_element(widget, GuiButtonTypeLeft, "Retry", results_button_callback, app);
    widget_add_button_element(
        widget, GuiButtonTypeRight, has_next ? "Next" : "Levels", results_button_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, EarViewWidget);
}

bool ear_scene_results_on_event(void* context, SceneManagerEvent event) {
    EarTrainerApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == ResultsRetry) {
        /* replace results with a fresh quiz so Back still lands on the level list */
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, EarSceneLevelSelect);
        scene_manager_next_scene(app->scene_manager, EarSceneQuiz);
        return true;
    }
    if(event.event == ResultsContinue) {
        QuizState* q = &app->quiz;
        bool has_next = q->passed && (app->level + 1 < LEVEL_COUNT);
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, EarSceneLevelSelect);
        if(has_next) {
            app->level++;
            if(curriculum_get(app->level)->new_count > 0) {
                app->teach_index = 0;
                scene_manager_next_scene(app->scene_manager, EarSceneTeach);
            } else {
                scene_manager_next_scene(app->scene_manager, EarSceneQuiz);
            }
        }
        return true;
    }
    return false;
}

void ear_scene_results_on_exit(void* context) {
    EarTrainerApp* app = context;
    widget_reset(app->widget);
}
