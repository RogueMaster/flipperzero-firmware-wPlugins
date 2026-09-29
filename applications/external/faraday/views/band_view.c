#include "band_view.h"

#include <furi.h>
#include <gui/gui.h>
#include <stdio.h>
#include <string.h>

struct BandView {
    View* view;
    BandViewOkCallback ok_cb;
    void* ok_ctx;
};

typedef struct {
    BandData d;
} BandModel;

/* Four band rows between the header rule (y=11) and the action strip (row 53),
 * on a 10-row grid. Each row's text sits on baseline ROW_BASE + i*ROW_STEP and
 * its bar occupies the seven rows ending on that baseline, so a row can never
 * reach above its own slot - the first version put the bar at baseline-7 with
 * height 7, which pushed row 0 up through the rule and into the header. */
#define ROW_BASE 21
#define ROW_STEP 10

static void band_view_draw(Canvas* canvas, void* model) {
    BandModel* m = model;
    const BandData* d = &m->d;
    char buf[20];

    canvas_set_font(canvas, FontSecondary);
    /* Baseline 8, not 9: "hold your fob" has a descender that would otherwise
     * land on the rule below it. */
    canvas_draw_str(canvas, 2, 8, "FIND BAND");
    canvas_draw_str_aligned(canvas, 126, 8, AlignRight, AlignBottom, "hold your fob");
    canvas_draw_line(canvas, 0, 11, 127, 11);

    for(uint8_t i = 0; i < FDY_BAND_COUNT; i++) {
        int y = ROW_BASE + i * ROW_STEP; /* text baseline for this row */
        bool win = d->done && d->found && d->winner == i;

        /* A caret follows the sweep so the screen is visibly working; the
         * winner keeps a filled marker in the same column afterwards. Both
         * stay inside the row, so neither disturbs the grid. */
        if(win) {
            canvas_draw_box(canvas, 0, y - 5, 4, 4);
        } else if(!d->done && d->active == i) {
            canvas_draw_str(canvas, 0, y, ">");
        }

        canvas_draw_str(canvas, 7, y, d->label[i] ? d->label[i] : "-");

        int bw = (44 * (d->norm[i] > 100 ? 100 : d->norm[i])) / 100;
        canvas_draw_frame(canvas, 46, y - 6, 46, 7);
        if(bw > 0) canvas_draw_box(canvas, 47, y - 5, bw, 5);

        snprintf(buf, sizeof(buf), "%d", d->peak[i]);
        canvas_draw_str_aligned(canvas, 126, y, AlignRight, AlignBottom, buf);
    }

    /* action strip */
    canvas_draw_box(canvas, 0, 53, 128, 11);
    canvas_set_color(canvas, ColorWhite);
    if(!d->done) {
        snprintf(buf, sizeof(buf), "Scanning %u/%u", (unsigned)(d->pass + 1), (unsigned)d->passes);
        canvas_draw_str(canvas, 3, 61, buf);
        canvas_draw_str_aligned(canvas, 125, 61, AlignRight, AlignBottom, "press it");
    } else if(d->found) {
        snprintf(buf, sizeof(buf), "%s +%d dB", d->label[d->winner], (int)d->margin);
        canvas_draw_str(canvas, 3, 61, buf);
        canvas_draw_str_aligned(canvas, 125, 61, AlignRight, AlignBottom, "OK use it");
    } else {
        /* Two different failures, and only one of them the user can act on. */
        canvas_draw_str(canvas, 3, 61, d->weak ? "Hold it closer" : "No fob heard");
        canvas_draw_str_aligned(canvas, 125, 61, AlignRight, AlignBottom, "OK retry");
    }
    canvas_set_color(canvas, ColorBlack);
}

static bool band_view_input(InputEvent* event, void* context) {
    BandView* v = context;
    if(event->key != InputKeyOk) return false;
    if(event->type == InputTypeShort || event->type == InputTypeLong) {
        if(v->ok_cb) v->ok_cb(v->ok_ctx);
        return true;
    }
    return false;
}

BandView* band_view_alloc(void) {
    BandView* v = malloc(sizeof(BandView));
    v->ok_cb = NULL;
    v->ok_ctx = NULL;
    v->view = view_alloc();
    view_set_context(v->view, v);
    view_set_draw_callback(v->view, band_view_draw);
    view_set_input_callback(v->view, band_view_input);
    view_allocate_model(v->view, ViewModelTypeLocking, sizeof(BandModel));
    return v;
}

void band_view_free(BandView* v) {
    furi_assert(v);
    view_free(v->view);
    free(v);
}

View* band_view_get_view(BandView* v) {
    furi_assert(v);
    return v->view;
}

void band_view_set_ok_callback(BandView* v, BandViewOkCallback cb, void* context) {
    furi_assert(v);
    v->ok_cb = cb;
    v->ok_ctx = context;
}

void band_view_update(BandView* v, const BandData* data) {
    furi_assert(v);
    with_view_model(v->view, BandModel * m, { m->d = *data; }, true);
}
