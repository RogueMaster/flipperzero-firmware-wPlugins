#include "scan_view.h"
#include <gui/elements.h>
#include <furi.h>

struct ScanView {
    View* view;
};

typedef struct {
    uint32_t phase;
} ScanModel;

static void scan_view_draw(Canvas* canvas, void* model) {
    ScanModel* m = model;
    canvas_clear(canvas);

    const int cx = 64;
    const int cy = 34;

    /* expanding reader-field rings emitted from the antenna spot */
    for(int k = 0; k < 3; k++) {
        int r = 5 + (int)((m->phase * 2 + k * 9) % 27);
        canvas_draw_circle(canvas, cx, cy, r);
    }
    /* the tag/reticle at the centre */
    canvas_draw_disc(canvas, cx, cy, 3);
    canvas_draw_line(canvas, cx - 9, cy, cx - 5, cy);
    canvas_draw_line(canvas, cx + 5, cy, cx + 9, cy);
    canvas_draw_line(canvas, cx, cy - 9, cx, cy - 5);
    canvas_draw_line(canvas, cx, cy + 5, cx, cy + 9);

    /* header */
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, cx, 2, AlignCenter, AlignTop, "Grade a Card");

    /* animated status line */
    canvas_set_font(canvas, FontSecondary);
    char dots[4] = {0};
    int nd = (m->phase / 3) % 4;
    for(int i = 0; i < nd; i++) dots[i] = '.';
    char buf[24];
    snprintf(buf, sizeof(buf), "Reading NFC%s", dots);
    canvas_draw_str_aligned(canvas, cx, 54, AlignCenter, AlignTop, buf);
    canvas_draw_str_aligned(
        canvas, cx, 63, AlignCenter, AlignBottom, "Hold badge to the back");
}

static bool scan_view_input(InputEvent* event, void* context) {
    UNUSED(event);
    UNUSED(context);
    return false; // let Back propagate to the scene manager
}

ScanView* scan_view_alloc(void) {
    ScanView* v = malloc(sizeof(ScanView));
    v->view = view_alloc();
    view_set_context(v->view, v);
    view_allocate_model(v->view, ViewModelTypeLocking, sizeof(ScanModel));
    view_set_draw_callback(v->view, scan_view_draw);
    view_set_input_callback(v->view, scan_view_input);
    return v;
}

void scan_view_free(ScanView* v) {
    furi_assert(v);
    view_free(v->view);
    free(v);
}

View* scan_view_get_view(ScanView* v) {
    furi_assert(v);
    return v->view;
}

void scan_view_reset(ScanView* v) {
    furi_assert(v);
    with_view_model(v->view, ScanModel * m, { m->phase = 0; }, true);
}

void scan_view_tick(ScanView* v) {
    furi_assert(v);
    with_view_model(v->view, ScanModel * m, { m->phase++; }, true);
}
