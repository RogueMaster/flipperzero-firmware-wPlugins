#include "sweep_view.h"
#include <gui/elements.h>
#include <furi.h>
#include <string.h>
#include <stdio.h>

struct SweepView {
    View* view;
    SweepViewCallback cb;
    void* ctx;
};

typedef struct {
    uint32_t from_ns, to_ns, step_ns;
    uint32_t current_ns;
    uint32_t index, total;
    uint32_t shots, hits;
    uint32_t last_hit_ns;
    bool running;
} SweepModel;

static void sweep_view_draw(Canvas* canvas, void* model) {
    SweepModel* m = model;
    canvas_clear(canvas);

    /* header */
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Sweep");
    canvas_set_font(canvas, FontSecondary);
    const char* st = m->running ? "RUN" : "PAUSE";
    int w = canvas_string_width(canvas, st) + 6;
    if(m->running) {
        canvas_draw_rbox(canvas, 126 - w, 1, w, 11, 2);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, 126 - w + 3, 10, st);
        canvas_set_color(canvas, ColorBlack);
    } else {
        canvas_draw_rframe(canvas, 126 - w, 1, w, 11, 2);
        canvas_draw_str(canvas, 126 - w + 3, 10, st);
    }
    canvas_draw_line(canvas, 0, 13, 128, 13);

    /* current width, prominent */
    char val[24];
    glitch_fmt_ns(m->current_ns, val, sizeof(val));
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 27, "width");
    canvas_draw_str_aligned(canvas, 126, 27, AlignRight, AlignBottom, val);

    /* progress bar */
    int bx = 2, by = 31, bw = 124, bh = 7;
    canvas_draw_rframe(canvas, bx, by, bw, bh, 2);
    if(m->total > 0) {
        int fill = (int)(((uint64_t)(m->index) * (bw - 2)) / m->total);
        if(fill > 0) canvas_draw_box(canvas, bx + 1, by + 1, fill, bh - 2);
    }

    /* range + step line */
    char range[40];
    char a[16], b[16];
    glitch_fmt_ns(m->from_ns, a, sizeof(a));
    glitch_fmt_ns(m->to_ns, b, sizeof(b));
    canvas_set_font(canvas, FontSecondary);
    snprintf(range, sizeof(range), "%s..%s", a, b);
    canvas_draw_str(canvas, 2, 47, range);
    char idx[24];
    snprintf(idx, sizeof(idx), "%lu/%lu", (unsigned long)m->index, (unsigned long)m->total);
    canvas_draw_str_aligned(canvas, 126, 47, AlignRight, AlignBottom, idx);

    /* counters / last hit */
    char line[48];
    if(m->last_hit_ns > 0) {
        char h[16];
        glitch_fmt_ns(m->last_hit_ns, h, sizeof(h));
        snprintf(line, sizeof(line), "hit @ %s (%lu)", h, (unsigned long)m->hits);
    } else {
        snprintf(line, sizeof(line), "shots %lu  hits %lu", (unsigned long)m->shots, (unsigned long)m->hits);
    }
    canvas_draw_str(canvas, 2, 56, line);

    canvas_draw_str_aligned(
        canvas, 64, 63, AlignCenter, AlignBottom, "OK:hit  <:pause  >:restart");
}

static bool sweep_view_input(InputEvent* event, void* context) {
    SweepView* v = context;
    if(event->type != InputTypeShort) return false;
    if(!v->cb) return false;
    switch(event->key) {
    case InputKeyOk:
        v->cb(SweepViewEventMark, v->ctx);
        return true;
    case InputKeyLeft:
        v->cb(SweepViewEventPause, v->ctx);
        return true;
    case InputKeyRight:
        v->cb(SweepViewEventRestart, v->ctx);
        return true;
    default:
        return false;
    }
}

SweepView* sweep_view_alloc(void) {
    SweepView* v = malloc(sizeof(SweepView));
    v->cb = NULL;
    v->ctx = NULL;
    v->view = view_alloc();
    view_set_context(v->view, v);
    view_allocate_model(v->view, ViewModelTypeLocking, sizeof(SweepModel));
    view_set_draw_callback(v->view, sweep_view_draw);
    view_set_input_callback(v->view, sweep_view_input);
    return v;
}

void sweep_view_free(SweepView* v) {
    furi_assert(v);
    view_free(v->view);
    free(v);
}

View* sweep_view_get_view(SweepView* v) {
    furi_assert(v);
    return v->view;
}

void sweep_view_set_callback(SweepView* v, SweepViewCallback cb, void* ctx) {
    v->cb = cb;
    v->ctx = ctx;
}

void sweep_view_set_range(SweepView* v, uint32_t from_ns, uint32_t to_ns, uint32_t step_ns) {
    with_view_model(
        v->view,
        SweepModel * m,
        {
            m->from_ns = from_ns;
            m->to_ns = to_ns;
            m->step_ns = step_ns;
        },
        true);
}

void sweep_view_set_progress(SweepView* v, uint32_t current_ns, uint32_t index, uint32_t total) {
    with_view_model(
        v->view,
        SweepModel * m,
        {
            m->current_ns = current_ns;
            m->index = index;
            m->total = total;
        },
        true);
}

void sweep_view_set_counts(SweepView* v, uint32_t shots, uint32_t hits) {
    with_view_model(
        v->view,
        SweepModel * m,
        {
            m->shots = shots;
            m->hits = hits;
        },
        true);
}

void sweep_view_set_running(SweepView* v, bool running) {
    with_view_model(v->view, SweepModel * m, { m->running = running; }, true);
}

void sweep_view_set_last_hit(SweepView* v, uint32_t hit_ns) {
    with_view_model(v->view, SweepModel * m, { m->last_hit_ns = hit_ns; }, true);
}
