#include "fingerprint_view.h"
#include <furi.h>
#include <gui/gui.h>
#include <stdio.h>
#include <string.h>

/* Layout. The screen is a fingerprint card: what it is, how sure we are, the
 * numbers behind that, and the raw carrier trace those numbers came from - so
 * the verdict is never asked to be believed on its own. */
#define ROW_CLASS_BASE 22 // FontPrimary baseline for the class name
#define ROW_BLURB_BASE 31
#define ROW_STAT1_BASE 40
#define ROW_STAT2_BASE 48
#define COL_RIGHT      66 // second column of the stat rows

#define CONF_X 88
#define CONF_Y 15
#define CONF_W 38
#define CONF_H 8

#define DIVIDER_Y 50
#define TRACE_HI  53 // carrier up
#define TRACE_LO  61 // carrier down

#define FLASH_TICKS 10 // ~1 s at the 100 ms UI tick

struct FingerprintView {
    View* view;
    FingerprintViewCallback save_cb;
    void* save_ctx;
    FingerprintViewCallback reset_cb;
    void* reset_ctx;
};

typedef struct {
    bool armed;
    bool error;
    bool present;
    CadenceStats cadence;
    EmitterVerdict verdict;
    uint8_t trace[SPECTER_TRACE_LEN];
    uint8_t trace_head;
    uint8_t anim;
    uint8_t flash; // ticks left to show flash_msg
    char flash_msg[12];
} FingerprintModel;

static void draw_error(Canvas* canvas) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignCenter, "NFC unavailable");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignCenter, "Close any other NFC app.");
}

/* The pulse train: one screen column per trace slice, drawn as a logic-analyser
 * waveform with real vertical edges. This is the raw carrier - not the smoothed
 * strength - so a polling reader shows up as an unmistakable square wave. */
static void draw_trace(Canvas* canvas, const FingerprintModel* m) {
    bool prev_hi = false;
    for(uint32_t i = 0; i < SPECTER_TRACE_LEN; i++) {
        /* oldest slice at the left edge, newest at the right */
        uint32_t age = SPECTER_TRACE_LEN - 1u - i;
        uint32_t idx = (m->trace_head + SPECTER_TRACE_LEN - age) % SPECTER_TRACE_LEN;
        bool hi = m->trace[idx] >= 2u; // majority of the slice's samples

        int y = hi ? TRACE_HI : TRACE_LO;
        canvas_draw_dot(canvas, (int)i, y);
        if(i > 0 && hi != prev_hi) canvas_draw_line(canvas, (int)i, TRACE_HI, (int)i, TRACE_LO);
        prev_hi = hi;
    }
}

static void draw_confidence(Canvas* canvas, uint8_t confidence) {
    canvas_draw_frame(canvas, CONF_X, CONF_Y, CONF_W, CONF_H);
    uint32_t fill = ((uint32_t)confidence * (CONF_W - 2u)) / 100u;
    if(fill) canvas_draw_box(canvas, CONF_X + 1, CONF_Y + 1, (int)fill, CONF_H - 2);
}

