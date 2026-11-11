#include "quiz_view.h"

#include <furi.h>
#include <ctype.h>
#include <gui/elements.h>
#include <string.h>

#define QUIZ_BLOCKS     5 /* question + 4 answers */
#define QUIZ_FOOTER     QUIZ_BLOCKS /* extra block shown once the answer is revealed */
#define QUIZ_TEXT_SIZE  1700
#define QUIZ_MAX_LINES  160
#define QUIZ_LINE_CHARS 63

#define QUIZ_SCREEN_W  128
#define QUIZ_SCREEN_H  64
#define QUIZ_HEADER_H  9
#define QUIZ_CONTENT_Y 11
#define QUIZ_LINE_H    10 /* 7 px glyph + 2 px descender + 1 px gap */
#define QUIZ_VISIBLE   5
#define QUIZ_TEXT_W    124 /* leaves room for the scrollbar */
#define QUIZ_ANSWER_X  14 /* after the "A." label */

typedef struct {
    uint16_t start;
    uint8_t length;
    uint8_t block;
} QuizLine;

typedef struct {
    char left[12];
    char center[12];
    char right[8];
    char text[QUIZ_TEXT_SIZE];
    uint16_t block_text[QUIZ_BLOCKS + 1];
    int8_t selected; /* -1 = reading the question, 0-3 = answer slot */
    int8_t chosen;
    int8_t correct;
    bool reveal;
    bool laid_out;
    uint8_t scroll;
    uint8_t line_count;
    uint8_t block_first[QUIZ_BLOCKS + 1];
    uint8_t block_lines[QUIZ_BLOCKS + 1];
    QuizLine lines[QUIZ_MAX_LINES];
} QuizViewModel;

struct QuizView {
    View* view;
    QuizViewCallback callback;
    void* context;
};

/* ---------------------------------------------------------------- layout */

static void quiz_view_wrap_block(Canvas* canvas, QuizViewModel* model, uint8_t block) {
    const char* text = model->text + model->block_text[block];
    const bool full_width = block == 0 || block == QUIZ_FOOTER;
    const size_t max_width = full_width ? QUIZ_TEXT_W - 2 : QUIZ_TEXT_W - QUIZ_ANSWER_X - 1;
    size_t i = 0;

    model->block_first[block] = model->line_count;
    while(text[i] != '\0' && model->line_count < QUIZ_MAX_LINES) {
        size_t width = 0;
        size_t j = i;
        size_t break_end = 0;
        size_t break_next = 0;
        bool has_break = false;
        while(text[j] != '\0' && j - i < QUIZ_LINE_CHARS) {
            size_t glyph = canvas_glyph_width(canvas, (uint8_t)text[j]);
            if(width + glyph > max_width) break;
            if(text[j] == ' ') {
                /* Break at a space, dropping it */
                break_end = j;
                break_next = j + 1;
                has_break = true;
            } else if(
                text[j] == '-' && j > i && isalpha((uint8_t)text[j - 1]) &&
                isalpha((uint8_t)text[j + 1])) {
                /* Break after the hyphen of a compound word such as
                 * "temperature-controlled", but not inside "-6 dB" or "RG-58" */
                break_end = j + 1;
                break_next = j + 1;
                has_break = true;
            }
            width += glyph;
            j++;
        }

        size_t end;
        size_t next;
        if(text[j] == '\0') {
            end = next = j;
        } else if(text[j] == ' ') {
            end = j;
            next = j + 1;
        } else if(has_break) {
            end = break_end;
            next = break_next;
        } else {
            /* One word wider than the screen: break it, always advancing */
            end = next = (j > i) ? j : i + 1;
        }

        QuizLine* line = &model->lines[model->line_count++];
        line->start = model->block_text[block] + i;
        line->length = end - i;
        line->block = block;
        i = next;
    }
    model->block_lines[block] = model->line_count - model->block_first[block];
}

