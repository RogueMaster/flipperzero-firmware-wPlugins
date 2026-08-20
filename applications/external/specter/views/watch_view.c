#include "watch_view.h"
#include <furi.h>
#include <gui/gui.h>
#include <stdio.h>
#include <string.h>

/* Watch mode: leave the Flipper somewhere and walk away. The screen is a
 * standing-guard readout - a big elapsed clock, a running detection count, and
 * when the last contact was - built to be legible at a glance from across a
 * room, then confirmed up close. The alarm and the wake-on-detection backlight
 * live in the scene; this view just draws the state. */

#define CLOCK_BASE 34 // FontBigNumbers baseline for the elapsed clock
#define STATUS_Y   14
#define STATUS_H   14
#define FOOT1_BASE 50
#define FOOT2_BASE 61
#define COL_RIGHT  66

struct WatchView {
    View* view;
    WatchViewCallback reset_cb;
    void* reset_ctx;
};

typedef struct {
    bool armed;
    bool error;
    bool present;
    uint8_t strength;
    uint8_t peak;
    uint32_t contacts;
    uint32_t watching_ms;
    uint32_t first_ms;
    uint32_t last_ms;
    uint32_t in_field_ms; // total time a carrier was actually up this watch
    uint8_t anim;
} WatchModel;

/* mm:ss, saturating at 99:59 so it can never overrun its slot. */
static void fmt_clock(char* out, size_t n, uint32_t ms) {
    uint32_t s = ms / 1000u;
    if(s > 99u * 60u + 59u) s = 99u * 60u + 59u;
    snprintf(out, n, "%02lu:%02lu", (unsigned long)(s / 60u), (unsigned long)(s % 60u));
}

/* "1m20s ago" style, compact enough for the footer. */
static void fmt_ago(char* out, size_t n, uint32_t now_ms, uint32_t then_ms) {
    if(then_ms == WATCH_NO_TIME || now_ms < then_ms) {
        snprintf(out, n, "--");
        return;
    }
    uint32_t s = (now_ms - then_ms) / 1000u;
    if(s < 60u) {
        snprintf(out, n, "%lus", (unsigned long)s);
    } else if(s < 3600u) {
        snprintf(out, n, "%lum%lus", (unsigned long)(s / 60u), (unsigned long)(s % 60u));
    } else {
        snprintf(out, n, "%luh%lum", (unsigned long)(s / 3600u), (unsigned long)((s / 60u) % 60u));
    }
}

