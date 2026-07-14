#include "../lib/bunnyconnect_views.h"
#include <furi.h>
#include <gui/view.h>
#include <gui/gui.h>
#include <input/input.h>

struct BunnyConnectCustomView {
    View* view;
    BunnyConnectViewCallback callback;
    void* context;
};

typedef struct {
    int selected_item;
    int item_count;
} BunnyConnectViewModel;

static void bunnyconnect_view_draw_callback(Canvas* canvas, void* model) {
    BunnyConnectViewModel* view_model = model;
    UNUSED(view_model);

    canvas_clear(canvas);

    // Draw a simple interface
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 10, 20, "BunnyConnect Custom View");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 10, 35, "Press OK to continue");
}

static bool bunnyconnect_view_input_callback(InputEvent* event, void* context) {
    BunnyConnectCustomView* custom_view = context;
    furi_assert(custom_view);

    bool consumed = false;

    if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(custom_view->callback) {
            custom_view->callback(custom_view->context);
        }
        consumed = true;
    }

    return consumed;
}

BunnyConnectCustomView* bunnyconnect_view_alloc(void) {
    BunnyConnectCustomView* custom_view = malloc(sizeof(BunnyConnectCustomView));
    custom_view->view = view_alloc();

    view_allocate_model(custom_view->view, ViewModelTypeLocking, sizeof(BunnyConnectViewModel));
    view_set_context(custom_view->view, custom_view);
    view_set_draw_callback(custom_view->view, bunnyconnect_view_draw_callback);
    view_set_input_callback(custom_view->view, bunnyconnect_view_input_callback);

    return custom_view;
}

void bunnyconnect_view_free(BunnyConnectCustomView* custom_view) {
    furi_assert(custom_view);
    view_free(custom_view->view);
    free(custom_view);
}

View* bunnyconnect_view_get_view(BunnyConnectCustomView* custom_view) {
    furi_assert(custom_view);
    return custom_view->view;
}

void bunnyconnect_view_set_callback(
    BunnyConnectCustomView* custom_view,
    BunnyConnectViewCallback callback,
    void* context) {
    furi_assert(custom_view);
    custom_view->callback = callback;
    custom_view->context = context;
}
