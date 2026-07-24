#include "quiz_view.h"

#include <furi.h>
#include <gui/elements.h>
#include <input/input.h>

#include "../ear_events.h"

/* --- piano keyboard ---------------------------------------------------- */

/* Two octaves of white keys fill the screen exactly: 15 * 8 = 120 px, with a
 * 4 px margin either side. Notes are drawn relative to the C below the
 * lowest one played, so a phrase always lands inside the drawn range. */
#define KEY_W      8
#define WHITE_KEYS 15
#define KB_X       ((128 - WHITE_KEYS * KEY_W) / 2)
#define WHITE_H    24
#define BLACK_H    13
/* narrow enough to leave clear white either side; at 6 px on an 8 px pitch
 * the black keys ran together into one dark band */
#define BLACK_W    5

/* position of each semitone among the white keys, or -1 when it is a black */
static const int8_t white_pos[12] = {0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6};
/* which white key a black key sits after */
static const int8_t black_after[12] = {-1, 0, -1, 1, -1, -1, 3, -1, 4, -1, 5, -1};

static bool semitone_is_black(uint8_t semitone) {
    return white_pos[semitone % 12] < 0;
}

static uint8_t white_key_x(uint8_t octave, uint8_t semitone) {
    return KB_X + (uint8_t)((octave * 7 + white_pos[semitone]) * KEY_W);
}

static uint8_t black_key_x(uint8_t octave, uint8_t semitone) {
    /* straddles the gap between the two white keys it sits between */
    return KB_X + (uint8_t)((octave * 7 + black_after[semitone] + 1) * KEY_W) - BLACK_W / 2;
}

void piano_draw(Canvas* canvas, uint8_t y, const uint8_t* notes, uint8_t count) {
    if(count == 0) return;

    uint8_t lowest = notes[0];
    for(uint8_t i = 1; i < count; i++)
        if(notes[i] < lowest) lowest = notes[i];
    uint8_t base_c = (uint8_t)((lowest / 12) * 12); /* the C at or below it */

    /* white keys first, so black keys draw on top */
    for(uint8_t i = 0; i < WHITE_KEYS; i++) {
        canvas_draw_frame(canvas, KB_X + i * KEY_W, y, KEY_W + 1, WHITE_H);
    }

    for(uint8_t i = 0; i < count; i++) {
        if(notes[i] < base_c) continue;
        uint8_t offset = (uint8_t)(notes[i] - base_c);
        if(offset > 24) continue; /* outside the two octaves drawn */
        uint8_t octave = offset / 12;
        uint8_t semitone = offset % 12;
        if(semitone_is_black(semitone)) continue;
        /* fill the exposed lower part of the key, below where the black keys
         * end, so a lit note reads at a glance */
        canvas_draw_box(
            canvas,
            white_key_x(octave, semitone) + 1,
            y + BLACK_H + 1,
            KEY_W - 1,
            WHITE_H - BLACK_H - 2);
    }

    /* black keys */
    for(uint8_t i = 0; i < WHITE_KEYS; i++) {
        uint8_t octave = i / 7;
        uint8_t white_index = i % 7;
        static const uint8_t black_for_white[7] = {1, 3, 0, 6, 8, 10, 0};
        if(black_for_white[white_index] == 0) continue; /* no black after E or B */
        uint8_t semitone = black_for_white[white_index];
        if(octave * 12 + semitone > 24) continue;
        canvas_draw_box(canvas, black_key_x(octave, semitone), y, BLACK_W, BLACK_H);
    }

    /* played black keys get a notch cut out of them so they read as lit */
    for(uint8_t i = 0; i < count; i++) {
        if(notes[i] < base_c) continue;
        uint8_t offset = (uint8_t)(notes[i] - base_c);
        if(offset > 24) continue;
        uint8_t octave = offset / 12;
        uint8_t semitone = offset % 12;
        if(!semitone_is_black(semitone)) continue;
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(
            canvas, black_key_x(octave, semitone) + 1, y + BLACK_H - 6, BLACK_W - 2, 4);
        canvas_set_color(canvas, ColorBlack);
    }
}

/* --- answer grid ------------------------------------------------------- */

struct QuizView {
    View* view;
    void (*callback)(void*, uint32_t);
    void* context;
};

/* the band between the header rule and the footer hints */
#define GRID_TOP    13
#define GRID_BOTTOM 55
#define CELL_H      12

/* Interval short names are two characters, scale names up to five, so the
 * column count follows the widest label rather than being fixed. */
static uint8_t grid_columns(Canvas* canvas, const QuizModel* model) {
    uint8_t widest = 0;
    for(uint8_t i = 0; i < model->choice_count; i++) {
        uint16_t w =
            canvas_string_width(canvas, content_shortname(model->mode, model->choices[i]));
        if(w > widest) widest = (uint8_t)w;
    }
    uint8_t cell = (uint8_t)(widest + 6);
    uint8_t cols = (uint8_t)(128 / (cell ? cell : 1));
    if(cols > 5) cols = 5;
    if(cols < 3) cols = 3;
    return cols;
}