static void watch_view_draw(Canvas* canvas, void* model) {
    WatchModel* m = model;
    char buf[24];

    /* ---------- header ---------- */
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 9, "WATCH");
    const char* hdr = m->error ? "NFC BUSY" : !m->armed ? "IDLE" : "ARMED";
    canvas_draw_str_aligned(canvas, 126, 9, AlignRight, AlignBottom, hdr);
    canvas_draw_line(canvas, 0, 11, 127, 11);

    if(m->error) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, "NFC unavailable");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas, 64, 46, AlignCenter, AlignCenter, "Close any other NFC app.");
        return;
    }

    /* ---------- status band ---------- */
    bool alarm = m->present;
    if(alarm) {
        /* The band stays solidly inverted. It used to alternate between filled
         * and outlined on every tick, which at a 100 ms tick is a 5 Hz strobe
         * across the full width of the screen - unpleasant to look at, hard to
         * read, and no more attention-grabbing than a steady block.
         *
         * The "this is live, not frozen" cue is a single small marker pulsing
         * at about 1 Hz instead. Same job, none of the flicker. */
        canvas_draw_box(canvas, 0, STATUS_Y - 1, 128, STATUS_H);
        canvas_set_color(canvas, ColorWhite);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(
            canvas, 64, STATUS_Y + 9, AlignCenter, AlignBottom, "READER PRESENT");
        if((m->anim / 5u) & 1u) {
            canvas_draw_disc(canvas, 6, STATUS_Y + 5, 2);
            canvas_draw_disc(canvas, 121, STATUS_Y + 5, 2);
        }
        canvas_set_color(canvas, ColorBlack);
    } else {
        canvas_set_font(canvas, FontPrimary);
        const char* word = m->contacts ? "CLEAR NOW" : "ALL CLEAR";
        canvas_draw_str_aligned(canvas, 4, STATUS_Y + 9, AlignLeft, AlignBottom, word);

        /* elapsed clock sits on the same band, right-aligned */
        canvas_set_font(canvas, FontBigNumbers);
        fmt_clock(buf, sizeof(buf), m->watching_ms);
        canvas_draw_str_aligned(canvas, 126, CLOCK_BASE, AlignRight, AlignBottom, buf);
    }

    /* ---------- footer: the tallies ---------- */
    canvas_draw_line(canvas, 0, FOOT1_BASE - 10, 127, FOOT1_BASE - 10);
    canvas_set_font(canvas, FontSecondary);

    snprintf(buf, sizeof(buf), "HITS %lu", (unsigned long)m->contacts);
    canvas_draw_str(canvas, 2, FOOT1_BASE, buf);
    snprintf(buf, sizeof(buf), "PEAK %u%%", (unsigned)m->peak);
    canvas_draw_str(canvas, COL_RIGHT, FOOT1_BASE, buf);

    char ago[16];
    fmt_ago(ago, sizeof(ago), m->watching_ms, m->last_ms);
    snprintf(buf, sizeof(buf), "LAST %s", ago);
    canvas_draw_str(canvas, 2, FOOT2_BASE, buf);

    if(m->present) {
        snprintf(buf, sizeof(buf), "NOW %u%%", (unsigned)m->strength);
        canvas_draw_str(canvas, COL_RIGHT, FOOT2_BASE, buf);
    } else if(m->contacts) {
        /* Nothing right now, but something was here: how long a carrier was
         * actually up across the whole watch. That is the figure you want when
         * you come back to a Flipper you left somewhere. */
        uint32_t s = m->in_field_ms / 1000u;
        if(s < 600u) {
            snprintf(buf, sizeof(buf), "SEEN %lus", (unsigned long)s);
        } else {
            snprintf(buf, sizeof(buf), "SEEN %lum", (unsigned long)(s / 60u));
        }
        canvas_draw_str(canvas, COL_RIGHT, FOOT2_BASE, buf);
    } else {
        canvas_draw_str(canvas, COL_RIGHT, FOOT2_BASE, "OK=reset");
    }
}

static bool watch_view_input(InputEvent* event, void* context) {
    WatchView* v = context;
    if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(v->reset_cb) v->reset_cb(v->reset_ctx);
        return true;
    }
    return false; // everything else (incl. BACK) bubbles to the scene manager
}

WatchView* watch_view_alloc(void) {
    WatchView* v = malloc(sizeof(WatchView));
    memset(v, 0, sizeof(WatchView));
    v->view = view_alloc();
    view_set_context(v->view, v);
    view_set_draw_callback(v->view, watch_view_draw);
    view_set_input_callback(v->view, watch_view_input);
    view_allocate_model(v->view, ViewModelTypeLocking, sizeof(WatchModel));
    return v;
}

void watch_view_free(WatchView* v) {
    furi_assert(v);
    view_free(v->view);
    free(v);
}

View* watch_view_get_view(WatchView* v) {
    furi_assert(v);
    return v->view;
}

void watch_view_set_reset_callback(WatchView* v, WatchViewCallback cb, void* ctx) {
    furi_assert(v);
    v->reset_cb = cb;
    v->reset_ctx = ctx;
}

void watch_view_update(
    WatchView* v,
    const FieldStats* stats,
    uint32_t watching_ms,
    uint32_t first_ms,
    uint32_t last_ms) {
    furi_assert(v);
    furi_assert(stats);
    with_view_model(
        v->view,
        WatchModel * m,
        {
            m->armed = stats->armed;
            m->error = stats->error;
            m->present = stats->present;
            m->strength = stats->strength;
            m->peak = stats->peak;
            m->contacts = stats->contacts;
            m->watching_ms = watching_ms;
            m->first_ms = first_ms;
            m->last_ms = last_ms;
            m->in_field_ms = stats->in_field_ms;
        },
        true);
}

void watch_view_tick(WatchView* v) {
    furi_assert(v);
    with_view_model(v->view, WatchModel * m, { m->anim++; }, true);
}
