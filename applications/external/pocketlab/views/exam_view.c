#include "exam_view.h"

#include <furi.h>
#include <furi_hal_random.h>
#include <gui/elements.h>

#include "../helpers/pocketlab_content.h"
#include "../helpers/pocketlab_sound.h"

#define EXAM_ANIM_MS  90
#define EXAM_MAX      10 // questions per exam
#define EXAM_POOL_MAX 128 // upper bound on quizzes across all labs

// Random praise shown on a correct answer.
static const char* const exam_praises[] = {
    "Correct!",
    "Great job!",
    "Nailed it!",
    "Genius!",
    "Spot on!",
    "Brilliant!",
    "You rock!",
    "Boom!",
    "Perfect!",
    "Too easy!",
};
#define EXAM_PRAISE_COUNT COUNT_OF(exam_praises)

struct ExamView {
    View* view;
    FuriTimer* timer;
    NotificationApp* notifications;
    bool sound_enabled;
    ExamViewDoneCallback done_callback;
    void* done_context;
};

typedef struct {
    const PocketLabStep* questions[EXAM_MAX];
    uint8_t count; // number of questions in this exam
    uint8_t current; // question index
    uint8_t score;
    uint8_t quiz_sel;
    bool feedback;
    bool last_right;
    bool finished;
    const char* options[POCKETLAB_QUIZ_OPTIONS];
    uint8_t answer;
    uint32_t anim;
    uint8_t shake;
    uint8_t praise; // index into exam_praises for the current correct screen
    uint8_t fb_frame; // frames since the feedback screen opened (for confetti)
} ExamModel;

static uint32_t exam_rand(uint32_t max) {
    return furi_hal_random_get() % max;
}

// Pick the correct answer plus two distractors and shuffle them.
static void exam_view_prepare(ExamModel* model) {
    if(model->current >= model->count) return;
    const PocketLabStep* step = model->questions[model->current];

    const uint8_t dcount = step->distractor_count;
    uint8_t first = exam_rand(dcount);
    uint8_t second = exam_rand(dcount - 1);
    if(second >= first) second++;

    const char* options[POCKETLAB_QUIZ_OPTIONS] = {
        step->correct,
        step->distractors[first],
        step->distractors[second],
    };
    uint8_t correct = 0;

    for(uint8_t i = POCKETLAB_QUIZ_OPTIONS - 1; i > 0; i--) {
        const uint8_t j = exam_rand(i + 1);
        const char* tmp = options[i];
        options[i] = options[j];
        options[j] = tmp;
        if(correct == i) {
            correct = j;
        } else if(correct == j) {
            correct = i;
        }
    }

    for(uint8_t i = 0; i < POCKETLAB_QUIZ_OPTIONS; i++) {
        model->options[i] = options[i];
    }
    model->answer = correct;
    model->quiz_sel = 0;
    model->feedback = false;
}

static void exam_view_draw_chevron(Canvas* canvas, uint32_t anim) {
    if((anim / 5) % 2 != 0) return;
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 126, 62, AlignRight, AlignBottom, ">");
}

static void exam_view_draw_empty(Canvas* canvas) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 22, AlignCenter, AlignCenter, "No quizzes yet");
    canvas_set_font(canvas, FontSecondary);
    elements_text_box(
        canvas,
        4,
        32,
        120,
        28,
        AlignCenter,
        AlignTop,
        "Complete some labs to unlock the quiz.",
        false);
}

static void exam_view_draw_result(Canvas* canvas, const ExamModel* model) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 12, AlignCenter, AlignTop, "Quiz complete!");

    char line[20];
    snprintf(line, sizeof(line), "%u/%u", model->score, model->count);
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignCenter, line);

    const char* verdict;
    const uint8_t pct = model->count ? (uint16_t)model->score * 100 / model->count : 0;
    if(pct == 100) {
        verdict = "Perfect!";
    } else if(pct >= 70) {
        verdict = "Great job!";
    } else if(pct >= 40) {
        verdict = "Keep going!";
    } else {
        verdict = "Try again!";
    }
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 52, AlignCenter, AlignBottom, verdict);
    exam_view_draw_chevron(canvas, model->anim);
}

// A tiny burst of confetti from (cx, cy) over the first few frames.
static void exam_view_draw_confetti(Canvas* canvas, int cx, int cy, uint8_t f) {
    static const int8_t vx[8] = {-3, -2, -1, 1, 2, 3, -4, 4};
    for(uint8_t i = 0; i < 8; i++) {
        const int px = cx + vx[i] * f / 2;
        const int py = cy - 8 + f - 6 + (i % 3) * 3; // up, then settling down
        canvas_draw_dot(canvas, px, py);
    }
}

