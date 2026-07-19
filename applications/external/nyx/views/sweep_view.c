#include "sweep_view.h"
#include <furi.h>
#include <gui/gui.h>
#include <stdio.h>
#include <string.h>

/* The sweep screen is a locating instrument, not a dashboard. Everything on it
 * answers one of two questions you ask while walking a room: "is there IR here"
 * and "am I getting warmer". Hence the trend arrow next to the level, and the
 * scrolling trace with a peak-hold line across it — you find an emitter by
 * watching the trace climb, not by reading the number.
 *
 * Layout of the 128x64:
 *   y 0..11   header: mark, active mode, live dot
 *   y 12..35  readout: level, trend arrow, source kind, peak/hits
 *   y 37..51  trace with peak-hold line
 *   y 53..63  status strip, inverted into an alarm when locked on
 */

#define TRACE_BASE_Y 51
#define TRACE_H      13

struct SweepView {
    View* view;
    SweepViewCallback ok_cb;
    void* ok_ctx;
    SweepViewCallback long_ok_cb;
    void* long_ok_ctx;
};

typedef struct {
    bool armed;
    IrSenseError error;
    IrSenseMode active_mode;
    bool present;
    uint8_t level;
    uint8_t peak;
    int8_t trend;
    IrSourceKind kind;
    uint32_t hits;
    uint16_t baseline_mv;
    uint16_t ripple_mv;
    uint8_t trace[NYX_TRACE_LEN];
    uint8_t trace_head;
    uint8_t anim;
} SweepModel;

static const char* proximity_word(uint8_t level) {
    if(level >= 70) return "STRONG";
    if(level >= 45) return "CLOSE";
    if(level >= 20) return "NEAR";
    return "FAINT";
}

static void draw_error(Canvas* canvas, const SweepModel* m) {
    canvas_set_font(canvas, FontPrimary);

    if(m->error == IrSenseErrorIrBusy) {
        canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignCenter, "IR receiver busy");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignCenter, "Close any other IR app,");
        canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignCenter, "then re-open the sweep.");
    } else if(m->error == IrSenseErrorNoProbe) {
        canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignCenter, "No probe on the pin");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignCenter, "Check Probe Setup wiring,");
        canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignCenter, "or set Mode to Auto.");
    } else {
        canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignCenter, "ADC unavailable");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignCenter, "Another app holds it.");
    }
}

static void draw_nulling(Canvas* canvas, const SweepModel* m) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignCenter, "Nulling ambient");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignCenter, "Hold still, aim at the room");
    /* three dots filling in, so the wait reads as progress not a hang */
    for(uint8_t i = 0; i < 3; i++) {
        int x = 56 + i * 8;
        if((m->anim / 3u) % 3u >= i) {
            canvas_draw_disc(canvas, x, 52, 2);
        } else {
            canvas_draw_circle(canvas, x, 52, 2);
        }
    }
}

static void draw_trend(Canvas* canvas, int8_t trend) {
    if(trend > 0) {
        canvas_draw_triangle(canvas, 58, 30, 11, 9, CanvasDirectionBottomToTop);
    } else if(trend < 0) {
        canvas_draw_triangle(canvas, 58, 21, 11, 9, CanvasDirectionTopToBottom);
    } else {
        canvas_draw_box(canvas, 53, 24, 11, 3);
    }
}

static void draw_trace(Canvas* canvas, const SweepModel* m) {
    for(uint8_t k = 0; k < NYX_TRACE_LEN; k++) {
        uint8_t idx = (uint8_t)((m->trace_head + NYX_TRACE_LEN - k) % NYX_TRACE_LEN);
        int h = (m->trace[idx] * TRACE_H) / 100;
        int x = 126 - k * 2;
        if(h > 0) {
            canvas_draw_line(canvas, x, TRACE_BASE_Y, x, TRACE_BASE_Y - h);
        } else {
            canvas_draw_dot(canvas, x, TRACE_BASE_Y);
        }
    }
    /* peak-hold: the line to beat while you hunt for the hot spot */
    if(m->peak > 0) {
        int py = TRACE_BASE_Y - (m->peak * TRACE_H) / 100;
        for(int x = 0; x < 128; x += 4) canvas_draw_dot(canvas, x, py);
    }
}

/* Idle hint line. In onboard mode this is where Nyx keeps admitting what it
 * cannot see, because a clean-looking zero on this screen is exactly the
 * reading a DC illuminator produces. */
static void draw_hint(Canvas* canvas, const SweepModel* m) {
    char buf[32];
    const char* text;
    uint8_t phase = (uint8_t)((m->anim / 30u) % 3u); // ~3 s per phase at a 100 ms tick

    if(!m->armed) {
        text = "Idle";
    } else if(m->active_mode == IrSenseModeOnboard) {
        if(phase == 0) {
            text = "Pan slowly across walls";
        } else if(phase == 1) {
            text = "Onboard: pulsed IR only";
        } else {
            text = "OK: zero peak";
        }
    } else {
        if(phase == 0) {
            text = "Pan slowly across walls";
        } else if(phase == 1) {
            snprintf(
                buf,
                sizeof(buf),
                "amb %umV  rip %umV",
                (unsigned)m->baseline_mv,
                (unsigned)m->ripple_mv);
            text = buf;
        } else {
            text = "OK: zero  Hold OK: null";
        }
    }
    canvas_draw_str(canvas, 2, 62, text);
}