static void fingerprint_view_draw(Canvas* canvas, void* model) {
    FingerprintModel* m = model;
    char buf[24];

    /* ---------- header ---------- */
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 9, "FINGERPRINT");

    if(m->flash) {
        /* a confirmation takes over the right of the header for a beat */
        canvas_draw_box(canvas, 78, 0, 50, 11);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, 125, 9, AlignRight, AlignBottom, m->flash_msg);
        canvas_set_color(canvas, ColorBlack);
    } else {
        const char* state = m->error ? "NFC BUSY" : !m->armed ? "IDLE" : "LISTENING";
        canvas_draw_str_aligned(canvas, 116, 9, AlignRight, AlignBottom, state);
        if(m->present) {
            canvas_draw_disc(canvas, 123, 5, 2);
        } else {
            canvas_draw_circle(canvas, 123, 5, 2);
        }
    }
    canvas_draw_line(canvas, 0, 11, 127, 11);

    if(m->error) {
        draw_error(canvas);
        return;
    }

    /* ---------- the call ---------- */
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, ROW_CLASS_BASE, emitter_class_name(m->verdict.klass));
    draw_confidence(canvas, m->verdict.confidence);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, ROW_BLURB_BASE, emitter_class_blurb(m->verdict.klass));
    snprintf(buf, sizeof(buf), "%u%%", (unsigned)m->verdict.confidence);
    canvas_draw_str_aligned(canvas, 126, ROW_BLURB_BASE, AlignRight, AlignBottom, buf);

    /* ---------- the numbers behind it ---------- */
    const CadenceStats* c = &m->cadence;
    bool has_cadence = c->bursts > 0 && m->verdict.klass != EmitterClassNoField;

    /* A tilde is the whole honesty story in one character: we resolved the shape
     * but the durations are down at the sampler's own granularity. */
    const char* approx = m->verdict.timing_reliable ? "" : "~";

    if(has_cadence) {
        snprintf(buf, sizeof(buf), "PER %s%ums", approx, (unsigned)c->period_ms);
        canvas_draw_str(canvas, 2, ROW_STAT1_BASE, buf);
        snprintf(buf, sizeof(buf), "BST %s%ums", approx, (unsigned)c->burst_ms);
        canvas_draw_str(canvas, COL_RIGHT, ROW_STAT1_BASE, buf);

        snprintf(buf, sizeof(buf), "JIT %s%ums", approx, (unsigned)c->jitter_ms);
        canvas_draw_str(canvas, 2, ROW_STAT2_BASE, buf);
    } else {
        canvas_draw_str(canvas, 2, ROW_STAT1_BASE, "PER --");
        canvas_draw_str(canvas, COL_RIGHT, ROW_STAT1_BASE, "BST --");
        canvas_draw_str(canvas, 2, ROW_STAT2_BASE, "JIT --");
    }

    snprintf(buf, sizeof(buf), "DUTY %u%%", (unsigned)c->duty);
    canvas_draw_str(canvas, COL_RIGHT, ROW_STAT2_BASE, buf);

    /* ---------- raw carrier ---------- */
    canvas_draw_line(canvas, 0, DIVIDER_Y, 127, DIVIDER_Y);
    draw_trace(canvas, m);
}

static bool fingerprint_view_input(InputEvent* event, void* context) {
    FingerprintView* v = context;
    /* BACK (and anything not OK) bubbles up so the scene manager can exit. */
    if(event->key != InputKeyOk) return false;

    if(event->type == InputTypeShort) {
        if(v->save_cb) v->save_cb(v->save_ctx);
        return true;
    }
    if(event->type == InputTypeLong) {
        if(v->reset_cb) v->reset_cb(v->reset_ctx);
        return true;
    }
    /* Swallow the press/release halves so a long OK does not also fire short. */
    return event->type == InputTypePress || event->type == InputTypeRelease;
}

FingerprintView* fingerprint_view_alloc(void) {
    FingerprintView* v = malloc(sizeof(FingerprintView));
    memset(v, 0, sizeof(FingerprintView));
    v->view = view_alloc();
    view_set_context(v->view, v);
    view_set_draw_callback(v->view, fingerprint_view_draw);
    view_set_input_callback(v->view, fingerprint_view_input);
    view_allocate_model(v->view, ViewModelTypeLocking, sizeof(FingerprintModel));
    return v;
}

void fingerprint_view_free(FingerprintView* v) {
    furi_assert(v);
    view_free(v->view);
    free(v);
}

View* fingerprint_view_get_view(FingerprintView* v) {
    furi_assert(v);
    return v->view;
}

void fingerprint_view_set_save_callback(FingerprintView* v, FingerprintViewCallback cb, void* ctx) {
    furi_assert(v);
    v->save_cb = cb;
    v->save_ctx = ctx;
}

void fingerprint_view_set_reset_callback(FingerprintView* v, FingerprintViewCallback cb, void* ctx) {
    furi_assert(v);
    v->reset_cb = cb;
    v->reset_ctx = ctx;
}

void fingerprint_view_update(FingerprintView* v, const FieldStats* stats) {
    furi_assert(v);
    furi_assert(stats);
    with_view_model(
        v->view,
        FingerprintModel * m,
        {
            m->armed = stats->armed;
            m->error = stats->error;
            m->present = stats->present;
            m->cadence = stats->cadence;
            m->verdict = emitter_classify(&stats->cadence);
            memcpy(m->trace, stats->trace, sizeof(m->trace));
            m->trace_head = stats->trace_head;
        },
        true);
}

void fingerprint_view_flash(FingerprintView* v, const char* msg) {
    furi_assert(v);
    with_view_model(
        v->view,
        FingerprintModel * m,
        {
            strncpy(m->flash_msg, msg ? msg : "", sizeof(m->flash_msg) - 1);
            m->flash_msg[sizeof(m->flash_msg) - 1] = '\0';
            m->flash = FLASH_TICKS;
        },
        true);
}

void fingerprint_view_tick(FingerprintView* v) {
    furi_assert(v);
    with_view_model(
        v->view,
        FingerprintModel * m,
        {
            m->anim++;
            if(m->flash) m->flash--;
        },
        true);
}
