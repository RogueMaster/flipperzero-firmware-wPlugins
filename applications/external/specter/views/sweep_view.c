#include "sweep_view.h"
#include <furi.h>
#include <gui/gui.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

/* Gauge geometry (top semicircle, like an EMF / geiger meter) */
#define PCX   32 // pivot x
#define PCY   48 // pivot y
#define R_ARC 26
#define R_OUT 26
#define R_IN  22
#define R_NDL 23
#define R_SCN 19

struct SweepView {
    View* view;
    SweepViewCallback ok_cb;
    void* ok_ctx;
};

typedef struct {
    bool armed;
    bool error;
    bool present;
    uint8_t strength; // 0..100
    uint8_t peak; // 0..100
    uint32_t contacts;
    uint8_t history[SPECTER_HISTORY_LEN];
    uint8_t history_head;
    uint8_t anim;
    char sens[10];
} SweepModel;

static const char* proximity_word(uint8_t s) {
    if(s >= 70) return "STRONG";
    if(s >= 45) return "CLOSE";
    if(s >= 20) return "NEAR";
    return "FAINT";
}

/* value 0..100 -> point on the top semicircle (0% = left, 50% = up, 100% = right) */
static void gauge_point(uint8_t value, float radius, int* x, int* y) {
    if(value > 100) value = 100;
    float a = (float)M_PI * (1.0f - (float)value / 100.0f);
    *x = PCX + (int)(cosf(a) * radius);
    *y = PCY - (int)(sinf(a) * radius);
}

static void draw_error(Canvas* canvas) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 22, AlignCenter, AlignCenter, "NFC unavailable");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignCenter, "Close any other NFC app,");
    canvas_draw_str_aligned(canvas, 64, 48, AlignCenter, AlignCenter, "then re-open the sweep.");
}

static void sweep_view_draw(Canvas* canvas, void* model) {
    SweepModel* m = model;
    char buf[24];

    /* ---------- header ---------- */
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 9, "SPECTER");

    const char* state = m->error   ? "NFC BUSY" :
                        !m->armed  ? "IDLE" :
                        m->present ? "READER" :
                                     "SCANNING";
    canvas_draw_str_aligned(canvas, 116, 9, AlignRight, AlignBottom, state);
    if(m->present) {
        canvas_draw_disc(canvas, 123, 5, 2);
    } else {
        canvas_draw_circle(canvas, 123, 5, 2);
    }
    canvas_draw_line(canvas, 0, 11, 127, 11);

    if(m->error) {
        draw_error(canvas);
        return;
    }

    /* ---------- left: the EMF gauge ---------- */
    /* arc */
    int px = 0, py = 0;
    for(int v = 0; v <= 100; v += 3) {
        int ax, ay;
        gauge_point((uint8_t)v, R_ARC, &ax, &ay);
        if(v) canvas_draw_line(canvas, px, py, ax, ay);
        px = ax;
        py = ay;
    }
    /* ticks (top third = danger zone, drawn bolder) */
    for(int i = 0; i <= 10; i++) {
        uint8_t v = (uint8_t)(i * 10);
        bool hot = i >= 8;
        int ox, oy, ix, iy;
        gauge_point(v, R_OUT, &ox, &oy);
        gauge_point(v, hot ? R_IN - 3 : R_IN, &ix, &iy);
        canvas_draw_line(canvas, ix, iy, ox, oy);
        if(hot) canvas_draw_line(canvas, ix + 1, iy, ox + 1, oy);
    }

    /* scanner bug travelling the arc while idle-scanning */
    if(m->armed && !m->present) {
        uint8_t scan = (uint8_t)((m->anim * 4u) % 101u);
        int sx, sy;
        gauge_point(scan, R_SCN, &sx, &sy);
        canvas_draw_circle(canvas, sx, sy, 1);
    }

    /* needle */
    int tx, ty;
    gauge_point(m->strength, R_NDL, &tx, &ty);
    canvas_draw_line(canvas, PCX, PCY, tx, ty);
    canvas_draw_line(canvas, PCX - 1, PCY, tx, ty);
    canvas_draw_disc(canvas, tx, ty, 1);
    /* peak-hold marker */
    int kx, ky;
    gauge_point(m->peak, R_OUT - 1, &kx, &ky);
    canvas_draw_disc(canvas, kx, ky, 1);
    /* hub */
    canvas_draw_disc(canvas, PCX, PCY, 3);

    /* throb ring when a reader is locked on */
    if(m->present) {
        canvas_draw_circle(canvas, PCX, PCY, R_OUT + 1 + (m->anim % 3));
    }

    /* ---------- right: numeric readout ---------- */
    canvas_draw_line(canvas, 64, 13, 64, 51);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 68, 20, "FIELD");

    canvas_set_font(canvas, FontBigNumbers);
    snprintf(buf, sizeof(buf), "%u", (unsigned)m->strength);
    canvas_draw_str_aligned(canvas, 112, 45, AlignRight, AlignBottom, buf);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 114, 43, "%");

    snprintf(buf, sizeof(buf), "PK%u C%lu", (unsigned)m->peak, (unsigned long)m->contacts);
    canvas_draw_str(canvas, 68, 51, buf);

    /* ---------- bottom strip ---------- */
    canvas_draw_line(canvas, 0, 52, 127, 52);
    if(m->present) {
        canvas_draw_box(canvas, 0, 53, 128, 11);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_disc(canvas, 4, 58, 1);
        canvas_draw_str(canvas, 9, 62, "ACTIVE READER");
        canvas_draw_str_aligned(
            canvas, 125, 62, AlignRight, AlignBottom, proximity_word(m->strength));
        canvas_set_color(canvas, ColorBlack);
        /* alarm frame */
        canvas_draw_frame(canvas, 0, 0, 128, 64);
        canvas_draw_frame(canvas, 1, 1, 126, 62);
    } else {
        /* live waveform of recent field strength */
        for(int k = 0; k < 62; k++) {
            int idx = (m->history_head - k + 2 * SPECTER_HISTORY_LEN) % SPECTER_HISTORY_LEN;
            int v = m->history[idx];
            int x = 126 - k * 2;
            int y = 63 - (v * 9) / 100;
            if(y < 63)
                canvas_draw_line(canvas, x, 63, x, y);
            else
                canvas_draw_dot(canvas, x, 63);
        }
    }
}

static bool sweep_view_input(InputEvent* event, void* context) {
    SweepView* v = context;
    if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(v->ok_cb) v->ok_cb(v->ok_ctx);
        return true;
    }
    return false;
}

SweepView* sweep_view_alloc(void) {
    SweepView* v = malloc(sizeof(SweepView));
    v->ok_cb = NULL;
    v->ok_ctx = NULL;
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

void sweep_view_update(SweepView* v, const FieldStats* stats, const char* sens_label) {
    furi_assert(v);
    with_view_model(
        v->view,
        SweepModel * m,
        {
            m->armed = stats->armed;
            m->error = stats->error;
            m->present = stats->present;
            m->strength = stats->strength;
            m->peak = stats->peak;
            m->contacts = stats->contacts;
            memcpy(m->history, stats->history, sizeof(m->history));
            m->history_head = stats->history_head;
            if(sens_label) {
                strncpy(m->sens, sens_label, sizeof(m->sens) - 1);
                m->sens[sizeof(m->sens) - 1] = '\0';
            }
        },
        true);
}

void sweep_view_tick(SweepView* v) {
    furi_assert(v);
    with_view_model(v->view, SweepModel * m, { m->anim++; }, true);
}
