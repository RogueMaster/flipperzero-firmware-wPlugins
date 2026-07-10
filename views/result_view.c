#include "result_view.h"
#include <gui/elements.h>
#include <furi.h>
#include <string.h>

struct ResultView {
    View* view;
    ResultViewCallback cb;
    void* ctx;
};

typedef struct {
    CardGrade grade;
    bool has;
} ResultModel;

/* Truncate a copy of `src` so it fits within `max_w` px in the current font. */
static void fit_text(Canvas* canvas, const char* src, int max_w, char* out, size_t out_sz) {
    strncpy(out, src, out_sz - 1);
    out[out_sz - 1] = '\0';
    if(canvas_string_width(canvas, out) <= max_w) return;

    size_t len = strlen(out);
    while(len > 1) {
        len--;
        char probe[48];
        size_t n = (len < sizeof(probe) - 3) ? len : sizeof(probe) - 3;
        memcpy(probe, src, n);
        probe[n] = '.';
        probe[n + 1] = '.';
        probe[n + 2] = '\0';
        if(canvas_string_width(canvas, probe) <= max_w) {
            strncpy(out, probe, out_sz - 1);
            out[out_sz - 1] = '\0';
            return;
        }
    }
}

static void result_view_draw(Canvas* canvas, void* model) {
    ResultModel* m = model;
    canvas_clear(canvas);
    if(!m->has) return;
    const CardGrade* g = &m->grade;

    /* --- title: what the card is --- */
    canvas_set_font(canvas, FontPrimary);
    char name[40];
    fit_text(canvas, g->card_name, 124, name, sizeof(name));
    canvas_draw_str(canvas, 2, 10, name);
    canvas_draw_line(canvas, 0, 12, 128, 12);

    /* --- grade badge (left) --- */
    canvas_draw_rframe(canvas, 2, 15, 34, 30, 3);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 19, 30, AlignCenter, AlignCenter, g->letter);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 19, 40, AlignCenter, AlignCenter, "GRADE");

    /* --- risk band bar (top right, inverted) --- */
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rbox(canvas, 40, 15, 86, 13, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas, 83, 21, AlignCenter, AlignCenter, grader_band_label(g->band));
    canvas_set_color(canvas, ColorBlack);

    /* --- score (big) --- */
    char sc[8];
    snprintf(sc, sizeof(sc), "%d", g->score);
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str(canvas, 44, 45, sc);
    int nw = canvas_string_width(canvas, sc);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 44 + nw + 3, 45, "/100");

    /* --- security meter --- */
    canvas_draw_frame(canvas, 2, 49, 124, 7);
    int inner = 120; // 124 - 4
    int fill = (g->score * inner) / 100;
    if(fill > 0) canvas_draw_box(canvas, 4, 51, fill, 3);

    /* --- footer hints (inverted) --- */
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 57, 128, 7);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 3, 63, "OK: Report");
    canvas_draw_str_aligned(canvas, 125, 63, AlignRight, AlignBottom, "Rescan >");
    canvas_set_color(canvas, ColorBlack);
}

static bool result_view_input(InputEvent* event, void* context) {
    ResultView* v = context;
    if(event->type != InputTypeShort) return false;

    if(event->key == InputKeyOk) {
        if(v->cb) v->cb(v->ctx, ResultEventDetails);
        return true;
    }
    if(event->key == InputKeyRight) {
        if(v->cb) v->cb(v->ctx, ResultEventRescan);
        return true;
    }
    return false; // Back / others fall through to navigation
}

ResultView* result_view_alloc(void) {
    ResultView* v = malloc(sizeof(ResultView));
    v->cb = NULL;
    v->ctx = NULL;
    v->view = view_alloc();
    view_set_context(v->view, v);
    view_allocate_model(v->view, ViewModelTypeLocking, sizeof(ResultModel));
    view_set_draw_callback(v->view, result_view_draw);
    view_set_input_callback(v->view, result_view_input);
    return v;
}

void result_view_free(ResultView* v) {
    furi_assert(v);
    view_free(v->view);
    free(v);
}

View* result_view_get_view(ResultView* v) {
    furi_assert(v);
    return v->view;
}

void result_view_set_callback(ResultView* v, ResultViewCallback cb, void* context) {
    furi_assert(v);
    v->cb = cb;
    v->ctx = context;
}

void result_view_set_grade(ResultView* v, const CardGrade* grade) {
    furi_assert(v);
    with_view_model(
        v->view,
        ResultModel * m,
        {
            m->grade = *grade;
            m->has = true;
        },
        true);
}
