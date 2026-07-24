#include <furi_hal_random.h>

#include "../ear_trainer_i.h"
#include "ear_scene.h"

/* ---- question set ---- */

static uint8_t rand_below(uint8_t n) {
    return n ? (uint8_t)(furi_hal_random_get() % n) : 0;
}

/* Build the order of items to test. A regular level guarantees several reps
 * of whatever it just introduced, then tops up with review drawn from
 * everything learned so far; a review level is pure mixed practice. */
static void build_queue(EarTrainerApp* app) {
    QuizState* q = &app->quiz;
    const EarLevel* level = curriculum_get(app->mode, app->level);
    q->queue_len = 0;

    if(!q->challenge) {
        for(uint8_t i = 0; i < level->new_count; i++) {
            for(uint8_t r = 0; r < QUIZ_REPS_PER_NEW; r++) {
                if(q->queue_len < sizeof(q->queue)) q->queue[q->queue_len++] = level->new_items[i];
            }
        }
        for(uint8_t r = 0; r < QUIZ_REVIEW_EXTRA; r++) {
            if(q->queue_len < sizeof(q->queue))
                q->queue[q->queue_len++] = q->pool[rand_below(q->pool_count)];
        }
    } else {
        for(uint8_t r = 0; r < CHALLENGE_QUESTIONS; r++) {
            if(q->queue_len < sizeof(q->queue))
                q->queue[q->queue_len++] = q->pool[rand_below(q->pool_count)];
        }
    }

    /* Fisher-Yates, so the guaranteed reps are not clustered at the front */
    for(uint8_t i = q->queue_len; i > 1; i--) {
        uint8_t j = rand_below(i);
        uint8_t tmp = q->queue[i - 1];
        q->queue[i - 1] = q->queue[j];
        q->queue[j] = tmp;
    }

    q->total_questions = q->queue_len;
    /* ceil without floating point */
    q->pass_score = (uint8_t)((q->total_questions * 8 + 9) / 10);
    q->star2_score = (uint8_t)((q->total_questions * 9 + 9) / 10);
}

/* ---- asking ---- */

static void push_model(EarTrainerApp* app) {
    QuizState* q = &app->quiz;
    QuizModel* m = &app->qm;

    m->mode = app->mode;
    m->question = q->q_index + 1;
    m->total = q->total_questions;
    m->score = q->correct;
    m->hints_left = q->hints_left;
    m->streak = q->streak;
    m->choice_count = q->pool_count;
    for(uint8_t i = 0; i < q->pool_count; i++)
        m->choices[i] = q->pool[i];
    m->challenge = q->challenge;
    m->mistakes_left =
        (uint8_t)(CHALLENGE_MAX_MISS >= q->mistakes ? CHALLENGE_MAX_MISS - q->mistakes : 0);
    m->show_hint = app->settings.show_mnemonic;
    m->quit_armed = q->quit_armed;
    m->played_count = q->note_count;
    for(uint8_t i = 0; i < q->note_count; i++)
        m->played_notes[i] = q->notes[i];
    quiz_view_update(app->quiz_view, m);
}

static void play_current(EarTrainerApp* app) {
    QuizState* q = &app->quiz;
    /* chords arpeggiate a touch faster so they hang together as one sound */
    uint16_t gap = (mode_content(app->mode) == ContentChord) ? 45 : 0;
    tone_player_play_sequence(app->player, q->notes, q->note_count, gap);
}

/* Turn the answer id into the actual run of notes, and pick a root that keeps
 * every note inside the playable table. */