static void exam_view_draw_feedback(Canvas* canvas, const ExamModel* model) {
    if(model->last_right) {
        if(model->fb_frame < 12) exam_view_draw_confetti(canvas, 64, 30, model->fb_frame);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(
            canvas, 64, 30, AlignCenter, AlignCenter, exam_praises[model->praise]);
    } else {
        // "Wrong" and the answer both centred on the screen.
        const int dx = (model->shake > 0) ? ((model->shake & 1) ? 3 : -3) : 0;
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64 + dx, 20, AlignCenter, AlignCenter, "Wrong");
        canvas_set_font(canvas, FontSecondary);
        char ans[48];
        snprintf(ans, sizeof(ans), "Answer: %s", model->options[model->answer]);
        elements_text_box(canvas, 4, 30, 120, 28, AlignCenter, AlignTop, ans, false);
    }
    exam_view_draw_chevron(canvas, model->anim);
}

static void exam_view_draw_question(Canvas* canvas, const ExamModel* model) {
    const PocketLabStep* step = model->questions[model->current];

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, "Quiz");
    char pos[12];
    snprintf(pos, sizeof(pos), "%u/%u", model->current + 1, model->count);
    canvas_draw_str_aligned(canvas, 126, 8, AlignRight, AlignBottom, pos);

    if(model->feedback) {
        exam_view_draw_feedback(canvas, model);
        return;
    }

    // Question in bold so it stands out from the answers (text box uses \e#).
    char q[80];
    snprintf(q, sizeof(q), "\e#%s\e#", step->title);
    elements_text_box(canvas, 2, 11, 124, 20, AlignLeft, AlignTop, q, false);

    // Answers, each with an "A." / "B." / "C." marker and a little padding.
    canvas_set_font(canvas, FontSecondary);
    for(uint8_t i = 0; i < POCKETLAB_QUIZ_OPTIONS; i++) {
        const uint8_t y = 39 + i * 11;
        if(i == model->quiz_sel) {
            canvas_draw_box(canvas, 0, y - 9, 128, 12);
            canvas_set_color(canvas, ColorWhite);
        }
        const char marker[3] = {(char)('A' + i), '.', '\0'};
        canvas_draw_str(canvas, 4, y, marker);
        canvas_draw_str(canvas, 15, y, model->options[i]);
        canvas_set_color(canvas, ColorBlack);
    }
}

static void exam_view_draw_callback(Canvas* canvas, void* context) {
    ExamModel* model = context;
    canvas_clear(canvas);

    if(model->count == 0) {
        exam_view_draw_empty(canvas);
    } else if(model->finished) {
        exam_view_draw_result(canvas, model);
    } else {
        exam_view_draw_question(canvas, model);
    }
}

static bool exam_view_input_callback(InputEvent* event, void* context) {
    ExamView* instance = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) {
        return false;
    }

    bool consumed = false;
    bool leave = false;
    bool play_sound = false;
    PocketLabSoundId sound = PocketLabSoundClick;

    with_view_model(
        instance->view,
        ExamModel * model,
        {
            if(model->count == 0) {
                if(event->key == InputKeyOk) {
                    leave = true;
                    consumed = true;
                }
            } else if(model->finished) {
                if(event->key == InputKeyOk || event->key == InputKeyRight) {
                    leave = true;
                    consumed = true;
                }
            } else if(model->feedback) {
                if(event->key == InputKeyOk || event->key == InputKeyRight) {
                    if(model->current + 1 < model->count) {
                        model->current++;
                        exam_view_prepare(model);
                        play_sound = true;
                        sound = PocketLabSoundClick;
                    } else {
                        model->finished = true;
                        play_sound = true;
                        sound = PocketLabSoundComplete;
                    }
                    consumed = true;
                }
            } else {
                if(event->key == InputKeyUp) {
                    model->quiz_sel =
                        (model->quiz_sel + POCKETLAB_QUIZ_OPTIONS - 1) % POCKETLAB_QUIZ_OPTIONS;
                    play_sound = true;
                    sound = PocketLabSoundType; // quiet navigation tick
                    consumed = true;
                } else if(event->key == InputKeyDown) {
                    model->quiz_sel = (model->quiz_sel + 1) % POCKETLAB_QUIZ_OPTIONS;
                    play_sound = true;
                    sound = PocketLabSoundType;
                    consumed = true;
                } else if(event->key == InputKeyOk) {
                    model->feedback = true;
                    model->last_right = model->quiz_sel == model->answer;
                    model->fb_frame = 0;
                    if(model->last_right) {
                        model->score++;
                        model->praise = (uint8_t)exam_rand(EXAM_PRAISE_COUNT);
                    } else {
                        model->shake = 6;
                    }
                    play_sound = true;
                    sound = model->last_right ? PocketLabSoundCorrect : PocketLabSoundWrong;
                    consumed = true;
                }
            }
        },
        true);

    if(play_sound) {
        pocketlab_sound_play(instance->notifications, instance->sound_enabled, sound);
    }
    if(leave && instance->done_callback) {
        instance->done_callback(instance->done_context);
    }

    return consumed;
}

