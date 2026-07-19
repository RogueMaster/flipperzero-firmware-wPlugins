#include "capture_view.h"
#include <gui/elements.h>
#include <furi.h>
#include <string.h>

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
    bool have;
    uint32_t phase;
} CaptureModel;

static void capture_view_draw(Canvas* canvas, void* model) {
    CaptureModel* m = model;
    canvas_clear(canvas);

    /* --- header --- */
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "RollCall");
    canvas_set_font(canvas, FontSecondary);
    char freq[20];
    snprintf(freq, sizeof(freq), "%s %s", m->band, m->mod);
    canvas_draw_str_aligned(canvas, 126, 9, AlignRight, AlignBottom, freq);
    canvas_draw_line(canvas, 0, 12, 128, 12);

    /* --- left: antenna emitting expanding rings ("listening") --- */
    const int cx = 22, cy = 31;
    for(int k = 0; k < 3; k++) {
        int r = 4 + (int)((m->phase * 2 + k * 8) % 15);
        canvas_draw_circle(canvas, cx, cy, r);
    }
    canvas_draw_line(canvas, cx, cy, cx, cy - 12); // mast
    canvas_draw_disc(canvas, cx, cy - 12, 2); // tip
    canvas_draw_line(canvas, cx - 4, cy + 4, cx + 4, cy + 4); // ground

    /* --- right: big captured / target counter --- */
    char big[6];
    snprintf(big, sizeof(big), "%d", m->count);
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str(canvas, 58, 36, big);
    int bw = canvas_string_width(canvas, big);
    char of[8];
    snprintf(of, sizeof(of), "/%d", m->target);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 58 + bw + 2, 36, of);
    canvas_draw_str(canvas, 58, 46, "presses");

    /* --- slot dots (fill as presses land) --- */
    int slots = m->target > 8 ? 8 : m->target;
    int total_w = slots * 8 - 3;
    int sx = 64 - total_w / 2;
    for(int i = 0; i < slots; i++) {
        int x = sx + i * 8;
        if(i < m->count)
            canvas_draw_disc(canvas, x, 52, 2);
        else
            canvas_draw_circle(canvas, x, 52, 2);
    }

    /* --- last decoded protocol --- */
    canvas_set_font(canvas, FontSecondary);
    if(m->have) {
        char line[30];
        snprintf(line, sizeof(line), "> %s", m->protocol);
        canvas_draw_str(canvas, 2, 46, line);
    }

    /* --- footer (inverted) --- */
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 57, 128, 7);
    canvas_set_color(canvas, ColorWhite);
    char dots[4] = {0};
    int nd = (m->phase / 3) % 4;
    for(int i = 0; i < nd; i++)
        dots[i] = '.';
    char hint[24];
    snprintf(hint, sizeof(hint), "Press remote%s", dots);
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
    RcCodeClass cls) {
    UNUSED(cls);
    furi_assert(v);
    with_view_model(
        v->view,
        CaptureModel * m,
        {
            m->count = count;
            if(protocol) {
                strncpy(m->protocol, protocol, sizeof(m->protocol) - 1);
                m->protocol[sizeof(m->protocol) - 1] = '\0';
                m->have = true;
            }
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
            m->protocol[0] = '\0';
            m->phase = 0;
        },
        true);
}

void capture_view_tick(CaptureView* v) {
    furi_assert(v);
    with_view_model(v->view, CaptureModel * m, { m->phase++; }, true);
}
