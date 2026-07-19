#include "meter_view.h"
#include "trident_icons.h"
#include <furi.h>
#include <string.h>

/*
 * Layout (128x64):
 *   y 0..11   header : icon + title + LIVE/IDLE dot
 *   y 14..34  big numeric readout + unit
 *   y 38..47  segmented bar meter with peak tick
 *   y 50..57  sub label (frequency / channel)
 *   y 58..63  footer hint
 */
#define METER_SEGS   24
#define METER_X0     3
#define METER_SEG_W  4
#define METER_SEG_GAP 1
#define METER_Y      39
#define METER_H      9

struct MeterView {
    View* view;
    MeterViewInputCb input_cb;
    void* input_ctx;
};

typedef struct {
    MeterSnapshot snap;
    uint8_t anim;
} MeterModel;

static void meter_view_draw(Canvas* canvas, void* model) {
    MeterModel* m = model;
    const MeterSnapshot* s = &m->snap;

    /* header */
    canvas_draw_icon(canvas, 0, 1, &I_trident_10px);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 13, 9, s->title[0] ? s->title : "Finder");
    const char* state = s->running ? "LIVE" : "IDLE";
    canvas_draw_str_aligned(canvas, 110, 9, AlignRight, AlignBottom, state);
    if(s->running && (m->anim & 1)) {
        canvas_draw_disc(canvas, 124, 5, 2);
    } else {
        canvas_draw_circle(canvas, 124, 5, 2);
    }
    canvas_draw_line(canvas, 0, 12, 127, 12);

    if(!s->present) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 6, 34, "Radio not detected.");
        canvas_draw_str(canvas, 6, 46, "Check the board & wiring.");
        return;
    }

    /* big readout */
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(canvas, 60, 27, AlignRight, AlignCenter, s->value[0] ? s->value : "0");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 64, 24, s->unit);
    if(s->count > 0) {
        char c[16];
        snprintf(c, sizeof(c), "%lu pkt", (unsigned long)s->count);
        canvas_draw_str(canvas, 64, 34, c);
    }

    /* segmented bar meter */
    int level = s->level;
    if(level < 0) level = 0;
    if(level > 100) level = 100;
    int peak = s->peak;
    if(peak < 0) peak = 0;
    if(peak > 100) peak = 100;
    int lit = level * METER_SEGS / 100;
    int peak_seg = (peak * METER_SEGS - 1) / 100;
    for(int i = 0; i < METER_SEGS; i++) {
        int x = METER_X0 + i * (METER_SEG_W + METER_SEG_GAP);
        if(i < lit) {
            canvas_draw_box(canvas, x, METER_Y, METER_SEG_W, METER_H);
        } else if(i == peak_seg && peak > 0) {
            canvas_draw_frame(canvas, x, METER_Y, METER_SEG_W, METER_H);
        } else {
            canvas_draw_dot(canvas, x + METER_SEG_W / 2, METER_Y + METER_H - 1);
        }
    }

    /* sub label */
    canvas_set_font(canvas, FontSecondary);
    if(s->sub[0]) canvas_draw_str_aligned(canvas, 64, 55, AlignCenter, AlignBottom, s->sub);

    /* footer */
    if(s->foot[0]) canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, s->foot);
}

static bool meter_view_input(InputEvent* event, void* context) {
    MeterView* v = context;
    if((event->type == InputTypeShort || event->type == InputTypeRepeat) && v->input_cb) {
        switch(event->key) {
        case InputKeyUp:
        case InputKeyDown:
        case InputKeyLeft:
        case InputKeyRight:
            v->input_cb(v->input_ctx, event->key);
            return true;
        case InputKeyOk:
            if(event->type == InputTypeShort) {
                v->input_cb(v->input_ctx, InputKeyOk);
                return true;
            }
            return false;
        default:
            return false;
        }
    }
    return false;
}

MeterView* meter_view_alloc(void) {
    MeterView* v = malloc(sizeof(MeterView));
    v->input_cb = NULL;
    v->input_ctx = NULL;
    v->view = view_alloc();
    view_set_context(v->view, v);
    view_set_draw_callback(v->view, meter_view_draw);
    view_set_input_callback(v->view, meter_view_input);
    view_allocate_model(v->view, ViewModelTypeLocking, sizeof(MeterModel));
    with_view_model(
        v->view, MeterModel * m, { memset(m, 0, sizeof(MeterModel)); }, false);
    return v;
}

void meter_view_free(MeterView* v) {
    furi_assert(v);
    view_free(v->view);
    free(v);
}

View* meter_view_get_view(MeterView* v) {
    furi_assert(v);
    return v->view;
}

void meter_view_set_input_callback(MeterView* v, MeterViewInputCb cb, void* context) {
    furi_assert(v);
    v->input_cb = cb;
    v->input_ctx = context;
}

void meter_view_set_snapshot(MeterView* v, const MeterSnapshot* snap) {
    furi_assert(v);
    furi_assert(snap);
    with_view_model(
        v->view,
        MeterModel * m,
        {
            m->snap = *snap;
            m->anim++;
        },
        true);
}
