#include "air_view.h"
#include <furi.h>
#include <stdio.h>
#include <string.h>

/* Bars: three rows between the header rule and the readout line. */
#define BAR_X     18
#define BAR_W     80
#define BAR_H     7
#define BAR_TOP   16
#define BAR_PITCH 10

#define FOOT_TOP 53

struct AirView {
    View* view;
    AirViewCallback help_cb;
    void* help_ctx;
};

typedef struct {
    AirSnapshot snap;
} AirModel;

static void air_view_draw(Canvas* canvas, void* model) {
    AirModel* m = model;
    const AirSnapshot* s = &m->snap;
    char buf[24];

    /* ---- header ---- */
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Air Check");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 126, 10, AlignRight, AlignBottom, "ONBOARD");
    canvas_draw_line(canvas, 0, 12, 127, 12);

    /* ---- one bar per BLE advertising channel ---- */
    canvas_set_font(canvas, FontSecondary);
    for(size_t c = 0; c < AIR_ADV_CHANNELS; c++) {
        int y = BAR_TOP + (int)c * BAR_PITCH;
        int baseline = y + BAR_H;

        canvas_draw_str(canvas, 2, baseline, air_adv_label[c]);
        canvas_draw_frame(canvas, BAR_X, y, BAR_W, BAR_H);

        uint8_t pct = s->busy_pct[c];
        if(pct > 100) pct = 100;
        int fill = (BAR_W - 2) * pct / 100;
        if(fill > 0) canvas_draw_box(canvas, BAR_X + 1, y + 1, fill, BAR_H - 2);

        snprintf(buf, sizeof(buf), "%u%%", (unsigned)pct);
        canvas_draw_str_aligned(canvas, 126, baseline, AlignRight, AlignBottom, buf);
    }

    /* ---- what the numbers rest on ---- */
    if(s->valid) {
        snprintf(buf, sizeof(buf), "floor %d dBm", (int)s->floor_dbm);
    } else {
        snprintf(buf, sizeof(buf), "warming up");
    }
    canvas_draw_str(canvas, 2, 51, buf);

    uint32_t n = s->samples > 99999UL ? 99999UL : s->samples;
    snprintf(buf, sizeof(buf), "n=%lu", (unsigned long)n);
    canvas_draw_str_aligned(canvas, 126, 51, AlignRight, AlignBottom, buf);

    /* ---- verdict band ----
     * "energy only" rides alongside the verdict on every single frame, and it
     * is not decoration. This radio reports how much RF is in the air and
     * nothing else - it cannot read an address, a vendor or a payload - so a
     * word like BUSY must never be allowed to read as "trackers found". */
    canvas_draw_box(canvas, 0, FOOT_TOP, 128, 11);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str(canvas, 3, 61, air_band_label(air_check_band(s)));
    canvas_draw_str_aligned(canvas, 125, 61, AlignRight, AlignBottom, "energy only");
    canvas_set_color(canvas, ColorBlack);
}

static bool air_view_input(InputEvent* event, void* context) {
    AirView* av = context;
    /* Short OR Long: the firmware sends Long, never Short, once a key is held
     * past the long-press threshold. */
    if(event->type != InputTypeShort && event->type != InputTypeLong) return false;
    if(event->key == InputKeyLeft) {
        if(av->help_cb) av->help_cb(av->help_ctx);
        return true;
    }
    return false;
}

AirView* air_view_alloc(void) {
    AirView* av = malloc(sizeof(AirView));
    memset(av, 0, sizeof(AirView));
    av->view = view_alloc();
    view_set_context(av->view, av);
    view_set_draw_callback(av->view, air_view_draw);
    view_set_input_callback(av->view, air_view_input);
    view_allocate_model(av->view, ViewModelTypeLocking, sizeof(AirModel));
    return av;
}

void air_view_free(AirView* av) {
    furi_assert(av);
    view_free(av->view);
    free(av);
}

View* air_view_get_view(AirView* av) {
    furi_assert(av);
    return av->view;
}

void air_view_set_snapshot(AirView* av, const AirSnapshot* snap) {
    furi_assert(av);
    with_view_model(av->view, AirModel * m, { m->snap = *snap; }, true);
}

void air_view_set_help_callback(AirView* av, AirViewCallback cb, void* context) {
    furi_assert(av);
    av->help_cb = cb;
    av->help_ctx = context;
}
