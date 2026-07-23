#include "quiz_view.h"

#include <furi.h>
#include <gui/elements.h>
#include <input/input.h>

#include "../ear_events.h"

/* Answer grid geometry: five columns of short names ("P5", "m3") is enough
 * for all thirteen intervals in three rows. */
#define GRID_COLS 5
#define CELL_W    25
#define CELL_H    12
#define GRID_X    1
#define GRID_Y    17

struct QuizView {
    View* view;
    void (*callback)(void*, uint32_t);
    void* context;
};

static void quiz_view_draw(Canvas* canvas, void* model_ptr) {
    const QuizModel* model = model_ptr;
    char buf[40];

    canvas_clear(canvas);

    if(model->phase == QuizPhaseFeedback) {
        const IntervalInfo* info = interval_get(model->correct_interval);

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(
            canvas, 64, 12, AlignCenter, AlignBottom, model->last_correct ? "Correct" : "Nope");

        canvas_set_font(canvas, FontSecondary);
        snprintf(buf, sizeof(buf), "%s (%s)", info->name, info->shortname);
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignCenter, buf);

        if(model->show_mnemonic) {
            canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignCenter, info->mnemonic);
        }

        if(model->last_correct && model->streak > 1) {
            snprintf(buf, sizeof(buf), "streak %u", model->streak);
            canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, buf);
        }
        return;
    }

    /* header: progress, score and remaining hints */
    canvas_set_font(canvas, FontSecondary);
    snprintf(buf, sizeof(buf), "%u/%u", model->question, model->total);
    canvas_draw_str(canvas, 2, 9, buf);

    if(model->challenge) {
        snprintf(buf, sizeof(buf), "lives %u", model->mistakes_left);
    } else {
        snprintf(buf, sizeof(buf), "%u right", model->score);
    }
    canvas_draw_str_aligned(canvas, 64, 9, AlignCenter, AlignBottom, buf);

    snprintf(buf, sizeof(buf), "hint %u", model->hints_left);
    canvas_draw_str_aligned(canvas, 126, 9, AlignRight, AlignBottom, buf);
    canvas_draw_line(canvas, 0, 11, 127, 11);

    /* answer grid */
    for(uint8_t i = 0; i < model->choice_count; i++) {
        uint8_t col = i % GRID_COLS;
        uint8_t row = i / GRID_COLS;
        uint8_t x = GRID_X + col * CELL_W;
        uint8_t y = GRID_Y + row * CELL_H;
        const char* label = interval_get(model->choices[i])->shortname;

        bool selected = (i == model->selected);
        if(selected) canvas_draw_box(canvas, x, y, CELL_W - 2, CELL_H - 1);
        canvas_set_color(canvas, selected ? ColorWhite : ColorBlack);
        canvas_draw_str_aligned(
            canvas, x + (CELL_W - 2) / 2, y + CELL_H - 3, AlignCenter, AlignBottom, label);
        if(model->eliminated[i]) {
            /* struck out by a hint, but still drawn so the layout is stable */
            canvas_draw_line(canvas, x + 2, y + CELL_H / 2, x + CELL_W - 5, y + CELL_H / 2);
        }
        canvas_set_color(canvas, ColorBlack);
    }

    /* footer */
    if(model->quit_armed) {
        canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "Back again to quit");
    } else {
        canvas_draw_str(canvas, 2, 63, "OK pick");
        canvas_draw_str_aligned(canvas, 126, 63, AlignRight, AlignBottom, "^ replay  v hint");
    }
}

static bool quiz_view_input(InputEvent* event, void* context) {
    QuizView* quiz_view = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;
    if(event->key == InputKeyBack) return false; /* let the scene manager handle it */

    /* The scene owns every piece of quiz state, including which answer is
     * highlighted; the view only reports what was pressed. Keeping one owner
     * avoids the two copies of `selected` drifting apart. */
    bool answering = false;
    with_view_model(
        quiz_view->view,
        QuizModel * m,
        { answering = (m->phase == QuizPhaseAnswering && m->choice_count > 0); },
        false);

    if(!answering) return true; /* swallow input while feedback is up */

    uint32_t emit = 0;
    switch(event->key) {
    case InputKeyLeft:
        emit = ETEventPrev;
        break;
    case InputKeyRight:
        emit = ETEventNext;
        break;
    case InputKeyUp:
        emit = ETEventReplay;
        break;
    case InputKeyDown:
        emit = ETEventHint;
        break;
    case InputKeyOk:
        emit = ETEventAnswer;
        break;
    default:
        return false;
    }

    if(quiz_view->callback) quiz_view->callback(quiz_view->context, emit);
    return true;
}

QuizView* quiz_view_alloc(void) {
    QuizView* quiz_view = malloc(sizeof(QuizView));
    quiz_view->callback = NULL;
    quiz_view->context = NULL;
    quiz_view->view = view_alloc();
    view_allocate_model(quiz_view->view, ViewModelTypeLocking, sizeof(QuizModel));
    view_set_context(quiz_view->view, quiz_view);
    view_set_draw_callback(quiz_view->view, quiz_view_draw);
    view_set_input_callback(quiz_view->view, quiz_view_input);
    return quiz_view;
}

void quiz_view_free(QuizView* quiz_view) {
    view_free(quiz_view->view);
    free(quiz_view);
}

View* quiz_view_get_view(QuizView* quiz_view) {
    return quiz_view->view;
}

void quiz_view_set_callback(QuizView* quiz_view, void (*callback)(void*, uint32_t), void* context) {
    quiz_view->callback = callback;
    quiz_view->context = context;
}

void quiz_view_update(QuizView* quiz_view, const QuizModel* incoming) {
    with_view_model(quiz_view->view, QuizModel * m, { *m = *incoming; }, true);
}
