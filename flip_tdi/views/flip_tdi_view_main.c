#include "flip_tdi_view_main.h"
#include "../flip_tdi_app_i.h"

#include <input/input.h>
#include <gui/elements.h>

struct FlipTDIViewMainType {
    View* view;
    FlipTDIViewMainTypeCallback callback;
    void* context;
};

typedef struct {
    uint32_t test;
} FlipTDIViewMainTypeModel;

void flip_tdi_view_main_set_callback(
    FlipTDIViewMainType* instance,
    FlipTDIViewMainTypeCallback callback,
    void* context) {
    furi_assert(instance);
    furi_assert(callback);
    instance->callback = callback;
    instance->context = context;
}

void flip_tdi_view_main_draw(Canvas* canvas, FlipTDIViewMainTypeModel* model) {
    UNUSED(model);
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_icon(canvas, 0, 0, &I_flip_tdi);
    elements_button_right(canvas, "More");
}

bool flip_tdi_view_main_input(InputEvent* event, void* context) {
    furi_assert(context);
    FlipTDIViewMainType* instance = context;
    UNUSED(instance);

    if(event->key == InputKeyBack) {
        return false;
    } else if(event->key == InputKeyRight && event->type == InputTypeShort) {
        instance->callback(FlipTDICustomEventMainMore, instance->context);
    }
    return true;
}

void flip_tdi_view_main_enter(void* context) {
    furi_assert(context);
    FlipTDIViewMainType* instance = context;
    with_view_model(instance->view, FlipTDIViewMainTypeModel * model, { model->test = 0; }, true);
}

void flip_tdi_view_main_exit(void* context) {
    furi_assert(context);
    FlipTDIViewMainType* instance = context;
    UNUSED(instance);
}

FlipTDIViewMainType* flip_tdi_view_main_alloc() {
    FlipTDIViewMainType* instance = malloc(sizeof(FlipTDIViewMainType));

    // View allocation and configuration
    instance->view = view_alloc();

    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(FlipTDIViewMainTypeModel));
    view_set_context(instance->view, instance);
    view_set_draw_callback(instance->view, (ViewDrawCallback)flip_tdi_view_main_draw);
    view_set_input_callback(instance->view, flip_tdi_view_main_input);
    view_set_enter_callback(instance->view, flip_tdi_view_main_enter);
    view_set_exit_callback(instance->view, flip_tdi_view_main_exit);

    with_view_model(instance->view, FlipTDIViewMainTypeModel * model, { model->test = 0; }, true);
    return instance;
}

void flip_tdi_view_main_free(FlipTDIViewMainType* instance) {
    furi_assert(instance);

    view_free(instance->view);
    free(instance);
}

View* flip_tdi_view_main_get_view(FlipTDIViewMainType* instance) {
    furi_assert(instance);
    return instance->view;
}
