#include "hunt_view.h"
#include <gui/elements.h>
#include <furi.h>
#include <string.h>

/* Chart geometry. RC_BAND_COUNT bars at HV_PITCH must span <= 128px. */
#define HV_BAR_W   8
#define HV_PITCH   9
#define HV_TOP     15
#define HV_BOTTOM  46
#define HV_HEIGHT  (HV_BOTTOM - HV_TOP)

/* A 48 dB climb over the noise floor is a full-height bar. A fob pressed
 * against the Flipper clips well past that; the scale is for reading, not
 * measuring. */
#define HV_FULL_SCALE_DB 48

struct HuntView {
    View* view;
    HuntViewCallback cb;
    void* ctx;
};

typedef struct {
    RcHuntBand bands[RC_BAND_COUNT];
    uint8_t count;
    int8_t best;
    uint32_t sweeps;
    uint32_t phase;
} HuntModel;

static int hunt_bar_height(const RcHuntBand* b) {
    if(!b->seen) return 0;
    int delta = (int)b->peak_dbm - (int)b->floor_dbm;
    if(delta <= 0) return 0;
    if(delta >= HV_FULL_SCALE_DB) return HV_HEIGHT;
    return delta * HV_HEIGHT / HV_FULL_SCALE_DB;
}

static void hunt_view_draw(Canvas* canvas, void* model) {
    HuntModel* m = model;
    canvas_clear(canvas);

    /* --- header --- */
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Find Band");
    canvas_set_font(canvas, FontSecondary);
    char sweeps[16];
    snprintf(sweeps, sizeof(sweeps), "%lu sweeps", (unsigned long)m->sweeps);
    canvas_draw_str_aligned(canvas, 126, 10, AlignRight, AlignBottom, sweeps);
    canvas_draw_line(canvas, 0, 12, 128, 12);

    /* --- per-band bars: how far each climbed over its own noise floor --- */
    canvas_draw_line(canvas, 0, HV_BOTTOM, 127, HV_BOTTOM);
    for(uint8_t i = 0; i < m->count; i++) {
        int x = 1 + i * HV_PITCH;
        int h = hunt_bar_height(&m->bands[i]);

        if(m->best >= 0 && i == (uint8_t)m->best) {
            /* The winner is solid, and wears a cap just above its own bar so it
             * reads at a glance. Clamped clear of the header rule at y=12. */
            if(h > 0) canvas_draw_box(canvas, x, HV_BOTTOM - h, HV_BAR_W, h);
            int cap = HV_BOTTOM - h - 3;
            if(cap < HV_TOP - 1) cap = HV_TOP - 1;
            canvas_draw_line(canvas, x, cap, x + HV_BAR_W - 1, cap);
        } else if(h > 0) {
            canvas_draw_frame(canvas, x, HV_BOTTOM - h, HV_BAR_W, h);
        } else {
            /* nothing heard yet - a single pixel so the slot is still visible */
            canvas_draw_dot(canvas, x + HV_BAR_W / 2, HV_BOTTOM - 1);
        }
    }

    /* --- verdict line --- */
    canvas_set_font(canvas, FontSecondary);
    char line[32];
    if(m->best >= 0 && m->best < (int8_t)m->count) {
        const RcHuntBand* b = &m->bands[m->best];
        int delta = (int)b->peak_dbm - (int)b->floor_dbm;
        snprintf(
            line, sizeof(line), "%s MHz  +%ddB", rc_bands[(uint8_t)m->best].label, delta);
    } else {
        char dots[4] = {0};
        int nd = (int)((m->phase / 3) % 4);
        for(int i = 0; i < nd; i++) dots[i] = '.';
        snprintf(line, sizeof(line), "Hold your remote down%s", dots);
    }
    canvas_draw_str(canvas, 2, 54, line);

    /* --- footer (inverted) --- */
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 56, 128, 8);
    canvas_set_color(canvas, ColorWhite);
    if(m->best >= 0) {
        canvas_draw_str(canvas, 3, 63, "OK: use this band");
    } else {
        canvas_draw_str(canvas, 3, 63, "Sweeping all bands");
    }
    canvas_draw_str_aligned(canvas, 125, 63, AlignRight, AlignBottom, "Back");
    canvas_set_color(canvas, ColorBlack);
}

static bool hunt_view_input(InputEvent* event, void* context) {
    HuntView* v = context;
    if(event->type == InputTypeShort && event->key == InputKeyOk) {
        bool ready = false;
        with_view_model(v->view, HuntModel * m, { ready = (m->best >= 0); }, false);
        if(ready && v->cb) v->cb(v->ctx, HuntEventAdopt);
        return true; // swallow OK either way; nothing else uses it here
    }
    return false; // Back propagates to the scene manager
}

HuntView* hunt_view_alloc(void) {
    HuntView* v = malloc(sizeof(HuntView));
    v->cb = NULL;
    v->ctx = NULL;
    v->view = view_alloc();
    view_set_context(v->view, v);
    view_allocate_model(v->view, ViewModelTypeLocking, sizeof(HuntModel));
    view_set_draw_callback(v->view, hunt_view_draw);
    view_set_input_callback(v->view, hunt_view_input);
    return v;
}

void hunt_view_free(HuntView* v) {
    furi_assert(v);
    view_free(v->view);
    free(v);
}

View* hunt_view_get_view(HuntView* v) {
    furi_assert(v);
    return v->view;
}

void hunt_view_set_callback(HuntView* v, HuntViewCallback cb, void* context) {
    furi_assert(v);
    v->cb = cb;
    v->ctx = context;
}

void hunt_view_set_data(
    HuntView* v,
    const RcHuntBand* bands,
    uint8_t count,
    int8_t best,
    uint32_t sweeps) {
    furi_assert(v);
    if(count > RC_BAND_COUNT) count = RC_BAND_COUNT;
    with_view_model(
        v->view,
        HuntModel * m,
        {
            memcpy(m->bands, bands, (size_t)count * sizeof(RcHuntBand));
            m->count = count;
            m->best = best;
            m->sweeps = sweeps;
        },
        true);
}

void hunt_view_reset(HuntView* v) {
    furi_assert(v);
    with_view_model(
        v->view,
        HuntModel * m,
        {
            memset(m->bands, 0, sizeof(m->bands));
            m->count = 0;
            m->best = -1;
            m->sweeps = 0;
            m->phase = 0;
        },
        true);
}

void hunt_view_tick(HuntView* v) {
    furi_assert(v);
    with_view_model(v->view, HuntModel * m, { m->phase++; }, true);
}
