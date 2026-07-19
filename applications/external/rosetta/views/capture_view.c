#include "capture_view.h"
#include <gui/elements.h>
#include <furi.h>
#include <string.h>
#include <stdio.h>

struct CaptureView {
    View* view;
    CaptureRescanCb rescan_cb;
    void* rescan_ctx;
};

typedef struct {
    char title[24];
    bool have; // result ready?
    uint32_t anim;
    CaptureAnnot annot;
} CaptureModel;

/* ------------------------------------------------------------------ draw */

static void draw_waiting(Canvas* canvas, CaptureModel* m) {
    const int cx = 96;
    const int cy = 34;

    /* expanding field rings, like a reader energising a tag */
    for(int k = 0; k < 3; k++) {
        int r = 5 + (int)((m->anim / 2 + k * 8) % 24);
        canvas_draw_circle(canvas, cx, cy, r);
    }
    canvas_draw_disc(canvas, cx, cy, 3);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 4, 12, m->title);

    canvas_set_font(canvas, FontSecondary);
    char dots[4] = {0};
    int nd = (m->anim / 12) % 4;
    for(int i = 0; i < nd; i++)
        dots[i] = '.';
    char buf[20];
    snprintf(buf, sizeof(buf), "listening%s", dots);
    canvas_draw_str(canvas, 4, 30, buf);

    elements_multiline_text_aligned(
        canvas, 4, 44, AlignLeft, AlignTop, "Hold the tag to\nthe Flipper");
}

static void draw_result(Canvas* canvas, CaptureModel* m) {
    /* header bar */
    canvas_draw_box(canvas, 0, 0, 128, 13);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 3, 10, "Captured");
    canvas_set_color(canvas, ColorBlack);

    /* annotated field lines */
    canvas_set_font(canvas, FontSecondary);
    int y = 24;
    for(uint8_t i = 0; i < m->annot.nline && i < CAPTURE_MAX_LINES; i++) {
        canvas_draw_str(canvas, 4, y, m->annot.lines[i]);
        y += 10;
    }

    /* verdict banner */
    int by = 52;
    if(m->annot.verdict_kind == CaptureVerdictGood) {
        canvas_draw_rframe(canvas, 0, by, 128, 12, 2);
        canvas_draw_str(canvas, 4, by + 9, m->annot.verdict);
    } else if(m->annot.verdict_kind == CaptureVerdictBad) {
        canvas_draw_rbox(canvas, 0, by, 128, 12, 2);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, 4, by + 9, m->annot.verdict);
        canvas_set_color(canvas, ColorBlack);
    } else {
        canvas_draw_str(canvas, 4, by + 9, m->annot.verdict);
    }
}

static void capture_view_draw(Canvas* canvas, void* model) {
    CaptureModel* m = model;
    canvas_clear(canvas);
    if(m->have) {
        draw_result(canvas, m);
    } else {
        draw_waiting(canvas, m);
    }
}

/* ----------------------------------------------------------------- input */

static bool capture_view_input(InputEvent* event, void* context) {
    CaptureView* v = context;
    bool consumed = false;

    bool have = false;
    with_view_model(v->view, CaptureModel * m, { have = m->have; }, false);

    if(event->type == InputTypeShort && have &&
       (event->key == InputKeyOk || event->key == InputKeyRight)) {
        with_view_model(v->view, CaptureModel * m, { m->have = false; }, true);
        if(v->rescan_cb) v->rescan_cb(v->rescan_ctx);
        consumed = true;
    }
    return consumed; // Back falls through to the scene manager
}

/* -------------------------------------------------------------- lifecycle */

CaptureView* capture_view_alloc(void) {
    CaptureView* v = malloc(sizeof(CaptureView));
    memset(v, 0, sizeof(CaptureView));
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

void capture_view_reset(CaptureView* v, const char* title) {
    furi_assert(v);
    with_view_model(
        v->view,
        CaptureModel * m,
        {
            m->have = false;
            m->anim = 0;
            strncpy(m->title, title ? title : "", sizeof(m->title) - 1);
            m->title[sizeof(m->title) - 1] = 0;
        },
        true);
}

void capture_view_tick(CaptureView* v) {
    furi_assert(v);
    with_view_model(v->view, CaptureModel * m, { m->anim++; }, true);
}

void capture_view_set_result(CaptureView* v, const CaptureAnnot* a) {
    furi_assert(v);
    furi_assert(a);
    with_view_model(
        v->view,
        CaptureModel * m,
        {
            m->annot = *a;
            m->have = true;
        },
        true);
}

void capture_view_set_rescan_cb(CaptureView* v, CaptureRescanCb cb, void* ctx) {
    furi_assert(v);
    v->rescan_cb = cb;
    v->rescan_ctx = ctx;
}