static void quiz_view_layout(Canvas* canvas, QuizViewModel* model) {
    model->line_count = 0;
    for(uint8_t block = 0; block <= QUIZ_FOOTER; block++) {
        quiz_view_wrap_block(canvas, model, block);
    }
    model->laid_out = true;
}

/* Lines the user can scroll through: the footer only exists after the reveal */
static uint8_t quiz_view_shown_lines(const QuizViewModel* model) {
    return model->reveal ? model->line_count : model->block_first[QUIZ_FOOTER];
}

static uint8_t quiz_view_max_scroll(const QuizViewModel* model) {
    uint8_t lines = quiz_view_shown_lines(model);
    return lines > QUIZ_VISIBLE ? lines - QUIZ_VISIBLE : 0;
}

/* Scroll as little as possible so that `block` is on screen. A block taller
 * than the screen is aligned to its top, or to its bottom when arriving
 * from below. */
static void quiz_view_show_block(QuizViewModel* model, uint8_t block, bool from_below) {
    int32_t first = model->block_first[block];
    int32_t lines = model->block_lines[block];
    int32_t end = first + lines;
    int32_t scroll = model->scroll;

    if(lines >= QUIZ_VISIBLE) {
        if(from_below) {
            if(first < scroll) scroll = end - QUIZ_VISIBLE;
        } else if(first < scroll || end > scroll + QUIZ_VISIBLE) {
            scroll = first;
        }
    } else if(first < scroll) {
        scroll = first;
    } else if(end > scroll + QUIZ_VISIBLE) {
        scroll = end - QUIZ_VISIBLE;
    }

    int32_t max_scroll = quiz_view_max_scroll(model);
    if(scroll > max_scroll) scroll = max_scroll;
    if(scroll < 0) scroll = 0;
    model->scroll = scroll;
}

/* ------------------------------------------------------------------ draw */

static void quiz_view_draw_mark(Canvas* canvas, int32_t y, bool check) {
    if(check) {
        canvas_draw_line(canvas, 2, y + 4, 4, y + 6);
        canvas_draw_line(canvas, 4, y + 6, 8, y + 2);
        canvas_draw_line(canvas, 2, y + 5, 4, y + 7);
        canvas_draw_line(canvas, 4, y + 7, 8, y + 3);
    } else {
        canvas_draw_line(canvas, 2, y + 2, 7, y + 7);
        canvas_draw_line(canvas, 7, y + 2, 2, y + 7);
        canvas_draw_line(canvas, 3, y + 2, 8, y + 7);
        canvas_draw_line(canvas, 8, y + 2, 3, y + 7);
    }
}

