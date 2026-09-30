#include "splash_view.h"
#include "../helpers/field_scale.h"
#include <furi.h>
#include <gui/gui.h>
#include <string.h>

/* Boot intro: a carrier arrives.
 *
 * The whole product in a little over two seconds, in three beats.
 *
 *   LISTENING  A trace writes itself across the screen from the left, flat and
 *              silent. That is what a clean room looks like on this instrument.
 *   CONTACT    A reader's poll cuts in, the line starts square-waving, and the
 *              screen inverts for a fifth of a second. The inversion is not
 *              decoration: it is the same gesture the Sweep screen makes when
 *              it locks on, so the intro and the instrument share a vocabulary.
 *   NAMEPLATE  SPECTER engraves itself a letter at a time, a rule grows out
 *              from the centre under it, and the tagline lands. Meanwhile the
 *              captured band keeps drifting left, so the screen is never still.
 *
 * It is the instrument introducing itself rather than a logo bumper, and the
 * waveform is honest about where it comes from. The banner renderer is
 * forbidden from inventing carrier and has to read a real capture back off
 * disk; a FAP cannot carry a capture, so the shape here is generated - but
 * from the app's OWN constant, SPECTER_FULL_SCALE_DUTY, the duty cycle the
 * entire meter is scaled against. The intro draws the number the product is
 * built on, not a pleasing-looking squiggle.
 *
 * Any key skips it, and Settings has a switch to turn it off for good: an intro
 * you cannot get past is one people learn to resent.
 */

#define SPLASH_DONE_TICKS  22u
#define SPLASH_WRITE_TICKS 9u // ticks the write head takes to cross the screen

/* The beat where the poll is found: the carrier band inverts for two ticks.
 *
 * It used to invert the WHOLE SCREEN, and that was wrong - not subtly wrong,
 * reported-as-a-bug wrong. A 128x64 panel going entirely black for a fifth of
 * a second during startup does not read as emphasis, it reads as the display
 * glitching or the app crashing, which is a terrible thing for an intro to
 * imply about a tool you are about to trust. Inverting only the band the
 * carrier lives in says the same thing - the instrument just registered
 * something - and unmistakably belongs to the drawing rather than to the
 * hardware. */
#define SPLASH_CONTACT_TICK 4u
#define SPLASH_CONTACT_LEN  2u

/* The inverted band. Sits clear of everything drawn at that moment: the rule
 * on row 30 has rows 31-33 blank below it, and nothing else exists yet (the
 * nameplate starts at tick 6, the tagline at 13). */
#define SPLASH_BAND_TOP 34
#define SPLASH_BAND_H   19

#define SPLASH_NAME_TICK  6u // nameplate starts engraving
#define SPLASH_RULE_TICK  9u // rule starts growing
#define SPLASH_DRIFT_TICK 10u // the captured band starts sliding left
#define SPLASH_TAG_TICK   13u // tagline lands

/* Rows. Checked against the house clearance rule - FontPrimary inks
 * [baseline-8 .. baseline-1] and FontSecondary [baseline-7 .. baseline-1], with
 * the baseline row itself blank:
 *   nameplate ink 18..25, rule on 30          -> 4 blank rows
 *   rule on 30, trace HI on 40                -> 9 blank rows
 *   trace LO on 50, tagline ink 55..61        -> 4 blank rows
 *   tagline ink ends 61, screen edge on 63    -> 2 blank rows          */
#define SPLASH_NAME_BASE 26
#define SPLASH_RULE_Y    30
#define SPLASH_TRACE_HI  40
#define SPLASH_TRACE_LO  50
#define SPLASH_TAG_BASE  62

/* Where the silence ends and the reader's poll begins, in screen columns.
 * Chosen so the head reaches it at SPLASH_CONTACT_TICK: the flash and the
 * first square edge are the same event, not two events that nearly coincide. */
#define SPLASH_QUIET_COLS 52

/* One poll cycle, in columns. The high run is SPECTER_FULL_SCALE_DUTY percent
 * of it - the same duty a real terminal radiates, and the value field_scale.c
 * maps onto a full meter. */
#define SPLASH_PERIOD_COLS 10

/* The nameplate, set spaced the way the banner engraves it. Revealed two
 * characters per tick from a FIXED left edge, so the letters land in place
 * instead of sliding as a centred string would. */
static const char SPLASH_NAME[] = "S P E C T E R";
#define SPLASH_NAME_LEN   (sizeof(SPLASH_NAME) - 1u)
#define SPLASH_NAME_SPEED 2u

struct SplashView {
    View* view;
    SplashViewCallback done_cb;
    void* done_ctx;
};

typedef struct {
    uint8_t anim; // frames since the scene entered
} SplashModel;

/* True when the carrier is up in this column. Left of SPLASH_QUIET_COLS the
 * room is quiet, so the line simply sits at LO. */
static bool splash_carrier_hi(int col) {
    if(col < SPLASH_QUIET_COLS) return false;
    int phase = (col - SPLASH_QUIET_COLS) % SPLASH_PERIOD_COLS;
    int high = (SPLASH_PERIOD_COLS * SPECTER_FULL_SCALE_DUTY) / 100;
    if(high < 1) high = 1;
    return phase < high;
}

/* The same logic-analyser drawing the Fingerprint screen uses: a dot per column
 * at the carrier's level, and a full vertical wherever the level changes.
 * `drift` slides the captured band leftwards once the write is done, which is
 * what keeps the finished composition alive rather than frozen. */
