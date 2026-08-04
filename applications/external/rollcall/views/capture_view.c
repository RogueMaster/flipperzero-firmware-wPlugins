#include "capture_view.h"
#include <gui/elements.h>
#include <furi.h>
#include <string.h>

/* Carrier strength maps onto the bar between these two. -100 dBm is about the
 * CC1101's noise floor; -40 dBm is a fob held against the Flipper. */
#define CV_RSSI_MIN_DBM (-100)
#define CV_RSSI_MAX_DBM (-40)

/* Pixels available to the protocol name before the slot dots begin. */
#define CV_PROTO_MAX_W 84

struct CaptureView {
    View* view;
    CaptureViewCallback cb;
    void* ctx;
};

typedef struct {
    char band[12];
    char mod[8];
    uint8_t target;
    uint8_t count;
    char protocol[28];
    uint16_t bits;
    bool have;
    int8_t rssi;
    bool active; // raw pulses flowing right now
    uint32_t phase;
} CaptureModel;

static uint8_t capture_bar_fill(int8_t rssi, uint8_t width) {
    if(rssi <= CV_RSSI_MIN_DBM) return 0;
    if(rssi >= CV_RSSI_MAX_DBM) return width;
    int32_t span = CV_RSSI_MAX_DBM - CV_RSSI_MIN_DBM;
    return (uint8_t)(((int32_t)rssi - CV_RSSI_MIN_DBM) * (int32_t)width / span);
}

static void capture_view_draw(Canvas* canvas, void* model) {
    CaptureModel* m = model;
    canvas_clear(canvas);

    /* --- header --- */
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "RollCall");
    canvas_set_font(canvas, FontSecondary);
    char freq[20];
    snprintf(freq, sizeof(freq), "%s %s", m->band, m->mod);
    canvas_draw_str_aligned(canvas, 126, 10, AlignRight, AlignBottom, freq);
    canvas_draw_line(canvas, 0, 12, 128, 12);

    /* --- left: antenna emitting expanding rings ("listening") ---
     * The outermost ring has to stop above the protocol line at y=46, so the
     * radius is capped at 12 around cy=26 rather than left to grow into it. */
    const int cx = 20, cy = 25;
    for(int k = 0; k < 3; k++) {
        int r = 4 + (int)((m->phase * 2 + k * 8) % 8);
        canvas_draw_circle(canvas, cx, cy, r);
    }
    canvas_draw_line(canvas, cx, cy, cx, cy - 9); // mast
    canvas_draw_disc(canvas, cx, cy - 9, 2); // tip
    canvas_draw_line(canvas, cx - 4, cy + 4, cx + 4, cy + 4); // ground

    /* --- right: big captured / target counter --- */
    char big[6];
    snprintf(big, sizeof(big), "%d", m->count);
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str(canvas, 56, 38, big);
    int bw = canvas_string_width(canvas, big);
    char of[8];
    snprintf(of, sizeof(of), "/%d", m->target);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 56 + bw + 2, 38, of);

    /* --- last decoded protocol (left) --- */
    if(m->have) {
        /* Sized for the worst case the compiler can see: a full protocol name
         * plus a five-digit bit count. Then trimmed to the width actually
         * available, so a long name cannot run into the slot dots. */
        char line[40];
        if(m->bits > 0) {
            snprintf(line, sizeof(line), "%s %ubit", m->protocol, m->bits);
        } else {
            snprintf(line, sizeof(line), "%s", m->protocol);
        }
        size_t len = strlen(line);
        while(len > 1 && canvas_string_width(canvas, line) > CV_PROTO_MAX_W) {
            line[--len] = '\0';
        }
        canvas_draw_str(canvas, 2, 46, line);
    }

    /* --- slot dots (right, fill as presses land) --- */
    int slots = m->target > 8 ? 8 : m->target;
    if(slots > 0) {
        int pitch = slots > 6 ? 5 : 6;
        int total_w = slots * pitch - (pitch - 4);
        int sx = 126 - total_w;
        for(int i = 0; i < slots; i++) {
            int x = sx + i * pitch;
            if(i < m->count)
                canvas_draw_disc(canvas, x, 43, 2);
            else
                canvas_draw_circle(canvas, x, 43, 2);
        }
    }

    /* --- signal row: activity pip + carrier bar + dBm --- */
    if(m->active)
        canvas_draw_box(canvas, 2, 48, 6, 6);
    else
        canvas_draw_frame(canvas, 2, 48, 6, 6);

    canvas_draw_frame(canvas, 11, 48, 69, 6);
    uint8_t fill = capture_bar_fill(m->rssi, 67);
    if(fill > 0) canvas_draw_box(canvas, 12, 49, fill, 4);

    char dbm[12];
    if(m->rssi <= -127) {
        snprintf(dbm, sizeof(dbm), "--dBm");
    } else {
        snprintf(dbm, sizeof(dbm), "%ddBm", (int)m->rssi);
    }
    canvas_draw_str_aligned(canvas, 126, 54, AlignRight, AlignBottom, dbm);

    /* --- footer (inverted) --- */
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 56, 128, 8);
    canvas_set_color(canvas, ColorWhite);

    char hint[26];
    if(m->count == 0 && m->active) {
        /* Right band, wrong protocol or modulation - say so instead of just
         * sitting at zero and letting the user think the app is broken. */
        snprintf(hint, sizeof(hint), "RF seen, no decode");
    } else {
        char dots[4] = {0};
        int nd = (int)((m->phase / 3) % 4);
        for(int i = 0; i < nd; i++) dots[i] = '.';
        snprintf(hint, sizeof(hint), "Press remote%s", dots);
    }
    canvas_draw_str(canvas, 3, 63, hint);
    canvas_draw_str_aligned(canvas, 125, 63, AlignRight, AlignBottom, "OK: Analyze");
    canvas_set_color(canvas, ColorBlack);
}

