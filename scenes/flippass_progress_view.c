#include "flippass_progress_view.h"
#include "flippass_open_input_gate.h"

#include <furi.h>
#include <gui/elements.h>
#include <stdio.h>
#include <string.h>

#define FLIPPASS_PROGRESS_TITLE_SIZE  32U
#define FLIPPASS_PROGRESS_STAGE_SIZE  48U
#define FLIPPASS_PROGRESS_DETAIL_SIZE 80U

typedef struct {
    char title[FLIPPASS_PROGRESS_TITLE_SIZE];
    char stage[FLIPPASS_PROGRESS_STAGE_SIZE];
    char detail[FLIPPASS_PROGRESS_DETAIL_SIZE];
    uint8_t progress;
} FlipPassProgressViewModel;

struct FlipPassProgressView {
    View* view;
    ViewPort* guard_port;
    Gui* gui;
    FlipPassOpenInputGate gate;
};

static bool flippass_progress_consume_input(InputEvent* event, void* context) {
    UNUSED(event);
    UNUSED(context);
    return true;
}

static void flippass_progress_view_draw_bar(Canvas* canvas, uint8_t progress) {
    const uint8_t bar_x = 12U;
    const uint8_t bar_y = 28U;
    const uint8_t bar_w = 104U;
    const uint8_t bar_h = 9U;
    const uint8_t inner_w = bar_w - 2U;
    const uint8_t clamped = (progress <= 100U) ? progress : 100U;
    const uint8_t fill_w = (uint8_t)((inner_w * clamped) / 100U);

    elements_slightly_rounded_frame(canvas, bar_x, bar_y, bar_w, bar_h);
    if(fill_w > 0U) {
        canvas_draw_box(canvas, bar_x + 1U, bar_y + 1U, fill_w, bar_h - 2U);
    }
}

static void flippass_progress_view_draw_callback(Canvas* canvas, void* _model) {
    FlipPassProgressViewModel* model = _model;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 4, AlignCenter, AlignTop, model->title);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 18, AlignCenter, AlignTop, model->stage);

    flippass_progress_view_draw_bar(canvas, model->progress);

    elements_multiline_text_aligned(canvas, 64, 42, AlignCenter, AlignTop, model->detail);
}

static void flippass_progress_guard_draw(Canvas* canvas, void* context);
static void flippass_progress_guard_port_input(InputEvent* event, void* context);

FlipPassProgressView* flippass_progress_view_alloc(Gui* gui) {
    FlipPassProgressView* progress_view = calloc(1, sizeof(FlipPassProgressView));
    furi_check(progress_view);

    progress_view->view = view_alloc();
    view_allocate_model(
        progress_view->view, ViewModelTypeLocking, sizeof(FlipPassProgressViewModel));
    view_set_draw_callback(progress_view->view, flippass_progress_view_draw_callback);
    view_set_input_callback(progress_view->view, flippass_progress_consume_input);
    flippass_progress_view_reset(progress_view);
    /* Register once before the dispatcher runs. Adding a port while GUI is
     * blocked on a full dispatcher queue would wait on GUI's mutex forever. */
    progress_view->gui = gui;
    progress_view->guard_port = view_port_alloc();
    furi_check(progress_view->guard_port);
    view_port_enabled_set(progress_view->guard_port, false);
    view_port_draw_callback_set(
        progress_view->guard_port, flippass_progress_guard_draw, progress_view);
    view_port_input_callback_set(
        progress_view->guard_port, flippass_progress_guard_port_input, progress_view);
    gui_add_view_port(gui, progress_view->guard_port, GuiLayerFullscreen);
    return progress_view;
}

void flippass_progress_view_free(FlipPassProgressView* view) {
    furi_check(view);

    flippass_progress_guard_end(view);
    gui_remove_view_port(view->gui, view->guard_port);
    view_port_free(view->guard_port);
    view_free(view->view);
    free(view);
}

View* flippass_progress_view_get_view(FlipPassProgressView* view) {
    furi_check(view);
    return view->view;
}

void flippass_progress_view_reset(FlipPassProgressView* view) {
    furi_check(view);

    with_view_model(
        view->view,
        FlipPassProgressViewModel * model,
        {
            snprintf(model->title, sizeof(model->title), "%s", "Working");
            snprintf(model->stage, sizeof(model->stage), "%s", "Preparing");
            model->detail[0] = '\0';
            model->progress = 0U;
        },
        true);
}

void flippass_progress_view_set_state(
    FlipPassProgressView* view,
    const char* title,
    const char* stage,
    const char* detail,
    uint8_t progress) {
    furi_check(view);

    with_view_model(
        view->view,
        FlipPassProgressViewModel * model,
        {
            snprintf(model->title, sizeof(model->title), "%s", title != NULL ? title : "");
            snprintf(model->stage, sizeof(model->stage), "%s", stage != NULL ? stage : "");
            snprintf(model->detail, sizeof(model->detail), "%s", detail != NULL ? detail : "");
            model->progress = progress;
        },
        true);
    if(view->guard_port != NULL) view_port_update(view->guard_port);
}

static void flippass_progress_guard_draw(Canvas* canvas, void* context) {
    FlipPassProgressView* view = context;
    with_view_model(
        view->view,
        FlipPassProgressViewModel * model,
        { flippass_progress_view_draw_callback(canvas, model); },
        false);
}

static void flippass_progress_guard_port_input(InputEvent* event, void* context) {
    /* Hardware and GUI RPC both publish through RECORD_INPUT_EVENTS. The app
     * subscription records Back and held keys; this port prevents queueing. */
    UNUSED(event);
    UNUSED(context);
}

void flippass_progress_guard_begin(FlipPassProgressView* view) {
    furi_assert(view);
    view->gate = (FlipPassOpenInputGate){.active = true, .finished_tick = furi_get_tick()};
    view_port_enabled_set(view->guard_port, true);
}

void flippass_progress_guard_end(FlipPassProgressView* view) {
    if(view == NULL || view->guard_port == NULL) return;
    view_port_enabled_set(view->guard_port, false);
    view->gate.active = false;
}

bool flippass_progress_guard_active(const FlipPassProgressView* view) {
    return view != NULL && view->gate.active;
}

void flippass_progress_guard_input(FlipPassProgressView* view, const InputEvent* event) {
    if(view != NULL) flippass_open_input_gate_event(&view->gate, event);
}

void flippass_progress_guard_suspend(FlipPassProgressView* view, bool suspended) {
    /* Only an active open may toggle the port; enabling it outside a guarded
     * open would swallow all input with nothing left to disable it again. */
    if(view == NULL || view->guard_port == NULL || !view->gate.active) return;
    view->gate.suspended = suspended;
    view->gate.held = 0U;
    view_port_enabled_set(view->guard_port, !suspended);
}

void flippass_progress_guard_finish(FlipPassProgressView* view) {
    view->gate.finished_tick = furi_get_tick();
}

bool flippass_progress_guard_drained(const FlipPassProgressView* view) {
    return flippass_open_input_gate_drained(&view->gate, furi_get_tick(), furi_ms_to_ticks(200U));
}

bool flippass_progress_guard_canceled(const FlipPassProgressView* view) {
    return view != NULL && view->gate.active && view->gate.canceled;
}

bool flippass_progress_guard_exit_requested(const FlipPassProgressView* view) {
    return view != NULL && view->gate.active && view->gate.exit_requested;
}
