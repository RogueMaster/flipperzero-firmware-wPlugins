#include "scope_view.h"
#include <gui/elements.h>
#include <furi.h>
#include <string.h>
#include <stdio.h>

/* Scope plot area geometry. */
#define PLOT_X0 0
#define PLOT_Y0 14
#define PLOT_W  128
#define PLOT_H  38
#define PLOT_Y1 (PLOT_Y0 + PLOT_H)

struct ScopeView {
    View* view;
};

typedef struct {
    RfSnapshot snap;
    bool have;
} ScopeModel;

static void scope_view_draw(Canvas* canvas, void* model) {
    ScopeModel* m = model;
    canvas_clear(canvas);

    RfSnapshot* s = &m->snap;

    /* ---- header ---- */
    canvas_set_font(canvas, FontPrimary);
    char hz[16];
    uint32_t mhz = s->freq_hz / 1000000u;
    uint32_t frac = (s->freq_hz % 1000000u) / 10000u; // 2 decimals
    snprintf(hz, sizeof(hz), "%lu.%02lu MHz", (unsigned long)mhz, (unsigned long)frac);
    canvas_draw_str(canvas, 2, 11, hz);

    canvas_set_font(canvas, FontSecondary);
    char rssi[16];
    snprintf(rssi, sizeof(rssi), "%d dBm", (int)s->rssi_dbm);
    canvas_draw_str_aligned(canvas, 126, 10, AlignRight, AlignBottom, rssi);

    if(!s->present) {
        elements_multiline_text_aligned(
            canvas, 64, 36, AlignCenter, AlignCenter, "Sub-GHz radio\nunavailable");
        return;
    }

    /* ---- plot frame ---- */
    canvas_draw_line(canvas, PLOT_X0, PLOT_Y1, PLOT_X0 + PLOT_W - 1, PLOT_Y1);

    /* map an envelope level 0..RF_SCOPE_MAX onto plot rows */
    const int span = RF_SCOPE_MAX;

    /* threshold line (dotted): "carrier present" cut */
    int ty = PLOT_Y1 - (int)((int)s->threshold * (PLOT_H - 2) / span);
    for(int x = PLOT_X0; x < PLOT_X0 + PLOT_W; x += 3) {
        canvas_draw_dot(canvas, x, ty);
    }

    /* the envelope trace + fill under any burst above threshold */
    int cols = PLOT_W;
    for(int x = 0; x < cols; x++) {
        int idx = x * RF_SCOPE_SAMPLES / cols;
        int lvl = s->level[idx];
        int y = PLOT_Y1 - (lvl * (PLOT_H - 2) / span);
        if(y < PLOT_Y0) y = PLOT_Y0;

        canvas_draw_dot(canvas, PLOT_X0 + x, y);
        /* emphasise bursts: draw a vertical stem when above the threshold */
        if(lvl >= s->threshold && s->threshold > 0) {
            canvas_draw_line(canvas, PLOT_X0 + x, y, PLOT_X0 + x, PLOT_Y1 - 1);
        }
    }

    /* ---- footer / legend ---- */
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 63, "OOK: above line = carrier ON");
}

static bool scope_view_input(InputEvent* event, void* context) {
    UNUSED(event);
    UNUSED(context);
    return false; // Back propagates to the scene manager
}

ScopeView* scope_view_alloc(void) {
    ScopeView* v = malloc(sizeof(ScopeView));
    v->view = view_alloc();
    view_set_context(v->view, v);
    view_allocate_model(v->view, ViewModelTypeLocking, sizeof(ScopeModel));
    view_set_draw_callback(v->view, scope_view_draw);
    view_set_input_callback(v->view, scope_view_input);
    return v;
}

void scope_view_free(ScopeView* v) {
    furi_assert(v);
    view_free(v->view);
    free(v);
}

View* scope_view_get_view(ScopeView* v) {
    furi_assert(v);
    return v->view;
}

void scope_view_reset(ScopeView* v) {
    furi_assert(v);
    with_view_model(v->view, ScopeModel * m, { memset(m, 0, sizeof(ScopeModel)); }, true);
}

void scope_view_set_snapshot(ScopeView* v, const RfSnapshot* snap) {
    furi_assert(v);
    furi_assert(snap);
    with_view_model(
        v->view,
        ScopeModel * m,
        {
            m->snap = *snap;
            m->have = true;
        },
        true);
}
