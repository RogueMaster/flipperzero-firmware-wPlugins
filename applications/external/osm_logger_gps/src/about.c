#include <furi.h>
#include <gui/canvas.h>
#include <gui/view_dispatcher.h>
#include <gui/view.h>
#include <gui/elements.h>
#include <input/input.h>

#include "app.h"
#include "about.h"

static void about_draw_callback(Canvas* canvas, void* ctx) {
    (void)ctx;
    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    elements_multiline_text_aligned(canvas, 64, 0, AlignCenter, AlignTop, "OSM Logger");

    canvas_set_font(canvas, FontSecondary);
    elements_multiline_text_aligned(canvas, 64, 14, AlignCenter, AlignTop, "GPS POI logger");
    elements_multiline_text_aligned(canvas, 64, 24, AlignCenter, AlignTop, "by Simon Grossi");
    elements_multiline_text_aligned(canvas, 64, 34, AlignCenter, AlignTop, "github.com/simongrossi");
    elements_multiline_text_aligned(canvas, 64, 44, AlignCenter, AlignTop, "MIT License");

    elements_multiline_text_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "Back");
}

static bool about_input_callback(InputEvent* event, void* ctx) {
    (void)event;
    (void)ctx;
    return false; // Back -> previous_callback
}

static uint32_t about_previous_callback(void* ctx) {
    (void)ctx;
    return AppViewMenu;
}

void app_start_about(App* app) {
    if(!app->about_view) {
        View* view = view_alloc();
        view_set_draw_callback(view, about_draw_callback);
        view_set_input_callback(view, about_input_callback);
        view_set_previous_callback(view, about_previous_callback);

        app->about_view = view;
        view_dispatcher_add_view(app->dispatcher, AppViewAbout, view);
    }
    view_dispatcher_switch_to_view(app->dispatcher, AppViewAbout);
}

void app_stop_about(App* app) {
    if(app->about_view) {
        view_dispatcher_remove_view(app->dispatcher, AppViewAbout);
        view_free(app->about_view);
        app->about_view = NULL;
    }
}
