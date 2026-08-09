#include "cidr_challenge.h"

typedef enum {
    EventTypeInput,
    EventTypeTick,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} AppEvent;

static const uint16_t exam_sizes[] = {10, 25, 50};

uint16_t exam_size(uint8_t index) {
    if(index >= COUNT_OF(exam_sizes)) index = 0;
    return exam_sizes[index];
}

uint32_t speed_time_left_ms(const Session* session) {
    uint32_t now = furi_get_tick();
    if(now >= session->deadline) return 0;
    return (session->deadline - now) * 1000 / furi_kernel_get_tick_frequency();
}

static uint32_t ticks_to_ms(uint32_t ticks) {
    return ticks * 1000 / furi_kernel_get_tick_frequency();
}

static void input_callback(InputEvent* input_event, void* context) {
    FuriMessageQueue* queue = context;
    AppEvent event = {.type = EventTypeInput, .input = *input_event};
    furi_message_queue_put(queue, &event, FuriWaitForever);
}

static void timer_callback(void* context) {
    FuriMessageQueue* queue = context;
    AppEvent event = {.type = EventTypeTick};
    furi_message_queue_put(queue, &event, 0);
}

static void play_feedback(CidrApp* app, bool correct) {
    if(!app->settings.sound) return;
    notification_message(app->notifications, correct ? &sequence_success : &sequence_error);
}

static void next_question(CidrApp* app) {
    Session* session = &app->session;
    quiz_generate(&session->question, app->settings.difficulty);
    session->selected = 0;
    session->answered = false;
    session->last_xp = 0;
    session->question_start = furi_get_tick();
}

static void session_start(CidrApp* app, GameMode mode) {
    Session* session = &app->session;
    memset(session, 0, sizeof(Session));

    session->mode = mode;
    if(mode == ModeExam) session->total = exam_size(app->settings.exam_index);
    if(mode == ModeSpeed) {
        session->deadline = furi_get_tick() + furi_ms_to_ticks(SPEED_DURATION_MS);
    }

    next_question(app);
    app->scene = SceneQuiz;
}

static void session_finish(CidrApp* app) {
    Session* session = &app->session;
    if(session->finished) return;
    session->finished = true;

    Stats* stats = &app->stats;
    stats->questions += session->asked;
    stats->correct += session->correct;
    stats->xp += session->xp;
    if(session->best_streak > stats->best_streak) stats->best_streak = session->best_streak;
    if(session->fastest_ms &&
       (stats->fastest_ms == 0 || session->fastest_ms < stats->fastest_ms)) {
        stats->fastest_ms = session->fastest_ms;
    }
    if(session->asked) stats->sessions++;

    stats_save(stats);
    app->scene = SceneResult;
}

static void submit_answer(CidrApp* app) {
    Session* session = &app->session;
    if(session->answered) return;

    session->elapsed_ms = ticks_to_ms(furi_get_tick() - session->question_start);
    if(session->elapsed_ms > MAX_ANSWER_TIME_MS) session->elapsed_ms = MAX_ANSWER_TIME_MS;
    session->last_correct = session->selected == session->question.correct;
    session->answered = true;
    session->answer_time = furi_get_tick();
    session->asked++;

    if(session->last_correct) {
        session->correct++;
        session->streak++;
        if(session->streak > session->best_streak) session->best_streak = session->streak;

        session->last_xp = XP_CORRECT;
        if(session->elapsed_ms <= XP_FAST_ANSWER_MS) session->last_xp += XP_FAST_BONUS;
        if(session->streak % XP_STREAK_STEP == 0) session->last_xp += XP_STREAK_BONUS;
        session->xp += session->last_xp;

        if(session->fastest_ms == 0 || session->elapsed_ms < session->fastest_ms) {
            session->fastest_ms = session->elapsed_ms;
        }
    } else {
        session->streak = 0;
    }

    play_feedback(app, session->last_correct);
}

static void advance_question(CidrApp* app) {
    Session* session = &app->session;

    if(session->mode == ModeSurvival && !session->last_correct) {
        session_finish(app);
    } else if(session->mode == ModeExam && session->asked >= session->total) {
        session_finish(app);
    } else if(session->mode == ModeSpeed && furi_get_tick() >= session->deadline) {
        session_finish(app);
    } else {
        next_question(app);
    }
}

static void open_difficulty(CidrApp* app, GameMode mode) {
    app->pending_mode = mode;
    app->option_index = app->settings.difficulty;
    app->scene = SceneDifficulty;
}

static void open_menu(CidrApp* app) {
    app->scene = SceneMenu;
    app->stats_cleared = false;
}

static void handle_menu_input(CidrApp* app, InputKey key) {
    switch(key) {
    case InputKeyUp:
        app->menu_index = (app->menu_index + MenuCount - 1) % MenuCount;
        break;
    case InputKeyDown:
        app->menu_index = (app->menu_index + 1) % MenuCount;
        break;
    case InputKeyOk:
    case InputKeyRight:
        switch(app->menu_index) {
        case MenuTraining:
            open_difficulty(app, ModeTraining);
            break;
        case MenuSpeed:
            open_difficulty(app, ModeSpeed);
            break;
        case MenuSurvival:
            open_difficulty(app, ModeSurvival);
            break;
        case MenuExam:
            app->option_index = app->settings.exam_index;
            app->scene = SceneExamSize;
            break;
        case MenuStats:
            app->stats_cleared = false;
            app->scene = SceneStats;
            break;
        case MenuSound:
            app->settings.sound = !app->settings.sound;
            settings_save(&app->settings);
            break;
        default:
            app->scene = SceneAbout;
            break;
        }
        break;
    case InputKeyBack:
        app->running = false;
        break;
    default:
        break;
    }
}