static void sweep_view_draw(Canvas* canvas, void* model) {
    SweepModel* m = model;
    char buf[24];

    /* ---------- header ---------- */
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 9, "NYX");

    const char* mode_word = (m->active_mode == IrSenseModeProbe) ? "PROBE" : "ONBOARD";
    canvas_draw_str_aligned(canvas, 116, 9, AlignRight, AlignBottom, mode_word);
    if(m->present) {
        canvas_draw_disc(canvas, 123, 5, 2);
    } else {
        canvas_draw_circle(canvas, 123, 5, 2);
    }
    canvas_draw_line(canvas, 0, 11, 127, 11);

    if(m->error != IrSenseErrorNone) {
        draw_error(canvas, m);
        return;
    }

    /* Probe mode holds the meter back until the ambient null is captured —
     * a reading before then would be measuring the room, not the emitter. */
    if(m->armed && m->active_mode == IrSenseModeProbe && m->baseline_mv == 0) {
        draw_nulling(canvas, m);
        return;
    }

    /* ---------- readout ---------- */
    canvas_set_font(canvas, FontBigNumbers);
    snprintf(buf, sizeof(buf), "%u", (unsigned)m->level);
    canvas_draw_str_aligned(canvas, 42, 34, AlignRight, AlignBottom, buf);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 44, 33, "%");

    draw_trend(canvas, m->trend);

    canvas_draw_line(canvas, 68, 13, 68, 34);
    canvas_draw_str(canvas, 72, 22, ir_sense_source_kind_str(m->kind));
    snprintf(buf, sizeof(buf), "PK%u H%lu", (unsigned)m->peak, (unsigned long)m->hits);
    canvas_draw_str(canvas, 72, 33, buf);

    /* ---------- trace ---------- */
    canvas_draw_line(canvas, 0, 36, 127, 36);
    draw_trace(canvas, m);

    /* ---------- status strip ---------- */
    canvas_draw_line(canvas, 0, 52, 127, 52);
    if(m->present) {
        canvas_draw_box(canvas, 0, 53, 128, 11);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_disc(canvas, 4, 58, 1);
        canvas_draw_str(canvas, 9, 62, "IR EMITTER");
        canvas_draw_str_aligned(
            canvas, 125, 62, AlignRight, AlignBottom, proximity_word(m->level));
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_frame(canvas, 0, 0, 128, 64);
        canvas_draw_frame(canvas, 1, 1, 126, 62);
    } else {
        draw_hint(canvas, m);
    }
}

static bool sweep_view_input(InputEvent* event, void* context) {
    SweepView* v = context;
    if(event->key != InputKeyOk) return false;

    if(event->type == InputTypeShort) {
        if(v->ok_cb) v->ok_cb(v->ok_ctx);
        return true;
    }
    if(event->type == InputTypeLong) {
        if(v->long_ok_cb) v->long_ok_cb(v->long_ok_ctx);
        return true;
    }
    return false;
}

SweepView* sweep_view_alloc(void) {
    SweepView* v = malloc(sizeof(SweepView));
    memset(v, 0, sizeof(SweepView));
    v->view = view_alloc();
    view_set_context(v->view, v);
    view_set_draw_callback(v->view, sweep_view_draw);
    view_set_input_callback(v->view, sweep_view_input);
    view_allocate_model(v->view, ViewModelTypeLocking, sizeof(SweepModel));
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

void sweep_view_set_ok_callback(SweepView* v, SweepViewCallback cb, void* context) {
    furi_assert(v);
    v->ok_cb = cb;
    v->ok_ctx = context;
}

void sweep_view_set_long_ok_callback(SweepView* v, SweepViewCallback cb, void* context) {
    furi_assert(v);
    v->long_ok_cb = cb;
    v->long_ok_ctx = context;
}

void sweep_view_update(SweepView* v, const IrStats* stats) {
    furi_assert(v);
    with_view_model(
        v->view,
        SweepModel * m,
        {
            m->armed = stats->armed;
            m->error = stats->error;
            m->active_mode = stats->active_mode;
            m->present = stats->present;
            m->level = stats->level;
            m->peak = stats->peak;
            m->trend = stats->trend;
            m->kind = stats->kind;
            m->hits = stats->hits;
            m->baseline_mv = stats->baseline_mv;
            m->ripple_mv = stats->ripple_mv;
            memcpy(m->trace, stats->trace, sizeof(m->trace));
            m->trace_head = stats->trace_head;
        },
        true);
}

void sweep_view_tick(SweepView* v) {
    furi_assert(v);
    with_view_model(v->view, SweepModel * m, { m->anim++; }, true);
}