static void quiz_view_draw_callback(Canvas* canvas, void* context) {
    QuizViewModel* model = context;

    canvas_set_font(canvas, FontSecondary);
    if(!model->laid_out) quiz_view_layout(canvas, model);

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, QUIZ_SCREEN_W, QUIZ_HEADER_H);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str_aligned(canvas, 2, 8, AlignLeft, AlignBottom, model->left);
    canvas_draw_str_aligned(canvas, 64, 8, AlignCenter, AlignBottom, model->center);
    canvas_draw_str_aligned(canvas, 126, 8, AlignRight, AlignBottom, model->right);
    canvas_set_color(canvas, ColorBlack);

    for(uint8_t row = 0; row < QUIZ_VISIBLE; row++) {
        uint16_t index = model->scroll + row;
        if(index >= quiz_view_shown_lines(model)) break;
        const QuizLine* line = &model->lines[index];
        int32_t y = QUIZ_CONTENT_Y + row * QUIZ_LINE_H;
        int8_t slot = (int8_t)line->block - 1;
        bool is_answer = slot >= 0 && slot < QUIZ_BLOCKS - 1;
        bool highlight = is_answer &&
                         (model->reveal ? slot == model->correct : slot == model->selected);

        if(highlight) {
            canvas_draw_box(canvas, 0, y, QUIZ_TEXT_W, QUIZ_LINE_H);
            canvas_set_color(canvas, ColorWhite);
        }

        char buffer[QUIZ_LINE_CHARS + 1];
        size_t length = line->length < QUIZ_LINE_CHARS ? line->length : QUIZ_LINE_CHARS;
        memcpy(buffer, model->text + line->start, length);
        buffer[length] = '\0';
        canvas_draw_str(canvas, is_answer ? QUIZ_ANSWER_X : 1, y + 8, buffer);

        if(is_answer && index == model->block_first[line->block]) {
            if(model->reveal && slot == model->correct) {
                quiz_view_draw_mark(canvas, y, true);
            } else if(model->reveal && slot == model->chosen) {
                quiz_view_draw_mark(canvas, y, false);
            } else {
                /* The dot keeps "A." apart from an answer that starts with "A" */
                char label[3] = {(char)('A' + slot), '.', '\0'};
                if(!model->reveal && slot == model->chosen) {
                    /* Already answered (exam): letter in a contrasting box */
                    canvas_draw_box(canvas, 1, y, 12, QUIZ_LINE_H);
                    canvas_invert_color(canvas);
                    canvas_draw_str(canvas, 2, y + 8, label);
                    canvas_invert_color(canvas);
                } else {
                    canvas_draw_str(canvas, 2, y + 8, label);
                }
            }
        }
        canvas_set_color(canvas, ColorBlack);
    }

    if(quiz_view_shown_lines(model) > QUIZ_VISIBLE) {
        elements_scrollbar_pos(
            canvas,
            QUIZ_SCREEN_W - 1,
            QUIZ_CONTENT_Y,
            QUIZ_SCREEN_H - QUIZ_CONTENT_Y,
            model->scroll,
            quiz_view_max_scroll(model) + 1);
    }
}

/* ----------------------------------------------------------------- input */

static void quiz_view_move(QuizViewModel* model, bool down) {
    uint8_t max_scroll = quiz_view_max_scroll(model);

    if(model->reveal) {
        if(down && model->scroll < max_scroll) model->scroll++;
        if(!down && model->scroll > 0) model->scroll--;
        return;
    }

    uint8_t block = model->selected + 1;
    uint8_t first = model->block_first[block];
    uint8_t end = first + model->block_lines[block];
    if(down) {
        if(end > model->scroll + QUIZ_VISIBLE) {
            model->scroll++;
        } else if(model->selected < 3) {
            model->selected++;
            quiz_view_show_block(model, block + 1, false);
        }
    } else {
        if(first < model->scroll) {
            model->scroll--;
        } else if(model->selected > -1) {
            model->selected--;
            quiz_view_show_block(model, block - 1, true);
        }
    }
}

static bool quiz_view_input_callback(InputEvent* event, void* context) {
    QuizView* quiz_view = context;
    bool consumed = false;
    bool send = false;
    QuizViewEvent quiz_event = QuizViewEventNext;
    uint8_t slot = 0;

    if(event->key == InputKeyBack) return false;
    bool is_short = event->type == InputTypeShort;
    bool is_repeat = event->type == InputTypeRepeat;

    with_view_model(
        quiz_view->view,
        QuizViewModel * model,
        {
            if(!model->laid_out) {
                /* Nothing has been drawn yet, so there is nothing to act on */
            } else if(event->key == InputKeyUp || event->key == InputKeyDown) {
                if(is_short || is_repeat) {
                    quiz_view_move(model, event->key == InputKeyDown);
                    consumed = true;
                }
            } else if(event->key == InputKeyOk && is_short) {
                if(model->reveal) {
                    send = true;
                } else if(model->selected < 0) {
                    quiz_view_move(model, true);
                } else {
                    quiz_event = QuizViewEventAnswer;
                    slot = model->selected;
                    send = true;
                }
                consumed = true;
            } else if(event->key == InputKeyRight && is_short) {
                send = true;
                consumed = true;
            } else if(event->key == InputKeyLeft && is_short) {
                quiz_event = QuizViewEventPrev;
                send = true;
                consumed = true;
            }
        },
        consumed);

    if(send && quiz_view->callback) {
        quiz_view->callback(quiz_event, slot, quiz_view->context);
    }
    return consumed;
}

