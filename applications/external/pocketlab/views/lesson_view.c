#include "lesson_view.h"

#include <furi.h>
#include <furi_hal_random.h>
#include <gui/elements.h>

#include "../helpers/pocketlab_sound.h"

#define LESSON_ANIM_PERIOD_MS 90

struct LessonView {
    View* view;
    FuriTimer* timer;
    NotificationApp* notifications;
    bool sound_enabled;
    LessonViewDoneCallback done_callback;
    void* done_context;
};

typedef struct {
    const PocketLabLab* lab;
    bool already_completed;
    size_t step;
    uint8_t quiz_sel;
    bool feedback;
    bool last_right;
    uint32_t anim;
    uint8_t shake; // countdown for the wrong-answer shake
    uint32_t xp_shown;
    const char* quiz_options[POCKETLAB_QUIZ_OPTIONS];
    uint8_t quiz_answer;
} LessonModel;

static uint32_t lesson_view_rand(uint32_t max) {
    return furi_hal_random_get() % max;
}

// When the current step is a quiz, pick the correct answer plus two random
// distractors and shuffle them, so the layout differs every time.
static void lesson_view_prepare_quiz(LessonModel* model) {
    const PocketLabStep* step = &model->lab->steps[model->step];
    if(step->type != PocketLabStepQuiz) {
        return;
    }

    const uint8_t count = step->distractor_count;
    uint8_t first = lesson_view_rand(count);
    uint8_t second = lesson_view_rand(count - 1);
    if(second >= first) {
        second++;
    }

    const char* options[POCKETLAB_QUIZ_OPTIONS] = {
        step->correct,
        step->distractors[first],
        step->distractors[second],
    };
    uint8_t correct = 0;

    for(uint8_t i = POCKETLAB_QUIZ_OPTIONS - 1; i > 0; i--) {
        const uint8_t j = lesson_view_rand(i + 1);
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
        model->quiz_options[i] = options[i];
    }
    model->quiz_answer = correct;
    model->quiz_sel = 0;
    model->feedback = false;
}

// Step progress dots, pinned to the bottom-left corner next to the chevron.
static void lesson_view_draw_dots(Canvas* canvas, const LessonModel* model) {
    const uint8_t count = model->lab->step_count;
    const uint8_t cy = 58;
    for(uint8_t i = 0; i < count; i++) {
        const uint8_t x = 5 + i * 6;
        if(i < model->step) {
            canvas_draw_disc(canvas, x, cy, 1);
        } else if(i == model->step) {
            canvas_draw_disc(canvas, x, cy, 2);
        } else {
            canvas_draw_circle(canvas, x, cy, 1);
        }
    }
}

static void lesson_view_draw_icon(Canvas* canvas, const uint16_t* rows, uint8_t x, uint8_t y) {
    for(uint8_t r = 0; r < 10; r++) {
        for(uint8_t c = 0; c < 10; c++) {
            if(rows[r] & (1u << c)) {
                canvas_draw_dot(canvas, x + c, y + r);
            }
        }
    }
}

static void lesson_view_draw_chevron(Canvas* canvas, uint32_t anim) {
    if((anim / 5) % 2 != 0) {
        return;
    }
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 126, 62, AlignRight, AlignBottom, ">");
}

static void lesson_view_draw_text_step(
    Canvas* canvas,
    const LessonModel* model,
    const PocketLabStep* step) {
    lesson_view_draw_icon(canvas, model->lab->icon, 2, 3);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 15, 12, step->title);
    elements_text_box(canvas, 2, 18, 124, 36, AlignLeft, AlignTop, step->body, false);
    lesson_view_draw_chevron(canvas, model->anim);
}

static void
    lesson_view_draw_quiz(Canvas* canvas, const LessonModel* model, const PocketLabStep* step) {
    lesson_view_draw_icon(canvas, model->lab->icon, 2, 3);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 15, 12, step->title);

    canvas_set_font(canvas, FontSecondary);
    for(uint8_t i = 0; i < POCKETLAB_QUIZ_OPTIONS; i++) {
        const uint8_t y = 28 + i * 11;
        if(i == model->quiz_sel) {
            canvas_draw_box(canvas, 0, y - 9, 128, 11);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str(canvas, 4, y, model->quiz_options[i]);
        canvas_set_color(canvas, ColorBlack);
    }
}