static void splash_draw_trace(Canvas* canvas, int written, int drift) {
    bool prev_hi = false;
    for(int x = 0; x < written && x < 128; x++) {
        bool hi = splash_carrier_hi(x + drift);
        canvas_draw_dot(canvas, x, hi ? SPLASH_TRACE_HI : SPLASH_TRACE_LO);
        if(x > 0 && hi != prev_hi) {
            canvas_draw_line(canvas, x, SPLASH_TRACE_HI, x, SPLASH_TRACE_LO);
        }
        prev_hi = hi;
    }
    /* The write head: a short tick at the leading edge while it is still
     * travelling, so the line reads as being drawn rather than revealed. */
    if(written > 0 && written < 128) {
        canvas_draw_line(canvas, written, SPLASH_TRACE_HI - 2, written, SPLASH_TRACE_LO + 2);
    }
}

static void splash_draw_content(Canvas* canvas, uint8_t a, bool flash) {
    /* (a + 1) so the very first frame already has a stub of line on it. At a*k
     * the intro opens on a completely blank screen for one tick, which on
     * hardware reads as the app having failed to start. */
    int written = (int)((((uint32_t)a + 1u) * 128u) / (SPLASH_WRITE_TICKS + 1u));
    if(written > 128) written = 128;

    int drift = 0;
    if(a >= SPLASH_DRIFT_TICK) drift = (int)((a - SPLASH_DRIFT_TICK) * 3u);

    /* There is no global invert on this canvas: filling the band and drawing
     * the trace white is the only way to do it, so the two have to be paired
     * here rather than wrapped around the whole draw. */
    if(flash) {
        canvas_draw_box(canvas, 0, SPLASH_BAND_TOP, 128, SPLASH_BAND_H);
        canvas_set_color(canvas, ColorWhite);
    }
    splash_draw_trace(canvas, written, drift);
    if(flash) canvas_set_color(canvas, ColorBlack);

    /* The presence indicator, in the same place and the same shapes the four
     * measurement screens use: hollow while nothing is there, filled once the
     * poll has been found. */
    bool found = a >= SPLASH_CONTACT_TICK;
    if(found) {
        canvas_draw_disc(canvas, 123, 5, 2);
    } else {
        canvas_draw_circle(canvas, 123, 5, 2);
    }

    if(a >= SPLASH_NAME_TICK) {
        canvas_set_font(canvas, FontPrimary);
        uint32_t shown = (a - SPLASH_NAME_TICK + 1u) * SPLASH_NAME_SPEED;
        if(shown > SPLASH_NAME_LEN) shown = SPLASH_NAME_LEN;

        char buf[SPLASH_NAME_LEN + 1u];
        memcpy(buf, SPLASH_NAME, shown);
        buf[shown] = '\0';

        /* Measured from the COMPLETE string, so every letter is engraved at the
         * x it will finally occupy. Centring each partial string instead makes
         * the whole nameplate crawl sideways as it fills. */
        int x0 = 64 - (int)(canvas_string_width(canvas, SPLASH_NAME) / 2u);
        canvas_draw_str(canvas, x0, SPLASH_NAME_BASE, buf);
    }

    if(a >= SPLASH_RULE_TICK) {
        int half = (int)((a - SPLASH_RULE_TICK + 1u) * 13u);
        if(half > 62) half = 62;
        canvas_draw_line(canvas, 64 - half, SPLASH_RULE_Y, 64 + half, SPLASH_RULE_Y);
    }

    if(a >= SPLASH_TAG_TICK) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas, 64, SPLASH_TAG_BASE, AlignCenter, AlignBottom, "NFC READER SWEEP");
    }
}

static void splash_view_draw(Canvas* canvas, void* model) {
    SplashModel* m = model;
    uint8_t a = m->anim;
    bool flash = a >= SPLASH_CONTACT_TICK && a < SPLASH_CONTACT_TICK + SPLASH_CONTACT_LEN;
    splash_draw_content(canvas, a, flash);
}

static bool splash_view_input(InputEvent* event, void* context) {
    SplashView* v = context;
    /* EVERY key skips to the menu, BACK included.
     *
     * BACK is the obvious thing to press to get past a splash, and this is the
     * root scene - so letting it bubble would hand it to the scene manager,
     * which would quit the app. Someone impatient with the intro would find
     * that trying to skip it closed Specter instead. Swallowing BACK for the
     * two seconds the intro is on screen costs nothing: it is the same
     * keypress either way, and it lands where the user was going. */
    if(event->type == InputTypeShort || event->type == InputTypeLong) {
        if(v->done_cb) v->done_cb(v->done_ctx);
        return true;
    }
    return false;
}

SplashView* splash_view_alloc(void) {
    SplashView* v = malloc(sizeof(SplashView));
    memset(v, 0, sizeof(SplashView));
    v->view = view_alloc();
    view_set_context(v->view, v);
    view_set_draw_callback(v->view, splash_view_draw);
    view_set_input_callback(v->view, splash_view_input);
    view_allocate_model(v->view, ViewModelTypeLocking, sizeof(SplashModel));
    return v;
}

void splash_view_free(SplashView* v) {
    furi_assert(v);
    view_free(v->view);
    free(v);
}

View* splash_view_get_view(SplashView* v) {
    furi_assert(v);
    return v->view;
}

void splash_view_set_done_callback(SplashView* v, SplashViewCallback cb, void* context) {
    furi_assert(v);
    v->done_cb = cb;
    v->done_ctx = context;
}

void splash_view_reset(SplashView* v) {
    furi_assert(v);
    with_view_model(v->view, SplashModel * m, { m->anim = 0; }, true);
}

bool splash_view_tick(SplashView* v) {
    furi_assert(v);
    bool done = false;
    with_view_model(
        v->view,
        SplashModel * m,
        {
            if(m->anim < 255) m->anim++;
            done = m->anim >= SPLASH_DONE_TICKS;
        },
        true);
    return done;
}
