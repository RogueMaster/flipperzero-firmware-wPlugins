#include "result_view.h"
#include <furi.h>
#include <string.h>
#include <math.h>

struct ResultView {
    View* view;
    ResultViewCallback cb;
    void* ctx;
};

typedef struct {
    bool correct;
    uint16_t points;
    uint32_t score;
    char rank[16];
    int frame;
} ResultModel;

static void draw_flag(Canvas* canvas, int x, int y) {
    /* a little flag on a pole - the "capture the flag" motif */
    canvas_draw_line(canvas, x, y - 9, x, y + 9);
    canvas_draw_line(canvas, x + 1, y - 9, x + 1, y + 9);
    canvas_draw_line(canvas, x - 3, y + 9, x + 4, y + 9); /* base */
    /* pennant */
    for(int i = 0; i < 8; i++) {
        canvas_draw_line(canvas, x + 2, y - 9 + i, x + 2 + (8 - i), y - 9 + i);
    }
}

static void draw_burst(Canvas* canvas, int cx, int cy, int frame) {
    int grow = frame * 3;
    if(grow > 20) grow = 20;
    for(int i = 0; i < 12; i++) {
        float a = (float)i * (float)(M_PI / 6.0);
        int r0 = 15;
        int r1 = 15 + grow;
        int x0 = cx + (int)(cosf(a) * r0);
        int y0 = cy + (int)(sinf(a) * r0);
        int x1 = cx + (int)(cosf(a) * r1);
        int y1 = cy + (int)(sinf(a) * r1);
        canvas_draw_line(canvas, x0, y0, x1, y1);
    }
    /* sparkle dots on alternate frames */
    if(frame % 2 == 0) {
        for(int i = 0; i < 12; i += 2) {
            float a = (float)i * (float)(M_PI / 6.0) + 0.26f;
            int r = 15 + grow + 3;
            canvas_draw_dot(canvas, cx + (int)(cosf(a) * r), cy + (int)(sinf(a) * r));
        }
    }
}

static void draw_frown(Canvas* canvas, int cx, int cy) {
    canvas_draw_circle(canvas, cx, cy, 11);
    canvas_draw_disc(canvas, cx - 4, cy - 3, 1);
    canvas_draw_disc(canvas, cx + 4, cy - 3, 1);
    /* down-turned mouth */
    canvas_draw_line(canvas, cx - 4, cy + 6, cx - 2, cy + 4);
    canvas_draw_line(canvas, cx - 2, cy + 4, cx + 2, cy + 4);
    canvas_draw_line(canvas, cx + 2, cy + 4, cx + 4, cy + 6);
}

static void result_view_draw(Canvas* canvas, void* model) {
    ResultModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    if(m->correct) {
        draw_burst(canvas, 64, 18, m->frame);
        draw_flag(canvas, 62, 18);

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignBottom, "FLAG CAPTURED");

        char line[40];
        snprintf(line, sizeof(line), "+%u pts    Score %lu", m->points, (unsigned long)m->score);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 53, AlignCenter, AlignBottom, line);

        canvas_draw_box(canvas, 0, 56, 128, 8);
        canvas_set_color(canvas, ColorWhite);
        char foot[28];
        snprintf(foot, sizeof(foot), "Rank: %s", m->rank);
        canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, foot);
        canvas_set_color(canvas, ColorBlack);
    } else {
        int dx = (m->frame < 6 && (m->frame % 2)) ? 2 : 0; /* brief shake */
        draw_frown(canvas, 64 + dx, 18);

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64 + dx, 42, AlignCenter, AlignBottom, "NOT QUITE");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas, 64, 53, AlignCenter, AlignBottom, "Wrong flag - try again");

        canvas_draw_box(canvas, 0, 56, 128, 8);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "Back to challenge");
        canvas_set_color(canvas, ColorBlack);
    }
}

static bool result_view_input(InputEvent* event, void* context) {
    ResultView* v = context;
    if(event->type != InputTypeShort) return false;
    if(event->key == InputKeyOk) {
        if(v->cb) v->cb(v->ctx, ResultEventContinue);
        return true;
    }
    return false; /* Back falls through to navigation */
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

void result_view_set(
    ResultView* v,
    bool correct,
    uint16_t points,
    uint32_t score,
    const char* rank) {
    furi_assert(v);
    with_view_model(
        v->view,
        ResultModel * m,
        {
            m->correct = correct;
            m->points = points;
            m->score = score;
            strncpy(m->rank, rank ? rank : "", sizeof(m->rank) - 1);
            m->rank[sizeof(m->rank) - 1] = '\0';
            m->frame = 0;
        },
        true);
}

void result_view_tick(ResultView* v) {
    furi_assert(v);
    with_view_model(v->view, ResultModel * m, { m->frame++; }, true);
}