// A bold check mark with its lower tip at (x, y).
static void lesson_view_draw_check(Canvas* canvas, uint8_t x, uint8_t y) {
    canvas_draw_line(canvas, x - 3, y - 3, x, y);
    canvas_draw_line(canvas, x, y, x + 6, y - 6);
    canvas_draw_line(canvas, x - 3, y - 4, x, y - 1); // thickness
    canvas_draw_line(canvas, x, y - 1, x + 6, y - 7);
}

static void
    lesson_view_draw_feedback(Canvas* canvas, const LessonModel* model, const PocketLabStep* step) {
    // On a wrong answer the title jitters left/right for a few frames.
    const int dx = (model->shake > 0) ? ((model->shake & 1) ? 3 : -3) : 0;
    canvas_set_font(canvas, FontPrimary);
    if(model->last_right) {
        lesson_view_draw_check(canvas, 5, 11);
        canvas_draw_str(canvas, 15, 12, pocketlab_text(PocketLabTextCorrect));
    } else {
        canvas_draw_str(canvas, 2 + dx, 12, pocketlab_text(PocketLabTextTryAgain));
    }
    elements_text_box(
        canvas,
        2,
        18,
        124,
        36,
        AlignLeft,
        AlignTop,
        model->last_right ? step->feedback_ok : step->feedback_no,
        false);
    lesson_view_draw_chevron(canvas, model->anim);
}

static void lesson_view_draw_medal(Canvas* canvas, uint8_t cx, uint8_t cy, uint32_t anim) {
    const uint8_t radius = 6 + (anim % 3);
    canvas_draw_disc(canvas, cx, cy, radius);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_disc(canvas, cx, cy, 2);
    canvas_set_color(canvas, ColorBlack);
}

static void lesson_view_draw_reward(Canvas* canvas, const LessonModel* model) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(
        canvas, 64, 14, AlignCenter, AlignBottom, pocketlab_text(PocketLabTextLabComplete));

    char xp_line[16];
    const char* line1;
    const char* line2;
    if(model->already_completed) {
        line1 = "Reviewed!";
        line2 = "Nice work!";
    } else {
        snprintf(xp_line, sizeof(xp_line), "+%lu XP", (unsigned long)model->xp_shown);
        line1 = xp_line;
        line2 = model->lab->badge;
    }

    // Center the medal + two text lines as one group, whatever the text length.
    canvas_set_font(canvas, FontSecondary);
    const uint16_t width_1 = canvas_string_width(canvas, line1);
    const uint16_t width_2 = canvas_string_width(canvas, line2);
    const uint16_t text_width = width_1 > width_2 ? width_1 : width_2;

    const uint8_t medal_slot = 18;
    const uint8_t gap = 6;
    const uint8_t start_x = (128 - (medal_slot + gap + text_width)) / 2;
    const uint8_t text_x = start_x + medal_slot + gap;

    lesson_view_draw_medal(canvas, start_x + medal_slot / 2, 37, model->anim);
    canvas_draw_str(canvas, text_x, 35, line1);
    canvas_draw_str(canvas, text_x, 45, line2);

    lesson_view_draw_chevron(canvas, model->anim);
}

static void lesson_view_draw_callback(Canvas* canvas, void* context) {
    LessonModel* model = context;
    if(model->lab == NULL) {
        return;
    }

    canvas_clear(canvas);
    lesson_view_draw_dots(canvas, model);

    const PocketLabStep* step = &model->lab->steps[model->step];
    switch(step->type) {
    case PocketLabStepQuiz:
        if(model->feedback) {
            lesson_view_draw_feedback(canvas, model, step);
        } else {
            lesson_view_draw_quiz(canvas, model, step);
        }
        break;
    case PocketLabStepReward:
        lesson_view_draw_reward(canvas, model);
        break;
    default:
        lesson_view_draw_text_step(canvas, model, step);
        break;
    }
}

