#include "../hamcram.h"

#define QUIZ_FIGURE_NOTE "[Figure not shown: see ncvec.org]"

static void quiz_view_callback(QuizViewEvent event, uint8_t slot, void* context) {
    HamCram* app = context;
    uint32_t custom = HamCramEventQuizNext;
    if(event == QuizViewEventAnswer) {
        custom = HamCramEventQuizAnswer + (slot & 0x03);
    } else if(event == QuizViewEventPrev) {
        custom = HamCramEventQuizPrev;
    }
    view_dispatcher_send_custom_event(app->view_dispatcher, custom);
}

/* Displayed slot holding original answer `answer` */
static int8_t quiz_slot_of(uint8_t perm, uint8_t answer) {
    for(uint8_t slot = 0; slot < POOL_ANSWERS; slot++) {
        if(session_perm_get(perm, slot) == answer) return slot;
    }
    return -1;
}

static const char* quiz_verdict(const SessionItem* item) {
    if(!session_item_answered(item)) return "Skipped";
    return session_item_correct(item) ? "Correct" : "Wrong";
}

static void quiz_show(HamCram* app) {
    Session* session = app->session;
    const SessionItem* item = &session->items[session->pos];
    QuizViewData data = {.chosen = -1, .correct = -1};
    PoolQuestion question;

    snprintf(app->line[0], sizeof(app->line[0]), "%u/%u", session->pos + 1, session->count);
    data.left = app->line[0];

    app->quiz_loaded = pool_read_question(app->pool, item->question, &question);
    if(!app->quiz_loaded) {
        /* Shown as an already-revealed card so OK simply moves on */
        data.question = "This question could not be read from the SD card.";
        for(uint8_t slot = 0; slot < POOL_ANSWERS; slot++)
            data.answers[slot] = "-";
        data.reveal = true;
        quiz_view_set_question(app->quiz_view, &data);
        return;
    }

    app->quiz_correct = question.correct;
    data.right = question.id;
    data.question = question.question;
    if(question.flags & POOL_FLAG_FIGURE) data.note = QUIZ_FIGURE_NOTE;
    if(question.ref[0] != '\0') {
        /* Part 97 citation from the pool, for looking the rule up afterwards */
        snprintf(app->line[1], sizeof(app->line[1]), "FCC rule: %.48s", question.ref);
        data.footer = app->line[1];
    }
    for(uint8_t slot = 0; slot < POOL_ANSWERS; slot++) {
        data.answers[slot] = question.answers[session_perm_get(item->perm, slot)];
    }

    bool answered = session_item_answered(item);
    if(answered) data.chosen = quiz_slot_of(item->perm, session_item_answer(item));

    switch(session->mode) {
    case SessionModeExam:
        /* No feedback until the exam is graded */
        break;
    case SessionModeExamReview:
        data.reveal = true;
        break;
    default:
        data.reveal = answered;
        break;
    }
    if(data.reveal) {
        data.correct = quiz_slot_of(item->perm, question.correct);
        data.center = quiz_verdict(item);
    }
    quiz_view_set_question(app->quiz_view, &data);
}

static void quiz_finish(HamCram* app) {
    Session* session = app->session;
    switch(session->mode) {
    case SessionModeExam:
        scene_manager_set_scene_state(app->scene_manager, HamCramSceneConfirm, HamCramConfirmExam);
        scene_manager_next_scene(app->scene_manager, HamCramSceneConfirm);
        break;
    case SessionModeExamReview:
        scene_manager_previous_scene(app->scene_manager);
        break;
    default:
        hamcram_save_progress(app);
        if(session->answered == 0) {
            scene_manager_previous_scene(app->scene_manager);
        } else {
            app->result.mode = session->mode;
            app->result.correct = session->correct;
            app->result.total = session->answered;
            app->result.need = 0;
            app->result.passed = false;
            scene_manager_next_scene(app->scene_manager, HamCramSceneResult);
        }
        break;
    }
}

static void quiz_next(HamCram* app) {
    Session* session = app->session;
    if(session->pos + 1 < session->count) {
        session->pos++;
        quiz_show(app);
    } else {
        quiz_finish(app);
    }
}

static void quiz_answer(HamCram* app, uint8_t slot) {
    Session* session = app->session;
    const SessionItem* item = &session->items[session->pos];
    if(!app->quiz_loaded) return;

    uint8_t answer = session_perm_get(item->perm, slot);
    bool correct = answer == app->quiz_correct;

    if(session->mode == SessionModeExam) {
        /* Answers can be changed freely until the exam is graded */
        session_answer(session, answer, correct);
        if(session->pos + 1 == session->count && session->answered < session->count) {
            /* End of the paper with blanks left: go back to the first one
             * rather than offering to grade an unfinished exam. */
            session->pos = 0;
            while(session_item_answered(&session->items[session->pos]))
                session->pos++;
            quiz_show(app);
        } else {
            quiz_next(app);
        }
    } else if(session->mode != SessionModeExamReview && !session_item_answered(item)) {
        session_answer(session, answer, correct);
        progress_record(app->progress, item->question, correct);
        quiz_view_reveal(
            app->quiz_view,
            slot,
            quiz_slot_of(item->perm, app->quiz_correct),
            correct ? "Correct" : "Wrong");
        hamcram_feedback(app, correct);
    }
}

void hamcram_scene_quiz_on_enter(void* context) {
    HamCram* app = context;
    /* The session was prepared by whichever scene sent us here; coming back
     * from a dialog resumes at the same question. */
    quiz_view_set_callback(app->quiz_view, quiz_view_callback, app);
    quiz_show(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, HamCramViewQuiz);
}

bool hamcram_scene_quiz_on_event(void* context, SceneManagerEvent event) {
    HamCram* app = context;
    Session* session = app->session;

    if(event.type == SceneManagerEventTypeBack) {
        if(session->mode == SessionModeExam) {
            scene_manager_set_scene_state(
                app->scene_manager, HamCramSceneConfirm, HamCramConfirmExam);
            scene_manager_next_scene(app->scene_manager, HamCramSceneConfirm);
            return true;
        }
        hamcram_save_progress(app);
        return false;
    }
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event >= HamCramEventQuizAnswer && event.event < HamCramEventQuizAnswer + 4) {
        quiz_answer(app, event.event - HamCramEventQuizAnswer);
    } else if(event.event == HamCramEventQuizNext) {
        quiz_next(app);
    } else if(event.event == HamCramEventQuizPrev) {
        if(session->pos > 0) {
            session->pos--;
            quiz_show(app);
        }
    } else {
        return false;
    }
    return true;
}

void hamcram_scene_quiz_on_exit(void* context) {
    HamCram* app = context;
    quiz_view_set_callback(app->quiz_view, NULL, NULL);
}
