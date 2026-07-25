#include "meter_view.h"
#include "../helpers/fdy_grade.h"

#include <furi.h>
#include <gui/gui.h>
#include <stdio.h>
#include <string.h>

struct MeterView {
    View* view;
    MeterViewOkCallback ok_cb;
    void* ok_ctx;
};

/* Frames over which the verdict animates in (at the 100 ms scene tick). */
#define FDY_REVEAL_MAX 6

typedef struct {
    MeterData d;
    uint8_t history[FDY_HISTORY_LEN];
    bool has_history;
    uint8_t anim;
    uint8_t reveal; // verdict reveal progress, 0..FDY_REVEAL_MAX
    uint8_t last_phase; // to detect the entry into the verdict face
} MeterModel;

/* ---------------- small drawing helpers ---------------- */

/* Right-aligned FontBigNumbers value with a hand-drawn minus (the big-number
 * font has no '-' glyph) and a small unit label sitting above the last digit. */
static void
    draw_big_value(Canvas* canvas, int x_right, int baseline, int value, const char* unit) {
    char buf[12]; // worst-case int, not worst-case RSSI: -Werror=format-truncation
    int mag = value < 0 ? -value : value;
    snprintf(buf, sizeof(buf), "%d", mag);

    canvas_set_font(canvas, FontBigNumbers);
    int w = canvas_string_width(canvas, buf);
    int x = x_right - w;
    canvas_draw_str_aligned(canvas, x_right, baseline, AlignRight, AlignBottom, buf);

    if(value < 0) {
        canvas_draw_box(canvas, x - 8, baseline - 8, 5, 2); // minus bar
    }
    if(unit) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, x_right, baseline - 17, AlignRight, AlignBottom, unit);
    }
}

/* A framed horizontal bar with a proportional fill and a peak-hold tick. */
static void draw_bar(Canvas* canvas, int x, int y, int w, int h, uint8_t fill_pct, int peak_pct) {
    canvas_draw_frame(canvas, x, y, w, h);
    int inner = w - 4;
    int fw = (inner * (fill_pct > 100 ? 100 : fill_pct)) / 100;
    if(fw > 0) canvas_draw_box(canvas, x + 2, y + 2, fw, h - 4);
    if(peak_pct >= 0) {
        int px = x + 2 + (inner * (peak_pct > 100 ? 100 : peak_pct)) / 100;
        canvas_draw_line(canvas, px, y - 1, px, y + h);
    }
}

static void draw_pips(Canvas* canvas, int x, int y, uint8_t filled) {
    for(int i = 0; i < 5; i++) {
        int px = x + i * 5;
        if(i < filled)
            canvas_draw_box(canvas, px, y, 3, 3);
        else
            canvas_draw_frame(canvas, px, y, 3, 3);
    }
}

/* A compact sparkline of the recent signal, drawn from the history ring. */
static void draw_sparkline(Canvas* canvas, const MeterData* d, int x0, int baseline, int cols) {
    if(!d->history) return;
    for(int k = 0; k < cols; k++) {
        int idx = (d->history_head - k + 2 * FDY_HISTORY_LEN) % FDY_HISTORY_LEN;
        int v = d->history[idx];
        int x = x0 + (cols - 1 - k) * 2;
        int h = (v * 7) / 100;
        if(h > 0)
            canvas_draw_line(canvas, x, baseline, x, baseline - h);
        else
            canvas_draw_dot(canvas, x, baseline);
    }
}

/* ---------------- faces ---------------- */

static void draw_header(Canvas* canvas, const MeterData* d) {
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 9, "FARADAY");
    if(d->band) canvas_draw_str_aligned(canvas, 126, 9, AlignRight, AlignBottom, d->band);
    canvas_draw_line(canvas, 0, 11, 127, 11);
}

static void draw_error_face(Canvas* canvas, const MeterData* d) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(
        canvas, 64, 30, AlignCenter, AlignCenter, d->err1 ? d->err1 : "Radio busy");
    canvas_set_font(canvas, FontSecondary);
    if(d->err2) canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignCenter, d->err2);
}