/* ------------------------------------------------------------------- API */

QuizView* quiz_view_alloc(void) {
    QuizView* quiz_view = malloc(sizeof(QuizView));
    quiz_view->callback = NULL;
    quiz_view->context = NULL;
    quiz_view->view = view_alloc();
    view_allocate_model(quiz_view->view, ViewModelTypeLocking, sizeof(QuizViewModel));
    view_set_context(quiz_view->view, quiz_view);
    view_set_draw_callback(quiz_view->view, quiz_view_draw_callback);
    view_set_input_callback(quiz_view->view, quiz_view_input_callback);
    with_view_model(
        quiz_view->view,
        QuizViewModel * model,
        {
            memset(model, 0, sizeof(QuizViewModel));
            model->selected = -1;
            model->chosen = -1;
            model->correct = -1;
        },
        false);
    return quiz_view;
}

void quiz_view_free(QuizView* quiz_view) {
    furi_check(quiz_view);
    view_free(quiz_view->view);
    free(quiz_view);
}

View* quiz_view_get_view(QuizView* quiz_view) {
    furi_check(quiz_view);
    return quiz_view->view;
}

void quiz_view_set_callback(QuizView* quiz_view, QuizViewCallback callback, void* context) {
    furi_check(quiz_view);
    quiz_view->callback = callback;
    quiz_view->context = context;
}

/* Append `text` at `*used`, always leaving the buffer terminated */
static void quiz_view_append(QuizViewModel* model, size_t* used, const char* text) {
    if(!text) return;
    size_t room = QUIZ_TEXT_SIZE - 1 - *used;
    size_t length = strlen(text);
    if(length > room) length = room;
    memcpy(model->text + *used, text, length);
    *used += length;
    model->text[*used] = '\0';
}

void quiz_view_set_question(QuizView* quiz_view, const QuizViewData* data) {
    furi_check(quiz_view);
    furi_check(data);
    with_view_model(
        quiz_view->view,
        QuizViewModel * model,
        {
            strlcpy(model->left, data->left ? data->left : "", sizeof(model->left));
            strlcpy(model->center, data->center ? data->center : "", sizeof(model->center));
            strlcpy(model->right, data->right ? data->right : "", sizeof(model->right));

            size_t used = 0;
            model->text[0] = '\0';
            model->block_text[0] = 0;
            if(data->note && data->note[0]) {
                quiz_view_append(model, &used, data->note);
                quiz_view_append(model, &used, " ");
            }
            quiz_view_append(model, &used, data->question);
            for(uint8_t i = 0; i < 4; i++) {
                if(used < QUIZ_TEXT_SIZE - 1) used++; /* keep the terminator */
                model->block_text[i + 1] = used;
                model->text[used] = '\0';
                quiz_view_append(model, &used, data->answers[i]);
            }
            if(used < QUIZ_TEXT_SIZE - 1) used++;
            model->block_text[QUIZ_FOOTER] = used;
            model->text[used] = '\0';
            quiz_view_append(model, &used, data->footer);

            model->selected = -1;
            model->chosen = data->chosen;
            model->correct = data->correct;
            model->reveal = data->reveal;
            model->scroll = 0;
            model->line_count = 0;
            model->laid_out = false;
        },
        true);
}

void quiz_view_reveal(QuizView* quiz_view, int8_t chosen, int8_t correct, const char* center) {
    furi_check(quiz_view);
    with_view_model(
        quiz_view->view,
        QuizViewModel * model,
        {
            strlcpy(model->center, center ? center : "", sizeof(model->center));
            model->chosen = chosen;
            model->correct = correct;
            model->reveal = true;
            if(model->laid_out && correct >= 0 && correct < 4) {
                quiz_view_show_block(model, correct + 1, false);
            }
        },
        true);
}