static bool capture_view_input(InputEvent* event, void* context) {
    CaptureView* v = context;
    if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(v->cb) v->cb(v->ctx, CaptureEventFinish);
        return true;
    }
    return false; // Back propagates to the scene manager
}

CaptureView* capture_view_alloc(void) {
    CaptureView* v = malloc(sizeof(CaptureView));
    v->cb = NULL;
    v->ctx = NULL;
    v->view = view_alloc();
    view_set_context(v->view, v);
    view_allocate_model(v->view, ViewModelTypeLocking, sizeof(CaptureModel));
    view_set_draw_callback(v->view, capture_view_draw);
    view_set_input_callback(v->view, capture_view_input);
    return v;
}

void capture_view_free(CaptureView* v) {
    furi_assert(v);
    view_free(v->view);
    free(v);
}

View* capture_view_get_view(CaptureView* v) {
    furi_assert(v);
    return v->view;
}

void capture_view_set_callback(CaptureView* v, CaptureViewCallback cb, void* context) {
    furi_assert(v);
    v->cb = cb;
    v->ctx = context;
}

void capture_view_set_config(CaptureView* v, const char* band, const char* mod, uint8_t target) {
    furi_assert(v);
    with_view_model(
        v->view,
        CaptureModel * m,
        {
            strncpy(m->band, band, sizeof(m->band) - 1);
            m->band[sizeof(m->band) - 1] = '\0';
            strncpy(m->mod, mod, sizeof(m->mod) - 1);
            m->mod[sizeof(m->mod) - 1] = '\0';
            m->target = target;
        },
        true);
}

void capture_view_set_progress(
    CaptureView* v,
    uint8_t count,
    const char* protocol,
    RcCodeClass cls,
    uint16_t bits) {
    UNUSED(cls);
    furi_assert(v);
    with_view_model(
        v->view,
        CaptureModel * m,
        {
            m->count = count;
            m->bits = bits;
            if(protocol) {
                strncpy(m->protocol, protocol, sizeof(m->protocol) - 1);
                m->protocol[sizeof(m->protocol) - 1] = '\0';
                m->have = true;
            }
        },
        true);
}

void capture_view_set_signal(CaptureView* v, int8_t rssi_dbm, bool active) {
    furi_assert(v);
    with_view_model(
        v->view,
        CaptureModel * m,
        {
            m->rssi = rssi_dbm;
            m->active = active;
        },
        true);
}

void capture_view_reset(CaptureView* v) {
    furi_assert(v);
    with_view_model(
        v->view,
        CaptureModel * m,
        {
            m->count = 0;
            m->have = false;
            m->bits = 0;
            m->protocol[0] = '\0';
            m->rssi = -127;
            m->active = false;
            m->phase = 0;
        },
        true);
}

void capture_view_tick(CaptureView* v) {
    furi_assert(v);
    with_view_model(v->view, CaptureModel * m, { m->phase++; }, true);
}
