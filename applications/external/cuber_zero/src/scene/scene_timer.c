#include "src/cuberzero.h"

typedef struct {
    PCUBERZERO instance;
    FuriMessageQueue* queue;
} TIMERSCENE, *PTIMERSCENE;

static void callbackTimer(void* const context) {
    furi_check(context);
}

static void callbackDraw(Canvas* const canvas, void* const context) {
    furi_check(canvas);
    furi_check(context);
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "Test String");
    canvas_draw_dot(canvas, 4, 1);
    canvas_draw_line(canvas, 3, 2, 5, 2);
    canvas_draw_line(canvas, 2, 3, 6, 3);
    canvas_draw_line(canvas, 1, 4, 7, 4);
}

static void callbackInput(InputEvent* const event, void* const context) {
    furi_check(event);
    furi_check(context);

    if(event->type == InputTypeShort && event->key == InputKeyBack) {
        furi_message_queue_put(((PTIMERSCENE)context)->queue, event, FuriWaitForever);
    }
}

void SceneTimerEnter(void* const context) {
    furi_check(context);
    const PTIMERSCENE instance = malloc(sizeof(TIMERSCENE));
    instance->instance = context;
    ViewPort* const viewport = view_port_alloc();
    instance->queue = furi_message_queue_alloc(1, sizeof(InputEvent));
    FuriTimer* const timer = furi_timer_alloc(callbackTimer, FuriTimerTypePeriodic, instance);
    view_port_draw_callback_set(viewport, callbackDraw, instance);
    view_port_input_callback_set(viewport, callbackInput, instance);
    view_dispatcher_stop(instance->instance->dispatcher);
    gui_remove_view_port(instance->instance->interface, instance->instance->dispatcher->viewport);
    gui_add_view_port(instance->instance->interface, viewport, GuiLayerFullscreen);
    furi_timer_start(timer, 1);
    const InputEvent* event;

    while(furi_message_queue_get(instance->queue, &event, FuriWaitForever) == FuriStatusOk) {
        break;
    }

    furi_timer_stop(timer);
    gui_remove_view_port(instance->instance->interface, viewport);
    gui_add_view_port(
        instance->instance->interface,
        instance->instance->dispatcher->viewport,
        GuiLayerFullscreen);
    furi_timer_free(timer);
    furi_message_queue_free(instance->queue);
    view_port_free(viewport);
    scene_manager_handle_back_event(instance->instance->manager);
    view_dispatcher_run(instance->instance->dispatcher);
    free(instance);
}