static void exam_view_timer_callback(void* context) {
    ExamView* instance = context;
    with_view_model(
        instance->view,
        ExamModel * model,
        {
            model->anim++;
            if(model->shake > 0) model->shake--;
            if(model->feedback && model->fb_frame < 250) model->fb_frame++;
        },
        true);
}

static void exam_view_enter_callback(void* context) {
    ExamView* instance = context;
    furi_timer_start(instance->timer, furi_ms_to_ticks(EXAM_ANIM_MS));
}

static void exam_view_exit_callback(void* context) {
    ExamView* instance = context;
    furi_timer_stop(instance->timer);
}

ExamView* exam_view_alloc(void) {
    ExamView* instance = malloc(sizeof(ExamView));
    instance->view = view_alloc();
    instance->notifications = NULL;
    instance->sound_enabled = false;
    instance->done_callback = NULL;
    instance->done_context = NULL;

    view_set_context(instance->view, instance);
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(ExamModel));
    view_set_draw_callback(instance->view, exam_view_draw_callback);
    view_set_input_callback(instance->view, exam_view_input_callback);
    view_set_enter_callback(instance->view, exam_view_enter_callback);
    view_set_exit_callback(instance->view, exam_view_exit_callback);

    instance->timer = furi_timer_alloc(exam_view_timer_callback, FuriTimerTypePeriodic, instance);

    return instance;
}

void exam_view_free(ExamView* instance) {
    furi_assert(instance);
    furi_timer_stop(instance->timer);
    furi_timer_free(instance->timer);
    view_free(instance->view);
    free(instance);
}

View* exam_view_get_view(ExamView* instance) {
    furi_assert(instance);
    return instance->view;
}

void exam_view_set_done_callback(ExamView* instance, ExamViewDoneCallback callback, void* context) {
    furi_assert(instance);
    instance->done_callback = callback;
    instance->done_context = context;
}

void exam_view_configure(
    ExamView* instance,
    uint64_t completed_mask,
    bool sound_enabled,
    NotificationApp* notifications) {
    furi_assert(instance);
    instance->notifications = notifications;
    instance->sound_enabled = sound_enabled;

    // Gather every quiz step from the completed labs.
    const PocketLabStep* pool[EXAM_POOL_MAX];
    uint16_t pool_count = 0;
    for(size_t i = 0; i < pocketlab_labs_count && pool_count < EXAM_POOL_MAX; i++) {
        if(!(completed_mask & (1ULL << i))) continue;
        const PocketLabLab* lab = &pocketlab_labs[i];
        for(size_t s = 0; s < lab->step_count && pool_count < EXAM_POOL_MAX; s++) {
            if(lab->steps[s].type == PocketLabStepQuiz) {
                pool[pool_count++] = &lab->steps[s];
            }
        }
    }

    // Fisher-Yates shuffle, then keep the first EXAM_MAX.
    for(uint16_t i = pool_count; i > 1; i--) {
        const uint16_t j = (uint16_t)exam_rand(i);
        const PocketLabStep* tmp = pool[i - 1];
        pool[i - 1] = pool[j];
        pool[j] = tmp;
    }

    with_view_model(
        instance->view,
        ExamModel * model,
        {
            model->count = (uint8_t)(pool_count < EXAM_MAX ? pool_count : EXAM_MAX);
            for(uint8_t i = 0; i < model->count; i++) {
                model->questions[i] = pool[i];
            }
            model->current = 0;
            model->score = 0;
            model->quiz_sel = 0;
            model->feedback = false;
            model->last_right = false;
            model->finished = false;
            model->anim = 0;
            model->shake = 0;
            model->praise = 0;
            model->fb_frame = 0;
            if(model->count > 0) exam_view_prepare(model);
        },
        true);
}
