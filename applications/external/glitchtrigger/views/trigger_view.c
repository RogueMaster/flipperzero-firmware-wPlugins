#include "trigger_view.h"
#include <gui/elements.h>
#include <furi.h>
#include <string.h>
#include <stdio.h>

struct TriggerView {
    View* view;
    TriggerViewCallback cb;
    void* ctx;
};

typedef struct {
    GlitchParams params;
    TriggerState state;
    uint32_t shots;
    uint8_t flash; // >0 for a few ticks right after a shot
    uint8_t blink; // free-running counter for the armed indicator
} TriggerModel;

/* -------- waveform geometry (schematic, not to scale) -------- */
#define WF_LEFT   4
#define WF_RIGHT  124
#define WF_HI     18 // "active" rail row
#define WF_LO     34 // "idle" rail row
#define WF_TRIG_X 9
#define WF_DELAY0 12
#define WF_DELAY1 46
#define WF_PW     8 // drawn pulse width (px)
#define WF_GAP    4 // drawn gap (px)
#define WF_MAXP   4 // pulses drawn before we switch to "xN"

static void draw_chip(Canvas* canvas, int x, int y, const char* txt, bool filled) {
    int w = canvas_string_width(canvas, txt) + 6;
    if(filled) {
        canvas_draw_rbox(canvas, x, y, w, 11, 2);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, x + 3, y + 9, txt);
        canvas_set_color(canvas, ColorBlack);
    } else {
        canvas_draw_rframe(canvas, x, y, w, 11, 2);
        canvas_draw_str(canvas, x + 3, y + 9, txt);
    }
}

static void draw_waveform(Canvas* canvas, TriggerModel* m) {
    const bool active_high = (m->params.polarity == GlitchPolarityActiveHigh);
    const int idle_y = active_high ? WF_LO : WF_HI;
    const int act_y = active_high ? WF_HI : WF_LO;

    /* trigger tick + label */
    bool tick_on = (m->state == TriggerStateIdle) || (m->blink & 1);
    if(tick_on) {
        canvas_draw_line(canvas, WF_TRIG_X, WF_HI - 3, WF_TRIG_X, WF_LO + 3);
        canvas_draw_line(canvas, WF_TRIG_X - 2, WF_HI - 1, WF_TRIG_X, WF_HI - 3);
        canvas_draw_line(canvas, WF_TRIG_X + 2, WF_HI - 1, WF_TRIG_X, WF_HI - 3);
    }

    /* idle rail up to the delay */
    canvas_draw_line(canvas, WF_LEFT, idle_y, WF_DELAY0, idle_y);

    /* delay: dashed rail (the "armed, counting down" region) */
    for(int x = WF_DELAY0; x < WF_DELAY1; x += 3) {
        canvas_draw_dot(canvas, x, idle_y);
    }

    /* pulse train */
    int x = WF_DELAY1;
    int drawn = m->params.pulses < WF_MAXP ? m->params.pulses : WF_MAXP;
    if(drawn < 1) drawn = 1;
    bool emph = m->flash > 0;
    for(int i = 0; i < drawn; i++) {
        canvas_draw_line(canvas, x, idle_y, x, act_y); // rising edge
        canvas_draw_line(canvas, x, act_y, x + WF_PW, act_y); // active top
        if(emph) canvas_draw_line(canvas, x, act_y + 1, x + WF_PW, act_y + 1);
        x += WF_PW;
        canvas_draw_line(canvas, x, act_y, x, idle_y); // falling edge
        if(i + 1 < drawn) {
            canvas_draw_line(canvas, x, idle_y, x + WF_GAP, idle_y);
            x += WF_GAP;
        }
    }
    /* trailing idle rail */
    canvas_draw_line(canvas, x, idle_y, WF_RIGHT, idle_y);

    /* spark on the most recent pulse when fired */
    if(emph) {
        int sx = WF_DELAY1 + WF_PW / 2;
        canvas_draw_disc(canvas, sx, act_y - 3, 1);
        canvas_draw_line(canvas, sx - 3, act_y - 6, sx + 3, act_y - 6);
    }
}

/* Compact one-line parameter readout under the waveform:  "100us  500ns  x1" */
static void draw_info_line(Canvas* canvas, TriggerModel* m) {
    char d[16], w[16], line[56];
    glitch_fmt_us(m->params.delay_us, d, sizeof(d));
    glitch_fmt_ns(m->params.width_ns, w, sizeof(w));
    snprintf(line, sizeof(line), "%s   %s   x%u", d, w, m->params.pulses);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 45, AlignCenter, AlignBottom, line);
}

