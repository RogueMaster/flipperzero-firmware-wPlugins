#include "../ear_trainer_i.h"
#include "ear_scene.h"

/* Introduces each new item before the quiz: its name, a keyboard showing the
 * shape, and a line on what it sounds like. OK replays it, Right moves on.
 * Reuses the quiz view so the keyboard here and the one after answering are
 * the same drawing. */

#define TEACH_ROOT 60 /* C4, fixed so the shape is what varies, not the pitch */

static void teach_event_callback(void* context, uint32_t event) {
    EarTrainerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

static uint8_t teach_current_id(EarTrainerApp* app) {
    const EarLevel* level = curriculum_get(app->mode, app->level);
    uint8_t index = app->teach_index < level->new_count ? app->teach_index : 0;
    return level->new_items[index];
}

static void teach_build_notes(EarTrainerApp* app, uint8_t id) {
    QuizState* q = &app->quiz;
    ContentType content = mode_content(app->mode);

    if(content == ContentInterval) {
        q->notes[0] = TEACH_ROOT;
        q->notes[1] = (uint8_t)(TEACH_ROOT + id);
        q->note_count = 2;
    } else {
        const Pattern* pattern = (content == ContentChord) ? chord_get(id) : scale_get(id);
        q->note_count = pattern->count;
        for(uint8_t i = 0; i < pattern->count && i < MAX_SEQUENCE; i++)
            q->notes[i] = (uint8_t)(TEACH_ROOT + pattern->steps[i]);
    }
}

static void teach_refresh(EarTrainerApp* app) {
    const EarLevel* level = curriculum_get(app->mode, app->level);
    uint8_t id = teach_current_id(app);
    teach_build_notes(app, id);

    QuizModel* m = &app->qm;
    memset(m, 0, sizeof(QuizModel));
    m->phase = QuizPhaseTeach;
    m->mode = app->mode;
    m->correct_id = id;
    m->played_count = app->quiz.note_count;
    for(uint8_t i = 0; i < app->quiz.note_count; i++)
        m->played_notes[i] = app->quiz.notes[i];
    m->teach_last = (app->teach_index + 1 >= level->new_count);
    quiz_view_update(app->quiz_view, m);
}

static void teach_play(EarTrainerApp* app) {
    uint16_t gap = (mode_content(app->mode) == ContentChord) ? 45 : 0;
    tone_player_play_sequence(app->player, app->quiz.notes, app->quiz.note_count, gap);
}

void ear_scene_teach_on_enter(void* context) {
    EarTrainerApp* app = context;
    quiz_view_set_callback(app->quiz_view, teach_event_callback, app);
    teach_refresh(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, EarViewQuiz);
    teach_play(app); /* hear it immediately, that is the point of the screen */
}

bool ear_scene_teach_on_event(void* context, SceneManagerEvent event) {
    EarTrainerApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    /* OK replays, Right advances; both arrive as quiz-view events. */
    if(event.event == ETEventAnswer || event.event == ETEventReplay) {
        teach_play(app);
        return true;
    }
    if(event.event == ETEventNext) {
        const EarLevel* level = curriculum_get(app->mode, app->level);
        if(app->teach_index + 1 < level->new_count) {
            app->teach_index++;
            teach_refresh(app);
            teach_play(app);
        } else {
            scene_manager_next_scene(app->scene_manager, EarSceneQuiz);
        }
        return true;
    }
    return false;
}

void ear_scene_teach_on_exit(void* context) {
    EarTrainerApp* app = context;
    tone_player_stop(app->player);
    quiz_view_set_callback(app->quiz_view, NULL, NULL);
}
