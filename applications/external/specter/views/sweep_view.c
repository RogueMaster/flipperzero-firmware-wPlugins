#include "sweep_view.h"
#include "view_chrome.h"
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

#define FLASH_TICKS 12 // ~1.2 s at the 100 ms UI tick

struct SweepView {
    View* view;
    SweepViewCallback ok_cb;
    void* ok_ctx;
    SweepViewCallback log_cb;
    void* log_ctx;
    SweepViewCallback left_cb;
    void* left_ctx;
    SweepViewStepCallback sens_cb;
    void* sens_ctx;
};

typedef struct {
    bool armed;
    bool error;
    bool present;
    uint8_t strength; // 0..100
    uint8_t peak; // 0..100
    uint8_t threshold_shown; // 0..100, where "reader" begins on this dial
    bool saturated; // meter pegged - closing in further will not move it
    uint32_t contacts;
    uint8_t history[SPECTER_HISTORY_LEN];
    uint8_t history_head;
    uint8_t anim;
    char sens[10];
    bool calibrating;
    uint8_t calib_progress; // 0..100
    uint8_t flash; // ticks left to show flash_msg
    char flash_msg[12];
} SweepModel;

/* value 0..100 -> point on the top semicircle (0% = left, 50% = up, 100% = right) */
static void gauge_point(uint8_t value, float radius, int* x, int* y) {
    if(value > 100) value = 100;
    float a = (float)M_PI * (1.0f - (float)value / 100.0f);
    *x = PCX + (int)(cosf(a) * radius);
    *y = PCY - (int)(sinf(a) * radius);
}

/* Drawn from explicit lines rather than a glyph so the shape is identical on
 * the device and in the generated mockups. */
static void draw_trend(Canvas* canvas, int x, int y, int dir) {
    if(dir == 0) {
        canvas_draw_line(canvas, x - 2, y + 2, x + 2, y + 2);
        return;
    }
    int tip = (dir > 0) ? y : y + 5;
    int tail = (dir > 0) ? y + 5 : y;
    int barb = (dir > 0) ? y + 3 : y + 2;
    canvas_draw_line(canvas, x, tail, x, tip);
    canvas_draw_line(canvas, x - 2, barb, x, tip);
    canvas_draw_line(canvas, x + 2, barb, x, tip);
}

