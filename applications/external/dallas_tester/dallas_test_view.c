#include "dallas_test_view.h"

#include <furi.h>
#include <gui/canvas.h>
#include <dallas_tester_icons.h>
#include <string.h>

struct DallasTestView {
    View* view;
};

typedef struct {
    DallasTestResult result;
} DallasTestViewModel;

// Idle / waiting screen, mirroring the stock iButton "Reading" screen: the waiting dolphin
// on the left, a prompt on the right.
static void dallas_test_view_draw_idle(Canvas* canvas, bool bus_fault) {
    canvas_draw_icon(canvas, 0, 10, &I_DolphinWait_59x54);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(
        canvas, 93, 28, AlignCenter, AlignCenter, bus_fault ? "Bus low" : "Apply key");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas, 93, 44, AlignCenter, AlignCenter, bus_fault ? "Check contacts" : "to contacts");
}

// Live readout: family code + CRC as the bold headline, then tPDL and tPDH detail lines.
static void dallas_test_view_draw_result(Canvas* canvas, const DallasTestResult* r) {
    char buf[40];

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_icon(canvas, 2, 3, &I_ibutt_10px);
    canvas_draw_str(canvas, 16, 11, "Dallas Tester");
    canvas_draw_line(canvas, 0, 14, 127, 14);

    if(r->rom_valid) {
        snprintf(buf, sizeof(buf), "FAM %02X   CRC ok", r->rom[0]);
    } else {
        snprintf(buf, sizeof(buf), "FAM --   CRC bad");
    }
    canvas_draw_str(canvas, 2, 28, buf);

    canvas_set_font(canvas, FontSecondary);
    snprintf(
        buf,
        sizeof(buf),
        "tPDL %uus  %u-%u sd%u",
        r->tpdl_us,
        r->tpdl_min_us,
        r->tpdl_max_us,
        r->tpdl_jitter_us);
    canvas_draw_str(canvas, 2, 41, buf);

    snprintf(buf, sizeof(buf), "tPDH %uus   n=%u", r->tpdh_us, r->samples_ok);
    canvas_draw_str(canvas, 2, 53, buf);
}

static void dallas_test_view_draw_callback(Canvas* canvas, void* model) {
    DallasTestViewModel* m = model;

    canvas_clear(canvas);
    if(m->result.samples_ok == 0) {
        dallas_test_view_draw_idle(canvas, m->result.bus_fault);
    } else {
        dallas_test_view_draw_result(canvas, &m->result);
    }
}

DallasTestView* dallas_test_view_alloc(void) {
    DallasTestView* dtv = malloc(sizeof(DallasTestView));
    dtv->view = view_alloc();
    view_set_context(dtv->view, dtv);
    view_allocate_model(dtv->view, ViewModelTypeLocking, sizeof(DallasTestViewModel));
    view_set_draw_callback(dtv->view, dallas_test_view_draw_callback);
    return dtv;
}

void dallas_test_view_free(DallasTestView* dtv) {
    furi_assert(dtv);
    view_free(dtv->view);
    free(dtv);
}

View* dallas_test_view_get_view(DallasTestView* dtv) {
    furi_assert(dtv);
    return dtv->view;
}

void dallas_test_view_set_result(DallasTestView* dtv, const DallasTestResult* result) {
    furi_assert(dtv);
    with_view_model(dtv->view, DallasTestViewModel * model, { model->result = *result; }, true);
}

void dallas_test_view_reset(DallasTestView* dtv) {
    furi_assert(dtv);
    with_view_model(
        dtv->view,
        DallasTestViewModel * model,
        { memset(&model->result, 0, sizeof(model->result)); },
        true);
}