static bool lesson_view_input_callback(InputEvent* event, void* context) {
    LessonView* instance = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) {
        return false;
    }

    bool consumed = false;
    bool finished = false;
    bool play_sound = false;
    PocketLabSoundId sound = PocketLabSoundClick;

    with_view_model(
        instance->view,
        LessonModel * model,
        {
            const PocketLabStep* step = &model->lab->steps[model->step];

            if(step->type == PocketLabStepQuiz && !model->feedback) {
                if(event->key == InputKeyUp) {
                    model->quiz_sel =
                        (model->quiz_sel + POCKETLAB_QUIZ_OPTIONS - 1) % POCKETLAB_QUIZ_OPTIONS;
                    consumed = true;
                } else if(event->key == InputKeyDown) {
                    model->quiz_sel = (model->quiz_sel + 1) % POCKETLAB_QUIZ_OPTIONS;
                    consumed = true;
                } else if(event->key == InputKeyOk) {
                    model->feedback = true;
                    model->last_right = model->quiz_sel == model->quiz_answer;
                    if(!model->last_right) model->shake = 6;
                    play_sound = true;
                    sound = model->last_right ? PocketLabSoundCorrect : PocketLabSoundWrong;
                    consumed = true;
                }
            } else if(step->type == PocketLabStepQuiz && model->feedback) {
                if(event->key == InputKeyOk || event->key == InputKeyRight) {
                    if(model->last_right) {
                        model->step++;
                        model->feedback = false;
                        model->quiz_sel = 0;
                        lesson_view_prepare_quiz(model);
                        play_sound = true;
                        sound = model->lab->steps[model->step].type == PocketLabStepReward ?
                                    PocketLabSoundComplete :
                                    PocketLabSoundClick;
                    } else {
                        model->feedback = false;
                    }
                    consumed = true;
                }
            } else if(step->type == PocketLabStepReward) {
                if(event->key == InputKeyOk || event->key == InputKeyRight) {
                    finished = true;
                    consumed = true;
                }
            } else {
                if(event->key == InputKeyOk || event->key == InputKeyRight) {
                    model->step++;
                    lesson_view_prepare_quiz(model);
                    play_sound = true;
                    sound = model->lab->steps[model->step].type == PocketLabStepReward ?
                                PocketLabSoundComplete :
                                PocketLabSoundClick;
                    consumed = true;
                }
            }
        },
        true);

    if(play_sound) {
        pocketlab_sound_play(instance->notifications, instance->sound_enabled, sound);
    }
    if(finished && instance->done_callback) {
        instance->done_callback(instance->done_context);
    }

    return consumed;
}

static void lesson_view_timer_callback(void* context) {
    LessonView* instance = context;
    with_view_model(
        instance->view,
        LessonModel * model,
        {
            model->anim++;
            if(model->shake > 0) model->shake--;
            const PocketLabStep* step = &model->lab->steps[model->step];
            if(step->type == PocketLabStepReward && !model->already_completed &&
               model->xp_shown < model->lab->xp) {
                const uint32_t remaining = model->lab->xp - model->xp_shown;
                model->xp_shown += (remaining + 5) / 6;
            }
        },
        true);
}

static void lesson_view_enter_callback(void* context) {
    LessonView* instance = context;
    furi_timer_start(instance->timer, furi_ms_to_ticks(LESSON_ANIM_PERIOD_MS));
}

static void lesson_view_exit_callback(void* context) {
    LessonView* instance = context;
    furi_timer_stop(instance->timer);
}

LessonView* lesson_view_alloc(void) {
    LessonView* instance = malloc(sizeof(LessonView));
    instance->view = view_alloc();
    instance->notifications = NULL;
    instance->sound_enabled = false;
    instance->done_callback = NULL;
    instance->done_context = NULL;

    view_set_context(instance->view, instance);
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(LessonModel));
    view_set_draw_callback(instance->view, lesson_view_draw_callback);
    view_set_input_callback(instance->view, lesson_view_input_callback);
    view_set_enter_callback(instance->view, lesson_view_enter_callback);
    view_set_exit_callback(instance->view, lesson_view_exit_callback);

    instance->timer =
        furi_timer_alloc(lesson_view_timer_callback, FuriTimerTypePeriodic, instance);

    return instance;
}

void lesson_view_free(LessonView* instance) {
    furi_assert(instance);
    furi_timer_stop(instance->timer);
    furi_timer_free(instance->timer);
    view_free(instance->view);
    free(instance);
}

View* lesson_view_get_view(LessonView* instance) {
    furi_assert(instance);
    return instance->view;
}

void lesson_view_set_done_callback(
    LessonView* instance,
    LessonViewDoneCallback callback,
    void* context) {
    furi_assert(instance);
    instance->done_callback = callback;
    instance->done_context = context;
}

void lesson_view_configure(
    LessonView* instance,
    const PocketLabLab* lab,
    bool already_completed,
    bool sound_enabled,
    NotificationApp* notifications) {
    furi_assert(instance);
    instance->notifications = notifications;
    instance->sound_enabled = sound_enabled;

    with_view_model(
        instance->view,
        LessonModel * model,
        {
            model->lab = lab;
            model->already_completed = already_completed;
            model->step = 0;
            model->quiz_sel = 0;
            model->feedback = false;
            model->last_right = false;
            model->anim = 0;
            model->shake = 0;
            model->xp_shown = 0;
            lesson_view_prepare_quiz(model);
        },
        true);
}