static void build_notes(EarTrainerApp* app) {
    QuizState* q = &app->quiz;
    ContentType content = mode_content(app->mode);

    uint8_t span;
    const Pattern* pattern = NULL;
    if(content == ContentInterval) {
        span = q->answer;
    } else {
        pattern = (content == ContentChord) ? chord_get(q->answer) : scale_get(q->answer);
        span = pattern_span(pattern);
    }

    uint8_t root_max = (uint8_t)(NOTE_MIDI_MAX - span);
    if(root_max > ROOT_MIDI_MAX) root_max = ROOT_MIDI_MAX;
    if(root_max < ROOT_MIDI_MIN) root_max = ROOT_MIDI_MIN;
    q->root_midi =
        app->settings.random_root ?
            (uint8_t)(ROOT_MIDI_MIN + rand_below((uint8_t)(root_max - ROOT_MIDI_MIN + 1))) :
            ROOT_MIDI_MIN;

    if(content == ContentInterval) {
        uint8_t low = q->root_midi;
        uint8_t high = (uint8_t)(q->root_midi + q->answer);
        if(app->mode == ModeIntervalAsc) {
            q->descending = false;
        } else if(app->mode == ModeIntervalDesc) {
            q->descending = true;
        } else {
            q->descending = (rand_below(2) == 1);
        }
        q->notes[0] = q->descending ? high : low;
        q->notes[1] = q->descending ? low : high;
        q->note_count = 2;
    } else {
        q->descending = false;
        q->note_count = pattern->count;
        for(uint8_t i = 0; i < pattern->count && i < MAX_SEQUENCE; i++)
            q->notes[i] = (uint8_t)(q->root_midi + pattern->steps[i]);
    }
}

static void next_question(EarTrainerApp* app) {
    QuizState* q = &app->quiz;

    uint8_t answer = q->queue[q->q_index];
    /* avoid asking the same thing twice running when there is a choice */
    if(answer == q->last_answer && q->pool_count > 1 && q->q_index + 1 < q->queue_len) {
        uint8_t swap =
            (uint8_t)(q->q_index + 1 + rand_below((uint8_t)(q->queue_len - q->q_index - 1)));
        if(swap < q->queue_len && q->queue[swap] != answer) {
            q->queue[q->q_index] = q->queue[swap];
            q->queue[swap] = answer;
            answer = q->queue[q->q_index];
        }
    }
    q->answer = answer;
    q->last_answer = answer;

    build_notes(app);

    app->qm.phase = QuizPhaseAnswering;
    app->qm.selected = 0;
    memset(app->qm.eliminated, 0, sizeof(app->qm.eliminated));
    q->quit_armed = false;
    push_model(app);
    play_current(app);
}

/* ---- answering ---- */

static void finish_quiz(EarTrainerApp* app) {
    QuizState* q = &app->quiz;
    q->finished = true;

    if(q->challenge) {
        q->passed = (q->mistakes <= CHALLENGE_MAX_MISS) && (q->q_index >= q->total_questions);
    } else {
        q->passed = q->correct >= q->pass_score;
    }

    q->stars = 0;
    if(q->passed) {
        q->stars = 1;
        if(q->correct >= q->star2_score) q->stars = 2;
        if(q->correct == q->total_questions) q->stars = 3;
    }

    EarProgress* p = &app->progress;
    uint8_t levels = curriculum_level_count(app->mode);
    if(q->stars > p->stars[app->mode][app->level]) p->stars[app->mode][app->level] = q->stars;
    if(q->passed && app->level + 1 >= p->unlocked[app->mode] && app->level + 1 < levels) {
        p->unlocked[app->mode] = (uint8_t)(app->level + 2);
    }
    ear_progress_save(p);

    scene_manager_next_scene(app->scene_manager, EarSceneResults);
}

static void submit_answer(EarTrainerApp* app) {
    QuizState* q = &app->quiz;
    uint8_t picked = app->qm.choices[app->qm.selected];
    bool correct = (picked == q->answer);

    q->q_index++;
    app->progress.answered++;
    if(correct) {
        q->correct++;
        q->streak++;
        app->progress.correct++;
        if(q->streak > app->progress.best_streak) app->progress.best_streak = q->streak;
        if(app->settings.led) notification_message(app->notifications, &sequence_blink_green_100);
    } else {
        q->streak = 0;
        q->mistakes++;
        if(app->settings.led) notification_message(app->notifications, &sequence_blink_red_100);
        if(app->settings.vibro) notification_message(app->notifications, &sequence_single_vibro);
    }

    app->qm.phase = QuizPhaseFeedback;
    app->qm.last_correct = correct;
    app->qm.correct_id = q->answer;
    app->qm.streak = q->streak;
    push_model(app);

    /* a wrong answer lingers longer: that is when the keyboard and the hint
     * are worth reading */
    furi_timer_start(
        app->feedback_timer, furi_ms_to_ticks(correct ? FEEDBACK_RIGHT_MS : FEEDBACK_WRONG_MS));
}

