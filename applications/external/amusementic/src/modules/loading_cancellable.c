#include "loading_cancellable.h"

#include <gui/icon_animation.h>
#include <gui/elements.h>
#include <gui/canvas.h>
#include <gui/view.h>
#include <input/input.h>

#include <furi.h>
#include <aic_icons.h>
#include <stdint.h>

struct LoadingCancellable {
    View* view;
};

typedef struct {
    IconAnimation* icon;
} LoadingCancellableModel;

static void loading_cancellable_draw_callback(Canvas* canvas, void* _model) {
    LoadingCancellableModel* model = (LoadingCancellableModel*)_model;

    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 0, 0, canvas_width(canvas), canvas_height(canvas));
    canvas_set_color(canvas, ColorBlack);

    uint8_t x = canvas_width(canvas) / 2 - 24 / 2;
    uint8_t y = canvas_height(canvas) / 2 - 24 / 2;

    canvas_draw_icon(canvas, x, y, &A_Loading_24);

    canvas_draw_icon_animation(canvas, x, y, model->icon);
}

static bool loading_cancellable_input_callback(InputEvent* event, void* context) {
    UNUSED(event);
    UNUSED(context);
    return false; // don't consume any inputs
}

static void loading_cancellable_enter_callback(void* context) {
    furi_assert(context);
    LoadingCancellable* instance = context;
    LoadingCancellableModel* model = view_get_model(instance->view);
    /* using LoadingCancellable View in conjunction with several
     * Stack View obligates to reassign
     * Update callback, as it can be rewritten
     */
    view_tie_icon_animation(instance->view, model->icon);
    icon_animation_start(model->icon);
    view_commit_model(instance->view, false);
}

static void loading_cancellable_exit_callback(void* context) {
    furi_assert(context);
    LoadingCancellable* instance = context;
    LoadingCancellableModel* model = view_get_model(instance->view);
    icon_animation_stop(model->icon);
    view_commit_model(instance->view, false);
}

LoadingCancellable* loading_cancellable_alloc(void) {
    LoadingCancellable* instance = malloc(sizeof(LoadingCancellable));
    instance->view = view_alloc();
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(LoadingCancellableModel));
    LoadingCancellableModel* model = view_get_model(instance->view);
    model->icon = icon_animation_alloc(&A_Loading_24);
    view_tie_icon_animation(instance->view, model->icon);
    view_commit_model(instance->view, false);

    view_set_context(instance->view, instance);
    view_set_draw_callback(instance->view, loading_cancellable_draw_callback);
    view_set_input_callback(instance->view, loading_cancellable_input_callback);
    view_set_enter_callback(instance->view, loading_cancellable_enter_callback);
    view_set_exit_callback(instance->view, loading_cancellable_exit_callback);

    return instance;
}

void loading_cancellable_free(LoadingCancellable* instance) {
    furi_check(instance);

    LoadingCancellableModel* model = view_get_model(instance->view);
    icon_animation_free(model->icon);
    view_commit_model(instance->view, false);

    furi_assert(instance);
    view_free(instance->view);
    free(instance);
}

View* loading_cancellable_get_view(LoadingCancellable* instance) {
    furi_check(instance);
    return instance->view;
}
