#include "watch_view.h"
#include "view_chrome.h"
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
/* 60, not 61. This footer and the Site Survey verdict footer are the same
 * component - divider on row 40, two stat rows, columns at x=2 and x=66 - and
 * they sat one pixel apart because they were written a week apart. */
#define FOOT2_BASE 60
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
    uint32_t last_ms;
    uint32_t in_field_ms; // total time a carrier was actually up this watch
} WatchModel;

/* mm:ss while that fits, then hh:mm - flagged, because Watch is the one mode
 * built to be left running for hours and a clock that stops at 99:59 while the
 * LAST timer keeps counting is a screen disagreeing with itself. */
static void fmt_clock(char* out, size_t n, uint32_t ms, bool* hours) {
    uint32_t s = ms / 1000u;
    if(s <= 99u * 60u + 59u) {
        *hours = false;
        snprintf(out, n, "%02lu:%02lu", (unsigned long)(s / 60u), (unsigned long)(s % 60u));
        return;
    }
    *hours = true;
    uint32_t h = s / 3600u;
    if(h > 99u) { // ~4 days; past here it is a stuck Flipper, not a watch
        snprintf(out, n, "99:59");
        return;
    }
    snprintf(out, n, "%02lu:%02lu", (unsigned long)h, (unsigned long)((s / 60u) % 60u));
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
    specter_chrome_header(
        canvas, "WATCH", specter_chrome_state(m->error, m->armed, m->present), m->present);

    if(m->error) {
        specter_chrome_nfc_error(canvas);
        return;
    }

    /* ---------- status band ---------- */
    bool alarm = m->present;
    if(alarm) {
        /* Nothing on this band animates. It began as a full-width invert that
         * alternated every tick - a 5 Hz strobe - and was then softened to a
         * pair of markers pulsing at 1 Hz, which still read as flashing to
         * anyone actually watching the screen.
         *
         * An alarm does not need to move to be noticed: a solid inverted block
         * against an otherwise light screen is already the loudest thing on it.
         * Liveness is carried by the readouts that genuinely change - NOW %,
         * the hit count, the LAST timer - not by blinking the alarm itself. */
        canvas_draw_box(canvas, 0, STATUS_Y - 1, 128, STATUS_H);
        canvas_set_color(canvas, ColorWhite);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(
            canvas, 64, STATUS_Y + 9, AlignCenter, AlignBottom, "ACTIVE READER");
        canvas_set_color(canvas, ColorBlack);

        /* A strength bar under the banner. Across a room the banner answers
         * "is something there"; this answers "is it on top of the Flipper or
         * at the edge of range", which is the next thing you want to know and
         * used to be readable only by walking over and squinting at NOW %. */
        canvas_draw_frame(canvas, 4, 29, 120, 9);
        int w = ((int)m->strength * 118) / 100;
        if(w > 0) canvas_draw_box(canvas, 5, 30, w, 7);

        /* Same 1px alarm border as Sweep and Site Survey. */
        canvas_draw_frame(canvas, 0, 0, 128, 64);
    } else {
        canvas_set_font(canvas, FontPrimary);
        const char* word = m->contacts ? "QUIET NOW" : "NO READER";
        canvas_draw_str_aligned(canvas, 4, STATUS_Y + 9, AlignLeft, AlignBottom, word);

        /* elapsed clock sits on the same band, right-aligned */
        bool hours = false;
        canvas_set_font(canvas, FontBigNumbers);
        fmt_clock(buf, sizeof(buf), m->watching_ms, &hours);
        canvas_draw_str_aligned(
            canvas, hours ? 120 : 126, CLOCK_BASE, AlignRight, AlignBottom, buf);
        if(hours) {
            /* "02:37h" - the marker is the whole difference between two hours
             * thirty-seven and two minutes thirty-seven. */
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 121, CLOCK_BASE, "h");
        }

        /* The reset key, visible whenever there is something to lose. It used
         * to appear only in the bottom-right slot when the count was zero -
         * i.e. advertised only while harmless, hidden once a short OK would
         * silently wipe an overnight record. */
        if(m->contacts) {
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 4, 33, "OK=re-arm");
        }
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
            snprintf(buf, sizeof(buf), "UP %lus", (unsigned long)s);
        } else {
            snprintf(buf, sizeof(buf), "UP %lum", (unsigned long)(s / 60u));
        }
        canvas_draw_str(canvas, COL_RIGHT, FOOT2_BASE, buf);
    } else {
        canvas_draw_str(canvas, COL_RIGHT, FOOT2_BASE, "OK=re-arm");
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

void watch_view_reset(WatchView* v) {
    furi_assert(v);
    with_view_model(
        v->view,
        WatchModel * m,
        {
            memset(m, 0, sizeof(WatchModel));
            m->last_ms = WATCH_NO_TIME;
        },
        true);
}

void watch_view_update(
    WatchView* v,
    const FieldStats* stats,
    uint32_t watching_ms,
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
            m->last_ms = last_ms;
            m->in_field_ms = stats->in_field_ms;
        },
        true);
}