static void sweep_view_draw(Canvas* canvas, void* model) {
    SweepModel* m = model;
    char buf[24];

    /* ---------- header ---------- */
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 9, "SWEEP");

    if(m->flash) {
        /* a confirmation takes over the right of the header for a beat */
        canvas_draw_box(canvas, 74, 0, 54, 11);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, 125, 9, AlignRight, AlignBottom, m->flash_msg);
        canvas_set_color(canvas, ColorBlack);
    } else {
        const char* state = m->calibrating ? "CALIBRATING" :
                                             specter_chrome_state(m->error, m->armed, m->present);
        canvas_draw_str_aligned(canvas, 116, 9, AlignRight, AlignBottom, state);
        if(m->present) {
            canvas_draw_disc(canvas, 123, 5, 2);
        } else {
            canvas_draw_circle(canvas, 123, 5, 2);
        }
    }
    specter_chrome_rule(canvas);

    if(m->error) {
        specter_chrome_nfc_error(canvas);
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
    /* Plain, evenly weighted ticks. The top three used to be drawn bolder as a
     * "danger zone", which was decoration pretending to be information: the app
     * does not call a reader at 80%, it calls one above the sensitivity
     * threshold, which on the default setting lands nearer a quarter of the
     * dial. The real line is drawn below, from the real number. */
    for(int i = 0; i <= 10; i++) {
        int ox, oy, ix, iy;
        gauge_point((uint8_t)(i * 10), R_OUT, &ox, &oy);
        gauge_point((uint8_t)(i * 10), R_IN, &ix, &iy);
        canvas_draw_line(canvas, ix, iy, ox, oy);
    }

    /* The threshold: everything past this mark is what the app will call a
     * reader, at the sensitivity you actually have set. It moves when you
     * change sensitivity or calibrate, so the dial finally answers "what counts
     * as a hit?" instead of guessing at it with bold tick marks. */
    {
        int mx, my, mox, moy;
        gauge_point(m->threshold_shown, R_IN, &mx, &my);
        gauge_point(m->threshold_shown, R_OUT + 2, &mox, &moy);
        canvas_draw_line(canvas, mx, my, mox, moy);
        canvas_draw_line(canvas, mx + 1, my, mox + 1, moy);
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
    canvas_draw_line(canvas, 64, 12, 64, 51);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 68, 20, "FIELD");

    /* Warmer/colder arrow, tucked beside the FIELD label. */
    if(m->armed && !m->calibrating) {
        draw_trend(canvas, 120, 14, field_trend(m->history, m->history_head, SPECTER_HISTORY_LEN));
    }

    /* The big number's glyphs are ~19px tall and hang down to their baseline, so
     * this sits high enough to clear the PK/C line underneath it - at baseline
     * 45 the two rows collided by a pixel and looked like one smudged block. */
    canvas_set_font(canvas, FontBigNumbers);
    snprintf(buf, sizeof(buf), "%u", (unsigned)m->strength);
    canvas_draw_str_aligned(canvas, 112, 42, AlignRight, AlignBottom, buf);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 114, 40, "%");

    /* This used to read "PK18 C7" - the peak and the contact count crammed
     * together. Two problems: C was the only abbreviation in the app defined
     * nowhere, and at "PK100 C999+" the row ran off the right edge of a 128px
     * screen. The tally belongs on Survey and Watch, where a count is the whole
     * point; the hunting screen wants the one number the needle is chasing, and
     * it finally names the peak-hold marker already drawn on the arc. */
    snprintf(buf, sizeof(buf), "PEAK %u%%", (unsigned)m->peak);
    canvas_draw_str(canvas, 68, 51, buf);

    /* ---------- bottom strip ---------- */
    canvas_draw_line(canvas, 0, 52, 127, 52);
    if(m->calibrating) {
        /* Learning the room's own noise floor, right where you are standing. */
        /* FontSecondary occupies rows [baseline-7 .. baseline], so a baseline of
         * 59 put the glyph tops on row 52 - straight through the divider above.
         * 60 clears it, and the progress bar drops to a plain 2px fill hugging
         * the bottom edge rather than a framed box that would then clip the
         * text from below. */
        /* The header now carries what is happening (CALIBRATING); this carries
         * what the user must actually do about it. */
        canvas_draw_str(canvas, 2, 62, "HOLD STILL");
        canvas_draw_str_aligned(canvas, 126, 62, AlignRight, AlignBottom, "OK=cancel");
        uint32_t fill = ((uint32_t)m->calib_progress * 128u) / 100u;
        if(fill) canvas_draw_box(canvas, 0, 63, (int)fill, 1);
    } else if(m->present) {
        canvas_draw_box(canvas, 0, 53, 128, 11);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_disc(canvas, 4, 58, 1);
        canvas_draw_str(canvas, 9, 62, "ACTIVE READER");
        canvas_draw_str_aligned(
            canvas,
            125,
            62,
            AlignRight,
            AlignBottom,
            field_proximity_word(m->strength, m->saturated));
        canvas_set_color(canvas, ColorBlack);
        /* One 1px border, matching Site Survey's alarm. There used to be a
         * second frame inset at (1,1,126,62); its bottom edge ran along row 62
         * and was the only reason this strip sat a pixel higher than the other
         * two states, so the text visibly hopped as you found a reader. */
        canvas_draw_frame(canvas, 0, 0, 128, 64);
    } else if(!m->contacts) {
        /* Nothing has been FOUND yet, so the waveform below is a flat row of
         * dots carrying no information. Spend the strip instead on the two
         * bindings that cannot be discovered by pressing things: LEFT is the
         * README's own first step, and hold-OK is how a finding gets saved.
         *
         * Gated on contacts, not peak: peak climbs off ordinary room noise
         * within a second or two of arming (field_detector.c line 228 raises it
         * from every sample, not just from detections), so a peak-based test
         * would blink this hint away before anyone had read it. Contacts only
         * moves when a real carrier was detected - so the training wheels come
         * off exactly when the user has demonstrably found something. */
        canvas_draw_str(canvas, 2, 62, "LEFT=cal hold OK=log");
    } else {
        /* Idle: the active sensitivity on the left, a live waveform of recent
         * field strength filling whatever space is left to the right of it. */
        char sbuf[16];
        snprintf(sbuf, sizeof(sbuf), "SENS %s", m->sens[0] ? m->sens : "?");
        canvas_draw_str(canvas, 2, 62, sbuf);
        int wave_left = 2 + (int)canvas_string_width(canvas, sbuf) + 4;

        for(int k = 0; k < 62; k++) {
            int x = 126 - k * 2;
            if(x < wave_left) break;
            int idx = (m->history_head - k + 2 * SPECTER_HISTORY_LEN) % SPECTER_HISTORY_LEN;
            int v = m->history[idx];
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

    /* BACK is never ours - let it reach the scene manager so it always exits,
     * even mid OK-long-press. Explicit so the swallow logic below can't grow to
     * cover it by accident. */
    if(event->key == InputKeyBack) return false;

    if(event->key == InputKeyOk) {
        if(event->type == InputTypeShort) {
            if(v->ok_cb) v->ok_cb(v->ok_ctx);
            return true;
        }
        if(event->type == InputTypeLong) {
            if(v->log_cb) v->log_cb(v->log_ctx);
            return true;
        }
        /* Swallow press/release so a long OK does not also fire the short one. */
        return event->type == InputTypePress || event->type == InputTypeRelease;
    }

    if(event->key == InputKeyLeft && event->type == InputTypeShort) {
        if(v->left_cb) v->left_cb(v->left_ctx);
        return true;
    }

    /* UP / DOWN step sensitivity without leaving the hunt. A noisy forecourt
     * wants Low and a quiet corridor wants High, and reaching that through the
     * menu meant taking the Flipper off the target for ten keypresses. */
    if(event->type == InputTypeShort && (event->key == InputKeyUp || event->key == InputKeyDown)) {
        if(v->sens_cb) v->sens_cb(v->sens_ctx, event->key == InputKeyUp);
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

void sweep_view_set_log_callback(SweepView* v, SweepViewCallback cb, void* context) {
    furi_assert(v);
    v->log_cb = cb;
    v->log_ctx = context;
}

void sweep_view_set_left_callback(SweepView* v, SweepViewCallback cb, void* context) {
    furi_assert(v);
    v->left_cb = cb;
    v->left_ctx = context;
}

void sweep_view_reset(SweepView* v) {
    furi_assert(v);
    with_view_model(
        v->view,
        SweepModel * m,
        {
            /* Sensitivity is configuration, not a reading - it survives. */
            char sens[sizeof(m->sens)];
            memcpy(sens, m->sens, sizeof(sens));
            memset(m, 0, sizeof(SweepModel));
            memcpy(m->sens, sens, sizeof(sens));
        },
        true);
}

void sweep_view_set_sens_callback(SweepView* v, SweepViewStepCallback cb, void* context) {
    furi_assert(v);
    v->sens_cb = cb;
    v->sens_ctx = context;
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
            m->saturated = stats->saturated;
            m->threshold_shown = stats->threshold_shown;
            m->contacts = stats->contacts;
            memcpy(m->history, stats->history, sizeof(m->history));
            m->history_head = stats->history_head;
            m->calibrating = stats->calibrating;
            m->calib_progress = stats->calibration_progress;
            if(sens_label) {
                strncpy(m->sens, sens_label, sizeof(m->sens) - 1);
                m->sens[sizeof(m->sens) - 1] = '\0';
            }
        },
        true);
}

void sweep_view_flash(SweepView* v, const char* msg) {
    furi_assert(v);
    with_view_model(
        v->view,
        SweepModel * m,
        {
            strncpy(m->flash_msg, msg ? msg : "", sizeof(m->flash_msg) - 1);
            m->flash_msg[sizeof(m->flash_msg) - 1] = '\0';
            m->flash = FLASH_TICKS;
        },
        true);
}

void sweep_view_tick(SweepView* v) {
    furi_assert(v);
    with_view_model(
        v->view,
        SweepModel * m,
        {
            m->anim++;
            if(m->flash) m->flash--;
        },
        true);
}