/* Move the highlight, stepping over answers a hint has already struck out. */
static void move_selection(EarTrainerApp* app, int8_t delta) {
    uint8_t count = app->qm.choice_count;
    if(count == 0) return;
    uint8_t sel = app->qm.selected;
    for(uint8_t step = 0; step < count; step++) {
        sel = (uint8_t)((sel + count + delta) % count);
        if(!app->qm.eliminated[sel]) break;
    }
    app->qm.selected = sel;
    push_model(app);
}

static void spend_hint(EarTrainerApp* app) {
    QuizState* q = &app->quiz;
    if(q->hints_left == 0) return;

    /* Never strike out so much that the answer is the only thing left: at
     * least two choices always remain, so a hint narrows the field without
     * handing over the result. */
    uint8_t standing = 0;
    for(uint8_t i = 0; i < q->pool_count; i++)
        if(!app->qm.eliminated[i]) standing++;
    uint8_t budget = (standing > 2) ? (uint8_t)(standing - 2) : 0;
    if(budget > 2) budget = 2;
    if(budget == 0) return;

    uint8_t removed = 0;
    for(uint8_t attempts = 0; attempts < 60 && removed < budget; attempts++) {
        uint8_t i = rand_below(q->pool_count);
        if(app->qm.choices[i] == q->answer || app->qm.eliminated[i]) continue;
        app->qm.eliminated[i] = true;
        removed++;
    }

    /* the highlight may have been sitting on one of those */
    if(app->qm.eliminated[app->qm.selected]) move_selection(app, 1);

    if(removed) q->hints_left--;
    push_model(app);
}

/* ---- scene ---- */

/* The view reports key presses here; forwarding them through the dispatcher
 * keeps every state change on the dispatcher thread. */
static void quiz_event_callback(void* context, uint32_t event) {
    EarTrainerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

void ear_scene_quiz_on_enter(void* context) {
    EarTrainerApp* app = context;
    QuizState* q = &app->quiz;

    memset(q, 0, sizeof(QuizState));
    q->challenge = curriculum_is_challenge(app->mode, app->level);
    q->hints_left = HINTS_PER_QUIZ;
    q->last_answer = 0xFF;
    q->pool_count = curriculum_learned_upto(app->mode, app->level, q->pool, sizeof(q->pool));
    build_queue(app);

    memset(&app->qm, 0, sizeof(QuizModel));
    quiz_view_set_callback(app->quiz_view, quiz_event_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, EarViewQuiz);
    next_question(app);
}

bool ear_scene_quiz_on_event(void* context, SceneManagerEvent event) {
    EarTrainerApp* app = context;
    QuizState* q = &app->quiz;

    if(event.type == SceneManagerEventTypeBack) {
        /* one Back arms, a second one leaves: an accidental press mid-quiz
         * should not throw the run away */
        if(!q->quit_armed && !q->finished) {
            q->quit_armed = true;
            push_model(app);
            return true;
        }
        tone_player_stop(app->player);
        furi_timer_stop(app->feedback_timer);
        return false;
    }

    if(event.type != SceneManagerEventTypeCustom) return false;

    switch(event.event) {
    case ETEventPrev:
        q->quit_armed = false;
        move_selection(app, -1);
        return true;

    case ETEventNext:
        q->quit_armed = false;
        move_selection(app, 1);
        return true;

    case ETEventReplay:
        q->quit_armed = false;
        push_model(app);
        play_current(app);
        return true;

    case ETEventHint:
        q->quit_armed = false;
        spend_hint(app);
        return true;

    case ETEventAnswer:
        if(app->qm.phase != QuizPhaseAnswering) return true;
        if(app->qm.eliminated[app->qm.selected]) return true;
        submit_answer(app);
        return true;

    case ETEventFeedbackDone:
        if(q->challenge && q->mistakes > CHALLENGE_MAX_MISS) {
            finish_quiz(app);
        } else if(q->q_index >= q->total_questions) {
            finish_quiz(app);
        } else {
            next_question(app);
        }
        return true;

    default:
        return false;
    }
}

void ear_scene_quiz_on_exit(void* context) {
    EarTrainerApp* app = context;
    furi_timer_stop(app->feedback_timer);
    tone_player_stop(app->player);
    quiz_view_set_callback(app->quiz_view, NULL, NULL);
    /* lifetime counters moved during the run even if it was abandoned */
    ear_progress_save(&app->progress);
}