static void handle_option_input(CidrApp* app, InputKey key, uint8_t count) {
    switch(key) {
    case InputKeyUp:
    case InputKeyLeft:
        app->option_index = (app->option_index + count - 1) % count;
        break;
    case InputKeyDown:
    case InputKeyRight:
        app->option_index = (app->option_index + 1) % count;
        break;
    case InputKeyOk:
        if(app->scene == SceneDifficulty) {
            app->settings.difficulty = (Difficulty)app->option_index;
            settings_save(&app->settings);
            session_start(app, app->pending_mode);
        } else {
            app->settings.exam_index = app->option_index;
            settings_save(&app->settings);
            session_start(app, ModeExam);
        }
        break;
    case InputKeyBack:
        open_menu(app);
        break;
    default:
        break;
    }
}

static void handle_quiz_input(CidrApp* app, InputKey key) {
    Session* session = &app->session;

    if(key == InputKeyBack) {
        if(session->asked > 0) {
            session_finish(app);
        } else {
            open_menu(app);
        }
        return;
    }

    if(session->answered) {
        if(key == InputKeyOk || key == InputKeyRight) advance_question(app);
        return;
    }

    switch(key) {
    case InputKeyUp:
    case InputKeyLeft:
        session->selected = (session->selected + QUIZ_ANSWER_COUNT - 1) % QUIZ_ANSWER_COUNT;
        break;
    case InputKeyDown:
    case InputKeyRight:
        session->selected = (session->selected + 1) % QUIZ_ANSWER_COUNT;
        break;
    case InputKeyOk:
        submit_answer(app);
        break;
    default:
        break;
    }
}

static void handle_input(CidrApp* app, const InputEvent* input) {
    if(app->scene == SceneStats && input->key == InputKeyOk && input->type == InputTypeLong) {
        stats_reset(&app->stats);
        stats_save(&app->stats);
        app->stats_cleared = true;
        return;
    }

    // Key repeat is only useful to scroll through lists
    if(input->type == InputTypeRepeat) {
        if(input->key != InputKeyUp && input->key != InputKeyDown) return;
    } else if(input->type != InputTypeShort) {
        return;
    }

    switch(app->scene) {
    case SceneMenu:
        handle_menu_input(app, input->key);
        break;
    case SceneDifficulty:
        handle_option_input(app, input->key, DifficultyCount);
        break;
    case SceneExamSize:
        handle_option_input(app, input->key, COUNT_OF(exam_sizes));
        break;
    case SceneQuiz:
        handle_quiz_input(app, input->key);
        break;
    default:
        if(input->key == InputKeyBack || input->key == InputKeyOk) open_menu(app);
        break;
    }
}

static void handle_tick(CidrApp* app) {
    if(app->scene != SceneQuiz) return;
    Session* session = &app->session;

    if(session->answered) {
        if(furi_get_tick() - session->answer_time >= furi_ms_to_ticks(FEEDBACK_MS)) {
            advance_question(app);
        }
    } else if(session->mode == ModeSpeed && furi_get_tick() >= session->deadline) {
        session_finish(app);
    }
}

static CidrApp* cidr_app_alloc(void) {
    CidrApp* app = malloc(sizeof(CidrApp));
    memset(app, 0, sizeof(CidrApp));

    app->queue = furi_message_queue_alloc(8, sizeof(AppEvent));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    stats_load(&app->stats);
    settings_load(&app->settings);

    app->scene = SceneMenu;
    app->running = true;

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, cidr_draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app->queue);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->timer = furi_timer_alloc(timer_callback, FuriTimerTypePeriodic, app->queue);
    furi_timer_start(app->timer, furi_ms_to_ticks(TICK_PERIOD_MS));

    return app;
}

static void cidr_app_free(CidrApp* app) {
    furi_timer_stop(app->timer);
    furi_timer_free(app->timer);

    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    furi_mutex_free(app->mutex);
    furi_message_queue_free(app->queue);
    free(app);
}

int32_t cidr_challenge_app(void* p) {
    UNUSED(p);

    CidrApp* app = cidr_app_alloc();
    AppEvent event;

    while(app->running) {
        if(furi_message_queue_get(app->queue, &event, FuriWaitForever) != FuriStatusOk) continue;

        bool redraw = true;

        furi_mutex_acquire(app->mutex, FuriWaitForever);
        if(event.type == EventTypeInput) {
            handle_input(app, &event.input);
        } else {
            // Ticks only matter while a countdown runs or feedback is on screen
            redraw = app->scene == SceneQuiz &&
                     (app->session.mode == ModeSpeed || app->session.answered);
            handle_tick(app);
        }
        furi_mutex_release(app->mutex);

        if(redraw) view_port_update(app->view_port);
    }

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    if(app->session.asked > 0) session_finish(app);
    furi_mutex_release(app->mutex);

    cidr_app_free(app);

    return 0;
}
