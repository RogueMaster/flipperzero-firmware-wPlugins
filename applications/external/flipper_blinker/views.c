#include "views.h"

#include <furi.h>
#include <gui/elements.h>

#define TRANSMIT_MESSAGE_SIZE (128U)
#define TRANSMIT_STATUS_SIZE  (24U)

typedef struct {
    char message[TRANSMIT_MESSAGE_SIZE];
    char status[TRANSMIT_STATUS_SIZE];
    char current_char;
    size_t current_index;
    size_t total;
} FlipperBlinkerTransmitViewModel;

struct FlipperBlinkerTransmitView {
    View* view;
    ViewNavigationCallback callback;
    void* context;
};

static void flipper_blinker_transmit_view_draw(Canvas* canvas, void* model) {
    FlipperBlinkerTransmitViewModel* view_model = model;
    char current[2] = {view_model->current_char ? view_model->current_char : '-', '\0'};
    float progress = 0.0f;

    if(view_model->total > 0) {
        progress = (float)view_model->current_index / (float)view_model->total;
        if(progress > 1.0f) progress = 1.0f;
    }

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 11, "Sending Morse");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 24, "Msg:");
    FuriString* message = furi_string_alloc_set(view_model->message);
    elements_string_fit_width(canvas, message, 100);
    canvas_draw_str(canvas, 25, 24, furi_string_get_cstr(message));
    furi_string_free(message);

    canvas_draw_str(canvas, 2, 38, "Char:");
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 36, 39, current);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 52, 38, view_model->status);

    elements_progress_bar(canvas, 2, 45, 124, progress);
    elements_button_left(canvas, "Stop");
}

static bool flipper_blinker_transmit_view_input(InputEvent* event, void* context) {
    FlipperBlinkerTransmitView* transmit_view = context;
    if((event->type == InputTypeShort) && (event->key == InputKeyBack)) {
        if(transmit_view->callback) {
            transmit_view->callback(transmit_view->context);
        }
        return true;
    }
    return false;
}

FlipperBlinkerTransmitView* flipper_blinker_transmit_view_alloc(void) {
    FlipperBlinkerTransmitView* transmit_view = malloc(sizeof(FlipperBlinkerTransmitView));
    transmit_view->view = view_alloc();
    transmit_view->callback = NULL;
    transmit_view->context = NULL;
    view_allocate_model(
        transmit_view->view, ViewModelTypeLocking, sizeof(FlipperBlinkerTransmitViewModel));
    view_set_context(transmit_view->view, transmit_view);
    view_set_draw_callback(transmit_view->view, flipper_blinker_transmit_view_draw);
    view_set_input_callback(transmit_view->view, flipper_blinker_transmit_view_input);
    return transmit_view;
}

void flipper_blinker_transmit_view_free(FlipperBlinkerTransmitView* transmit_view) {
    furi_assert(transmit_view);
    view_free(transmit_view->view);
    free(transmit_view);
}

View* flipper_blinker_transmit_view_get_view(FlipperBlinkerTransmitView* transmit_view) {
    furi_assert(transmit_view);
    return transmit_view->view;
}

void flipper_blinker_transmit_view_set_callback(
    FlipperBlinkerTransmitView* transmit_view,
    ViewNavigationCallback callback,
    void* context) {
    furi_assert(transmit_view);
    transmit_view->callback = callback;
    transmit_view->context = context;
}

void flipper_blinker_transmit_view_reset(
    FlipperBlinkerTransmitView* transmit_view,
    const char* message) {
    furi_assert(transmit_view);
    with_view_model(
        transmit_view->view,
        FlipperBlinkerTransmitViewModel * model,
        {
            strlcpy(model->message, message, sizeof(model->message));
            strlcpy(model->status, "Starting", sizeof(model->status));
            model->current_char = 0;
            model->current_index = 0;
            model->total = 0;
        },
        true);
}

void flipper_blinker_transmit_view_update(
    FlipperBlinkerTransmitView* transmit_view,
    char current_char,
    size_t current_index,
    size_t total) {
    furi_assert(transmit_view);
    with_view_model(
        transmit_view->view,
        FlipperBlinkerTransmitViewModel * model,
        {
            model->current_char = current_char;
            model->current_index = current_index;
            model->total = total;
            strlcpy(model->status, "Transmitting", sizeof(model->status));
        },
        true);
}

void flipper_blinker_transmit_view_set_status(
    FlipperBlinkerTransmitView* transmit_view,
    const char* status) {
    furi_assert(transmit_view);
    with_view_model(
        transmit_view->view,
        FlipperBlinkerTransmitViewModel * model,
        { strlcpy(model->status, status, sizeof(model->status)); },
        true);
}