static void draw_capture_face(Canvas* canvas, const MeterModel* m) {
    const MeterData* d = &m->d;
    const char* unit = d->is_nfc ? "%" : "dBm";
    char buf[24];

    /* phase pill */
    const char* name = (d->phase == FdyPhaseBaseline) ? "BASELINE" : "SHIELDED";
    const char* sub = (d->phase == FdyPhaseBaseline) ? "open air" : "in pouch";
    canvas_set_font(canvas, FontPrimary);
    int pw = canvas_string_width(canvas, name);
    canvas_draw_rbox(canvas, 2, 13, pw + 7, 12, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str(canvas, 6, 23, name);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 126, 23, AlignRight, AlignBottom, sub);

    /* live meter bar + peak tick */
    draw_bar(canvas, 2, 30, 70, 12, d->level, d->peak);

    /* While there is nothing to lock onto, a marker sweeps the empty bar so the
     * screen reads as actively listening rather than frozen. */
    if(!d->signal_ok) {
        const int span = 62; // inner sweep width
        int pos = (m->anim * 4) % (2 * span);
        if(pos > span) pos = 2 * span - pos;
        canvas_draw_box(canvas, 4 + pos, 34, 2, 4);
    }

    /* big live readout on the right */
    if(d->signal_ok || d->live_value != 0) {
        draw_big_value(canvas, 126, 47, d->live_value, unit);
    } else {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 126, 40, AlignRight, AlignBottom, "waiting");
        canvas_draw_str_aligned(canvas, 126, 48, AlignRight, AlignBottom, "for signal");
    }

    if(d->phase == FdyPhaseShield && d->have_base) {
        /* locked baseline reminder while capturing the shielded level */
        snprintf(buf, sizeof(buf), "BASE %d %s", d->base_value, unit);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 51, buf);
    } else if(d->phase == FdyPhaseBaseline) {
        /* live sparkline of the signal under the bar */
        draw_sparkline(canvas, d, 2, 51, 34);
    }

    /* Bottom action strip. It doubles as the signal cue: until a carrier has
     * actually risen out of the noise there is nothing worth locking, so the
     * strip keeps prompting and only then confirms. */
    canvas_draw_box(canvas, 0, 53, 128, 11);
    canvas_set_color(canvas, ColorWhite);
    const char* hint;
    if(d->signal_ok)
        hint = "Peak captured";
    else
        hint = d->is_nfc ? "In reader field" : "Press remote";
    canvas_draw_str(canvas, 3, 62, hint);
    canvas_draw_str_aligned(canvas, 125, 62, AlignRight, AlignBottom, "OK lock");
    canvas_set_color(canvas, ColorBlack);
}