static void trigger_view_draw(Canvas* canvas, void* model) {
    TriggerModel* m = model;
    canvas_clear(canvas);

    /* header: title + mode chip */
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Glitch");

    canvas_set_font(canvas, FontSecondary);
    const char* mode = glitch_trigger_label(m->params.trigger);
    int mw = canvas_string_width(canvas, mode) + 6;
    draw_chip(canvas, 126 - mw, 1, mode, false);
    canvas_draw_line(canvas, 0, 13, 128, 13);

    draw_waveform(canvas, m);
    draw_info_line(canvas, m);

    /* status band: state badge (left) + shot counter (right) */
    const char* badge = "IDLE";
    bool filled = false;
    switch(m->state) {
    case TriggerStateArmed:
        badge = "ARMED";
        filled = (m->blink & 1);
        break;
    case TriggerStateWaiting:
        badge = "WAIT EDGE";
        filled = (m->blink & 1);
        break;
    case TriggerStateRunning:
        badge = "RUN";
        filled = (m->blink & 1);
        break;
    default:
        break;
    }
    canvas_set_font(canvas, FontSecondary);
    draw_chip(canvas, 2, 48, badge, filled);

    char shots[20];
    snprintf(shots, sizeof(shots), "shots %lu", (unsigned long)m->shots);
    canvas_draw_str_aligned(canvas, 126, 57, AlignRight, AlignBottom, shots);

    /* bottom hint row */
    const char* hint = "OK: ARM";
    switch(m->state) {
    case TriggerStateArmed:
        hint = "OK: FIRE     Back: safe";
        break;
    case TriggerStateWaiting:
        hint = "waiting edge Back: safe";
        break;
    case TriggerStateRunning:
        hint = "OK: stop     Back: safe";
        break;
    default:
        break;
    }
    canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, hint);
}

static bool trigger_view_input(InputEvent* event, void* context) {
    TriggerView* v = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;
    if(!v->cb) return false;

    switch(event->key) {
    case InputKeyOk:
        v->cb(TriggerViewEventOk, v->ctx);
        return true;
    case InputKeyLeft:
        v->cb(TriggerViewEventLeft, v->ctx);
        return true;
    case InputKeyRight:
        v->cb(TriggerViewEventRight, v->ctx);
        return true;
    default:
        return false; // Back propagates to the scene manager
    }
}

TriggerView* trigger_view_alloc(void) {
    TriggerView* v = malloc(sizeof(TriggerView));
    v->cb = NULL;
    v->ctx = NULL;
    v->view = view_alloc();
    view_set_context(v->view, v);
    view_allocate_model(v->view, ViewModelTypeLocking, sizeof(TriggerModel));
    view_set_draw_callback(v->view, trigger_view_draw);
    view_set_input_callback(v->view, trigger_view_input);
    with_view_model(
        v->view,
        TriggerModel * m,
        {
            glitch_params_default(&m->params);
            m->state = TriggerStateIdle;
        },
        false);
    return v;
}

void trigger_view_free(TriggerView* v) {
    furi_assert(v);
    view_free(v->view);
    free(v);
}

View* trigger_view_get_view(TriggerView* v) {
    furi_assert(v);
    return v->view;
}

void trigger_view_set_callback(TriggerView* v, TriggerViewCallback cb, void* ctx) {
    furi_assert(v);
    v->cb = cb;
    v->ctx = ctx;
}

void trigger_view_set_params(TriggerView* v, const GlitchParams* p) {
    with_view_model(v->view, TriggerModel * m, { m->params = *p; }, true);
}

void trigger_view_set_state(TriggerView* v, TriggerState state) {
    with_view_model(v->view, TriggerModel * m, { m->state = state; }, true);
}

void trigger_view_set_shots(TriggerView* v, uint32_t shots) {
    with_view_model(v->view, TriggerModel * m, { m->shots = shots; }, true);
}

void trigger_view_flash(TriggerView* v) {
    with_view_model(v->view, TriggerModel * m, { m->flash = 4; }, true);
}

void trigger_view_tick(TriggerView* v) {
    with_view_model(
        v->view,
        TriggerModel * m,
        {
            m->blink++;
            if(m->flash > 0) m->flash--;
        },
        true);
}
