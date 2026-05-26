#include "include/ui/question_view.h"
#include "include/domain/category.h"
#include "include/i18n/strings.h"
#include <string.h>

#ifdef __has_include
#if __has_include(<furi.h>)
#include <furi.h>
#include <gui/canvas.h>
#include <gui/elements.h>
#include <gui/view.h>
#include <input/input.h>
#include <stdlib.h>
#define TZ_HAVE_FURI 1
#endif
#endif

/* ---- Pure word wrap ---- */

void qview_wrap(const char *text, uint8_t max_cols, char *buf, size_t buf_size, LineSlices *out) {
    if (!out)
        return;
    out->count = 0u;
    if (!text || !buf || buf_size == 0u || max_cols == 0u)
        return;

    size_t pos = 0u;
    const size_t tlen = strlen(text);
    if (tlen == 0u)
        return;

    size_t i = 0u;
    while (i < tlen && out->count < QVIEW_MAX_LINES) {
        while (i < tlen && text[i] == ' ')
            i++;
        if (i >= tlen)
            break;

        /* Greedy wrap: find the longest prefix <= max_cols ending on a space
         * boundary, falling through to a hard break if no space fits. */
        size_t end = i;
        size_t last_space = 0u;
        while (end < tlen && (end - i) < max_cols) {
            if (text[end] == ' ')
                last_space = end;
            end++;
        }

        size_t cut;
        if (end >= tlen) {
            cut = end;
        } else if (text[end] == ' ') {
            cut = end;
        } else if (last_space > i) {
            cut = last_space;
        } else {
            cut = end; /* hard break: word longer than max_cols */
        }

        const size_t line_len = cut - i;
        if (pos + line_len + 1u > buf_size)
            break;

        memcpy(buf + pos, text + i, line_len);
        buf[pos + line_len] = '\0';
        out->lines[out->count++] = buf + pos;
        pos += line_len + 1u;
        i = cut;
    }
}

/* ---- Furi-bound View ---- */

#ifdef TZ_HAVE_FURI

#define BODY_VISIBLE_LINES 6u
#define BODY_MAX_COLS 21u
#define WRAP_BUF_SIZE (QUESTION_MAX + ANSWER_MAX)

typedef struct {
    Question q;
    QViewMode mode;
    uint8_t scroll;
    char wrap_buf[WRAP_BUF_SIZE];
    LineSlices slices;
} QViewModel;

struct QuestionView {
    View *view;
    void *ctx;
    void (*on_action)(void *ctx, QViewAction action);
};

static void recompute_slices(QViewModel *m) {
    const char *text = (m->mode == QViewModeAnswer) ? m->q.answer : m->q.question;
    qview_wrap(text, BODY_MAX_COLS, m->wrap_buf, sizeof(m->wrap_buf), &m->slices);
    if (m->scroll >= m->slices.count) {
        m->scroll = (m->slices.count == 0u) ? 0u : (uint8_t)(m->slices.count - 1u);
    }
}

static void qview_draw(Canvas *canvas, void *model) {
    const QViewModel *m = model;
    canvas_clear(canvas);

    /* Inverted header bar with the category in the active locale. */
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 10);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    const char *cat = category_name(m->q.category_id, tz_locale_get());
    canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, cat);

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    const uint8_t y0 = 13u;
    for (uint8_t i = 0u; i < BODY_VISIBLE_LINES; ++i) {
        const uint8_t idx = (uint8_t)(m->scroll + i);
        if (idx >= m->slices.count)
            break;
        canvas_draw_str(canvas, 0, (int)(y0 + i * 8u + 7u), m->slices.lines[idx]);
    }

    if (m->scroll + BODY_VISIBLE_LINES < m->slices.count) {
        canvas_draw_str(canvas, 120, 54, "v");
    }

    const char *footer =
        (m->mode == QViewModeQuestion) ? tz_str(TzStrFooterReveal) : tz_str(TzStrFooterNext);
    canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignTop, footer);
}

static bool qview_input(InputEvent *event, void *context) {
    QuestionView *qv = context;
    if (!qv || !qv->on_action || event->type != InputTypeShort) {
        return false;
    }
    QViewAction a;
    switch (event->key) {
        case InputKeyOk:
            a = QViewActionReveal;
            break;
        case InputKeyRight:
            a = QViewActionNext;
            break;
        case InputKeyLeft:
            a = QViewActionPrev;
            break;
        case InputKeyUp:
            a = QViewActionScrollUp;
            break;
        case InputKeyDown:
            a = QViewActionScrollDown;
            break;
        case InputKeyBack:
            a = QViewActionMenu;
            break;
        default:
            return false;
    }
    qv->on_action(qv->ctx, a);
    return true;
}

QuestionView *question_view_alloc(void) {
    QuestionView *qv = malloc(sizeof(QuestionView));
    if (!qv)
        return NULL;
    *qv = (QuestionView){0};
    qv->view = view_alloc();
    if (!qv->view) {
        free(qv);
        return NULL;
    }
    view_allocate_model(qv->view, ViewModelTypeLockFree, sizeof(QViewModel));
    QViewModel *m = view_get_model(qv->view);
    memset(m, 0, sizeof(*m));
    m->mode = QViewModeQuestion;
    view_commit_model(qv->view, true);
    view_set_context(qv->view, qv);
    view_set_draw_callback(qv->view, qview_draw);
    view_set_input_callback(qv->view, qview_input);
    return qv;
}

void question_view_free(QuestionView *qv) {
    if (!qv)
        return;
    view_free(qv->view);
    free(qv);
}

View *question_view_get_view(QuestionView *qv) {
    furi_assert(qv);
    return qv->view;
}

void question_view_set_question(QuestionView *qv, const Question *q) {
    furi_assert(qv);
    if (!q)
        return;
    QViewModel *m = view_get_model(qv->view);
    m->q = *q;
    m->mode = QViewModeQuestion;
    m->scroll = 0u;
    recompute_slices(m);
    view_commit_model(qv->view, true);
}

void question_view_set_mode(QuestionView *qv, QViewMode mode) {
    furi_assert(qv);
    QViewModel *m = view_get_model(qv->view);
    m->mode = mode;
    m->scroll = 0u;
    recompute_slices(m);
    view_commit_model(qv->view, true);
}

void question_view_set_callback(QuestionView *qv, void *ctx,
                                void (*on_action)(void *ctx, QViewAction action)) {
    furi_assert(qv);
    qv->ctx = ctx;
    qv->on_action = on_action;
}

#endif /* TZ_HAVE_FURI */