static void draw_verdict_face(Canvas* canvas, const MeterModel* m) {
    const MeterData* d = &m->d;
    FdyRating rating = (FdyRating)d->rating;
    /* The headline is a DIFFERENCE of two dBm readings, so it is dB - not dBm.
     * The bars below show the absolute levels and print no unit. */
    const char* unit = d->is_nfc ? "%" : "dB";
    char buf[16];

    /* Reveal factor 0..FDY_REVEAL_MAX: the bars grow, the number tallies up and
     * the pips fill as the screen lands, so a result feels earned rather than
     * just appearing. Everything is static once fully revealed. */
    uint8_t r = m->reveal;
    int base_grow = (d->base_norm * r) / FDY_REVEAL_MAX;
    int shield_grow = (d->shield_norm * r) / FDY_REVEAL_MAX;

    /* comparison bars: OPEN vs BAG */
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 22, "OPEN");
    draw_bar(canvas, 30, 15, 68, 8, (uint8_t)base_grow, -1);
    snprintf(buf, sizeof(buf), "%d", d->base_value);
    canvas_draw_str_aligned(canvas, 126, 22, AlignRight, AlignBottom, buf);

    canvas_draw_str(canvas, 2, 32, "BAG");
    draw_bar(canvas, 30, 25, 68, 8, (uint8_t)shield_grow, -1);
    snprintf(buf, sizeof(buf), "%d", d->shield_value);
    canvas_draw_str_aligned(canvas, 126, 32, AlignRight, AlignBottom, buf);

    /* Attenuation headline (left). FontBigNumbers is ~19px tall and the gap
     * between the bars and the bottom strip is only 19px, so the number owns
     * that band outright - the caption rides the same baseline to its right
     * rather than sitting above it. */
    int target = d->atten < 0 ? 0 : d->atten;
    int shown = (target * r) / FDY_REVEAL_MAX; // count up
    snprintf(buf, sizeof(buf), "%d", shown);

    /* ">=" means the shielded reading was buried in the noise floor, so the
     * pouch is at least this good and possibly better. */
    int nx = 2;
    if(d->atten_floored) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 52, ">=");
        nx = 15;
    }
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str(canvas, nx, 53, buf);
    int nw = canvas_string_width(canvas, buf);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, nx + nw + 2, 52, unit);
    canvas_draw_str_aligned(
        canvas, 92, 52, AlignRight, AlignBottom, d->is_nfc ? "BLOCKED" : "ATTEN");

    /* Grade badge (right). A pass (B or better) is filled so it reads as a
     * reward; a poor grade is only outlined so it reads as a warning. */
    bool good = rating <= FdyRatingB;
    if(good) {
        canvas_draw_rbox(canvas, 96, 34, 31, 18, 3);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, 96, 34, 31, 18, 3);
    }
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 111, 43, AlignCenter, AlignCenter, fdy_rating_letter(rating));
    canvas_set_color(canvas, ColorBlack);

    /* A top grade earns a little sparkle at the badge corners, once revealed. */
    if(rating == FdyRatingAPlus && r >= FDY_REVEAL_MAX) {
        canvas_draw_line(canvas, 93, 33, 95, 35);
        canvas_draw_line(canvas, 128, 33, 126, 35);
        canvas_draw_dot(canvas, 91, 31);
    }

    /* inverted verdict strip: word + pips (left), retest hint (right) */
    canvas_draw_box(canvas, 0, 53, 128, 11);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    const char* word = fdy_rating_word(rating);
    canvas_draw_str(canvas, 3, 62, word);
    int ww = canvas_string_width(canvas, word);
    uint8_t pips = (uint8_t)((fdy_rating_pips(rating) * r) / FDY_REVEAL_MAX);
    draw_pips(canvas, 6 + ww, 57, pips);
    canvas_draw_str_aligned(canvas, 125, 62, AlignRight, AlignBottom, "OK retest");
    canvas_set_color(canvas, ColorBlack);
}

static void meter_view_draw(Canvas* canvas, void* model) {
    MeterModel* m = model;
    draw_header(canvas, &m->d);
    switch(m->d.phase) {
    case FdyPhaseError:
        draw_error_face(canvas, &m->d);
        break;
    case FdyPhaseVerdict:
        draw_verdict_face(canvas, m);
        break;
    default:
        draw_capture_face(canvas, m);
        break;
    }
}

static bool meter_view_input(InputEvent* event, void* context) {
    MeterView* v = context;
    if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(v->ok_cb) v->ok_cb(v->ok_ctx);
        return true;
    }
    return false;
}

MeterView* meter_view_alloc(void) {
    MeterView* v = malloc(sizeof(MeterView));
    v->ok_cb = NULL;
    v->ok_ctx = NULL;
    v->view = view_alloc();
    view_set_context(v->view, v);
    view_set_draw_callback(v->view, meter_view_draw);
    view_set_input_callback(v->view, meter_view_input);
    view_allocate_model(v->view, ViewModelTypeLocking, sizeof(MeterModel));
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

void meter_view_set_ok_callback(MeterView* v, MeterViewOkCallback cb, void* context) {
    furi_assert(v);
    v->ok_cb = cb;
    v->ok_ctx = context;
}

void meter_view_update(MeterView* v, const MeterData* data) {
    furi_assert(v);
    with_view_model(
        v->view,
        MeterModel * m,
        {
            uint8_t prev_phase = m->last_phase;
            m->d = *data;
            if(data->history) {
                memcpy(m->history, data->history, sizeof(m->history));
                m->has_history = true;
                m->d.history = m->history; // point at our own copy
            } else {
                m->has_history = false;
                m->d.history = NULL;
            }
            /* Restart the reveal animation each time we land on the verdict. */
            if(m->d.phase == FdyPhaseVerdict && prev_phase != FdyPhaseVerdict) m->reveal = 0;
            m->last_phase = m->d.phase;
        },
        true);
}

void meter_view_tick(MeterView* v) {
    furi_assert(v);
    with_view_model(
        v->view,
        MeterModel * m,
        {
            m->anim++;
            if(m->last_phase == FdyPhaseVerdict && m->reveal < FDY_REVEAL_MAX) m->reveal++;
        },
        true);
}
