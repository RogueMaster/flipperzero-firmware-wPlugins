// views/result_view.c
#include <gui/view.h>
#include <gui/gui.h>
#include <gui/elements.h>

#include "result_view.h"

static void result_view_draw(Canvas* canvas, void* model) {
    ResultViewModel* m = (ResultViewModel*)model;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Network Info");

    canvas_set_font(canvas, FontKeyboard);

    char buf[28];

    snprintf(buf, sizeof(buf), "%s/%u", m->network, m->cidr);
    canvas_draw_str(canvas, 5, 24, buf);

    snprintf(buf, sizeof(buf), "BC: %s", m->broadcast);
    canvas_draw_str(canvas, 2, 34, buf);

    snprintf(buf, sizeof(buf), "Hst: %lu", (unsigned long)m->host_count);
    canvas_draw_str(canvas, 2, 44, buf);

    snprintf(buf, sizeof(buf), "Msk: %s", m->subnet_mask);
    canvas_draw_str(canvas, 2, 54, buf);
}

static bool result_view_input(InputEvent* event, void* context) {
    UNUSED(event);
    UNUSED(context);
    return false; // let BACK propagate to the nav callback
}

View* result_view_alloc(void) {
    View* view = view_alloc();

    // Register the draw callback
    view_set_draw_callback(view, result_view_draw);

    // Register the input callback
    view_set_input_callback(view, result_view_input);

    // Allocate the model that lives inside this view
    view_allocate_model(view, ViewModelTypeLockFree, sizeof(ResultViewModel));

    return view;
}

void result_view_free(View* view) {
    view_free(view);
}
