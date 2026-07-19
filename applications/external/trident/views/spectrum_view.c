#include "spectrum_view.h"
#include "trident_icons.h" // generated from icons/ by fbt; on the app include path
#include <furi.h>
#include <string.h>

/*
 * Layout (128x64):
 *   y 0..11   header : icon + title + SCAN/IDLE status dot
 *   y 12      divider
 *   y 14..51  graph  : one vertical bar per bin, baseline at GRAPH_BASE
 *   y 52      divider
 *   y 53..63  footer : lo label | peak summary | hi label
 */
#define GRAPH_BASE 51
#define GRAPH_TOP  15
#define GRAPH_H    (GRAPH_BASE - GRAPH_TOP) // 36 px of vertical range

struct SpectrumView {
    View* view;
    SpectrumViewCallback ok_cb;
    void* ok_ctx;
};

typedef struct {
    SpectrumSnapshot snap;
    uint8_t anim;
} SpectrumModel;

static void spectrum_view_draw(Canvas* canvas, void* model) {
    SpectrumModel* m = model;
    const SpectrumSnapshot* s = &m->snap;

    /* ---- header ---- */
    canvas_draw_icon(canvas, 0, 1, &I_trident_10px);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 13, 9, s->title[0] ? s->title : "Analyzer");

    const char* state = s->running ? "SCAN" : "IDLE";
    canvas_draw_str_aligned(canvas, 110, 9, AlignRight, AlignBottom, state);
    if(s->running && (m->anim & 1)) {
        canvas_draw_disc(canvas, 124, 5, 2);
    } else {
        canvas_draw_circle(canvas, 124, 5, 2);
    }
    canvas_draw_line(canvas, 0, 12, 127, 12);

    /* ---- hardware missing ---- */
    if(!s->present) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 6, 34, "Radio not detected.");
        canvas_draw_str(canvas, 6, 46, "Check the board & wiring.");
        return;
    }

    /* ---- graph ---- */
    canvas_draw_line(canvas, 0, GRAPH_BASE + 1, 127, GRAPH_BASE + 1); // baseline

    uint16_t n = s->count;
    if(n > SPECTRUM_MAX_BINS) n = SPECTRUM_MAX_BINS;
    if(n > 0) {
        // Map n bins across the 128px width. n is typically 126 (1px bars) or a
        // sweep count that we widen so the graph fills the screen.
        for(uint16_t i = 0; i < n; i++) {
            int x = (n <= 1) ? 1 : (int)((uint32_t)i * 127u / (n - 1));
            int h = (int)s->level[i] * GRAPH_H / 100;
            if(h < 0) h = 0;
            if(h > GRAPH_H) h = GRAPH_H;
            if(h > 0) canvas_draw_line(canvas, x, GRAPH_BASE, x, GRAPH_BASE - h);
        }
        // peak marker: caret above the strongest bin
        if(s->peak_bin >= 0 && s->peak_bin < (int)n) {
            int px = (n <= 1) ? 1 : (int)((uint32_t)s->peak_bin * 127u / (n - 1));
            canvas_draw_line(canvas, px, GRAPH_TOP - 1, px - 2, GRAPH_TOP - 3);
            canvas_draw_line(canvas, px, GRAPH_TOP - 1, px + 2, GRAPH_TOP - 3);
        }
    }

    /* ---- footer ---- */
    canvas_draw_line(canvas, 0, 52, 127, 52);
    canvas_set_font(canvas, FontSecondary);
    if(s->lo_label[0]) canvas_draw_str(canvas, 0, 63, s->lo_label);
    if(s->hi_label[0]) canvas_draw_str_aligned(canvas, 127, 63, AlignRight, AlignBottom, s->hi_label);
    if(s->peak_label[0])
        canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, s->peak_label);
}

static bool spectrum_view_input(InputEvent* event, void* context) {
    SpectrumView* v = context;
    if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(v->ok_cb) v->ok_cb(v->ok_ctx);
        return true;
    }
    return false;
}

SpectrumView* spectrum_view_alloc(void) {
    SpectrumView* v = malloc(sizeof(SpectrumView));
    v->ok_cb = NULL;
    v->ok_ctx = NULL;
    v->view = view_alloc();
    view_set_context(v->view, v);
    view_set_draw_callback(v->view, spectrum_view_draw);
    view_set_input_callback(v->view, spectrum_view_input);
    view_allocate_model(v->view, ViewModelTypeLocking, sizeof(SpectrumModel));
    with_view_model(
        v->view, SpectrumModel * m, { memset(m, 0, sizeof(SpectrumModel)); }, false);
    return v;
}

void spectrum_view_free(SpectrumView* v) {
    furi_assert(v);
    view_free(v->view);
    free(v);
}

View* spectrum_view_get_view(SpectrumView* v) {
    furi_assert(v);
    return v->view;
}

void spectrum_view_set_ok_callback(SpectrumView* v, SpectrumViewCallback cb, void* context) {
    furi_assert(v);
    v->ok_cb = cb;
    v->ok_ctx = context;
}

void spectrum_view_set_snapshot(SpectrumView* v, const SpectrumSnapshot* snap) {
    furi_assert(v);
    furi_assert(snap);
    with_view_model(
        v->view,
        SpectrumModel * m,
        {
            m->snap = *snap;
            m->anim++;
        },
        true);
}
