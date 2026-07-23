#include "alert_view.h"
#include <furi.h>
#include <stdio.h>

struct AlertView {
    View* view;
    AlertViewCallback ok_cb;
    void* ok_ctx;
};

typedef struct {
    TrackerRecord rec;
    bool valid;
    uint8_t blink;
} AlertModel;

static void draw_big_warning(Canvas* canvas) {
    // triangle outline (doubled lines for weight)
    int tx = 24, ty = 16, lx = 8, ly = 50, rx = 40, ry = 50;
    canvas_draw_line(canvas, tx, ty, lx, ly);
    canvas_draw_line(canvas, tx + 1, ty, lx + 1, ly);
    canvas_draw_line(canvas, tx, ty, rx, ry);
    canvas_draw_line(canvas, tx - 1, ty, rx - 1, ry);
    canvas_draw_line(canvas, lx, ly, rx, ry);
    canvas_draw_line(canvas, lx, ly - 1, rx, ry - 1);
    // exclamation
    canvas_draw_box(canvas, 23, 26, 3, 14);
    canvas_draw_box(canvas, 23, 44, 3, 3);
}

static void alert_view_draw(Canvas* canvas, void* model) {
    AlertModel* m = model;
    char buf[40];

    // strobing banner
    if(m->blink) {
        canvas_draw_box(canvas, 0, 0, 128, 14);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_frame(canvas, 0, 0, 128, 14);
    }
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 11, AlignCenter, AlignBottom, "! TRACKER ALERT !");
    canvas_set_color(canvas, ColorBlack);

    draw_big_warning(canvas);

    // right-hand details
    canvas_set_font(canvas, FontPrimary);
    if(m->valid) {
        canvas_draw_str(canvas, 48, 28, tracker_type_short(m->rec.type));
        canvas_set_font(canvas, FontSecondary);
        uint32_t dur_s = (m->rec.last_seen - m->rec.first_seen) / 1000;
        snprintf(
            buf,
            sizeof(buf),
            "with you %02lu:%02lu",
            (unsigned long)(dur_s / 60),
            (unsigned long)(dur_s % 60));
        canvas_draw_str(canvas, 48, 40, buf);
    } else {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 48, 28, "Unknown tag");
    }
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 48, 50, "is following you");

    // footer
    canvas_draw_line(canvas, 0, 53, 127, 53);
    canvas_draw_str(canvas, 2, 62, "OK: details");
    canvas_draw_str_aligned(canvas, 126, 62, AlignRight, AlignBottom, "Back: dismiss");
}

static bool alert_view_input(InputEvent* event, void* context) {
    AlertView* av = context;
    if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(av->ok_cb) av->ok_cb(av->ok_ctx);
        return true;
    }
    return false;
}

AlertView* alert_view_alloc(void) {
    AlertView* av = malloc(sizeof(AlertView));
    av->ok_cb = NULL;
    av->ok_ctx = NULL;
    av->view = view_alloc();
    view_set_context(av->view, av);
    view_set_draw_callback(av->view, alert_view_draw);
    view_set_input_callback(av->view, alert_view_input);
    view_allocate_model(av->view, ViewModelTypeLocking, sizeof(AlertModel));
    return av;
}

void alert_view_free(AlertView* av) {
    furi_assert(av);
    view_free(av->view);
    free(av);
}

View* alert_view_get_view(AlertView* av) {
    furi_assert(av);
    return av->view;
}

void alert_view_set_record(AlertView* av, const TrackerRecord* rec) {
    furi_assert(av);
    with_view_model(
        av->view,
        AlertModel * m,
        {
            m->rec = *rec;
            m->valid = true;
            m->blink = 1;
        },
        true);
}

void alert_view_tick(AlertView* av) {
    furi_assert(av);
    with_view_model(av->view, AlertModel * m, { m->blink ^= 1; }, true);
}

void alert_view_set_ok_callback(AlertView* av, AlertViewCallback cb, void* context) {
    furi_assert(av);
    av->ok_cb = cb;
    av->ok_ctx = context;
}