static void draw_answering(Canvas* canvas, const QuizModel* model) {
    char buf[32];

    /* header: a progress bar beats a bare "3/11" for showing how far in you are */
    canvas_set_font(canvas, FontSecondary);
    uint8_t done = model->question > 0 ? (uint8_t)(model->question - 1) : 0;
    uint8_t bar_w = 58;
    canvas_draw_frame(canvas, 2, 2, bar_w, 7);
    if(model->total) {
        uint8_t fill = (uint8_t)((done * (bar_w - 2)) / model->total);
        if(fill) canvas_draw_box(canvas, 3, 3, fill, 5);
    }
    snprintf(buf, sizeof(buf), "%u/%u", model->question, model->total);
    canvas_draw_str(canvas, bar_w + 6, 9, buf);

    if(model->challenge) {
        snprintf(buf, sizeof(buf), "<3 %u", model->mistakes_left);
    } else {
        snprintf(buf, sizeof(buf), "%u ok", model->score);
    }
    canvas_draw_str_aligned(canvas, 126, 9, AlignRight, AlignBottom, buf);
    canvas_draw_line(canvas, 0, 11, 127, 11);

    /* Answer grid, centred both ways. Each row is centred on its own width so
     * a short last row sits under the middle of the row above rather than
     * hanging off to the left. */
    uint8_t cols = grid_columns(canvas, model);
    uint8_t cell_w = (uint8_t)(128 / cols);
    uint8_t rows = (uint8_t)((model->choice_count + cols - 1) / cols);
    uint8_t grid_h = (uint8_t)(rows * CELL_H);
    uint8_t grid_y = GRID_TOP;
    if(grid_h < (GRID_BOTTOM - GRID_TOP)) {
        grid_y = (uint8_t)(GRID_TOP + (GRID_BOTTOM - GRID_TOP - grid_h) / 2);
    }

    for(uint8_t i = 0; i < model->choice_count; i++) {
        uint8_t col = i % cols;
        uint8_t row = i / cols;
        uint8_t in_row = (uint8_t)(model->choice_count - row * cols);
        if(in_row > cols) in_row = cols;
        uint8_t x = (uint8_t)((128 - in_row * cell_w) / 2 + col * cell_w);
        uint8_t y = (uint8_t)(grid_y + row * CELL_H);
        const char* label = content_shortname(model->mode, model->choices[i]);

        bool selected = (i == model->selected);
        if(selected) canvas_draw_box(canvas, x + 1, y, cell_w - 2, CELL_H - 1);
        canvas_set_color(canvas, selected ? ColorWhite : ColorBlack);
        canvas_draw_str_aligned(
            canvas,
            (uint8_t)(x + cell_w / 2),
            (uint8_t)(y + CELL_H - 3),
            AlignCenter,
            AlignBottom,
            label);
        if(model->eliminated[i]) {
            /* struck out by a hint, still drawn so the layout stays put */
            canvas_draw_line(
                canvas,
                x + 3,
                (uint8_t)(y + CELL_H / 2),
                (uint8_t)(x + cell_w - 4),
                (uint8_t)(y + CELL_H / 2));
        }
        canvas_set_color(canvas, ColorBlack);
    }

    /* footer */
    if(model->quit_armed) {
        canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "Back again to quit");
    } else {
        canvas_draw_str(canvas, 2, 63, "OK pick");
        snprintf(buf, sizeof(buf), "^replay  v hint %u", model->hints_left);
        canvas_draw_str_aligned(canvas, 126, 63, AlignRight, AlignBottom, buf);
    }
}

static void draw_feedback(Canvas* canvas, const QuizModel* model) {
    char buf[40];

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, model->last_correct ? "Correct" : "Nope");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas, 126, 10, AlignRight, AlignBottom, content_name(model->mode, model->correct_id));

    /* the keyboard is the teaching moment: it shows exactly what was played */
    piano_draw(canvas, 14, model->played_notes, model->played_count);

    if(model->show_hint) {
        canvas_draw_str_aligned(
            canvas, 64, 44, AlignCenter, AlignTop, content_hint(model->mode, model->correct_id));
    }

    if(model->last_correct && model->streak > 1) {
        snprintf(buf, sizeof(buf), "streak %u", model->streak);
        canvas_draw_str_aligned(canvas, 126, 63, AlignRight, AlignBottom, buf);
    }
}

/* Same keyboard as the feedback screen, so what you are taught and what you
 * are shown after answering look identical. */
static void draw_teach(Canvas* canvas, const QuizModel* model) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, content_name(model->mode, model->correct_id));

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas,
        126,
        10,
        AlignRight,
        AlignBottom,
        content_shortname(model->mode, model->correct_id));

    piano_draw(canvas, 14, model->played_notes, model->played_count);

    canvas_draw_str_aligned(
        canvas, 64, 44, AlignCenter, AlignTop, content_hint(model->mode, model->correct_id));

    canvas_draw_str(canvas, 2, 63, "OK play");
    canvas_draw_str_aligned(
        canvas, 126, 63, AlignRight, AlignBottom, model->teach_last ? "start >" : "next >");
}

static void quiz_view_draw(Canvas* canvas, void* model_ptr) {
    const QuizModel* model = model_ptr;
    canvas_clear(canvas);
    if(model->phase == QuizPhaseTeach) {
        draw_teach(canvas, model);
    } else if(model->phase == QuizPhaseFeedback) {
        draw_feedback(canvas, model);
    } else {
        draw_answering(canvas, model);
    }
}

static bool quiz_view_input(InputEvent* event, void* context) {
    QuizView* quiz_view = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;
    if(event->key == InputKeyBack) return false; /* let the scene manager handle it */

    /* The scene owns every piece of quiz state, including which answer is
     * highlighted; the view only reports what was pressed. Keeping one owner
     * avoids two copies of `selected` drifting apart. */
    bool accepts = false;
    with_view_model(
        quiz_view->view,
        QuizModel * m,
        {
            accepts = (m->phase == QuizPhaseTeach) ||
                      (m->phase == QuizPhaseAnswering && m->choice_count > 0);
        },
        false);

    if(!accepts) return true; /* swallow input while feedback is up */

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
