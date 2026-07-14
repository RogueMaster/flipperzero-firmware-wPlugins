#include "mouse_jiggler.h"
#include "mouse_jiggler_resources.h"

static void mouse_jiggler_render_callback(Canvas* canvas, void* ctx) {
    const MouseJiggler* mouse_jiggler = ctx;
    furi_check(furi_mutex_acquire(mouse_jiggler->mutex, FuriWaitForever) == FuriStatusOk);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 5, 13, "USB Mouse Jiggler");
    canvas_draw_str(canvas, 5, 41, "interval");
    canvas_draw_str(canvas, 5, 28, "status");

    canvas_set_font(canvas, FontKeyboard);
    canvas_draw_str(
        canvas, 60, 41, MOUSE_JIGGLER_INTERVAL_STR[mouse_jiggler->worker->delay_index]);

    if(mouse_jiggler->worker->is_jiggle) {
        canvas_draw_str(canvas, 60, 28, "RUNNING");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 108, 60, "stop");
    } else {
        canvas_draw_str(canvas, 60, 28, "stopped");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 14, 60, "start");
        canvas_draw_str(canvas, 108, 60, "exit");

        canvas_draw_xbm(canvas, 5, 53, 7, 7, MOUSE_JIGGLER_IMAGE_OK_BUTTON);
        canvas_draw_xbm(canvas, 107, 32, 7, 4, MOUSE_JIGGLER_IMAGE_UP_BUTTON);
        canvas_draw_xbm(canvas, 107, 38, 7, 4, MOUSE_JIGGLER_IMAGE_DOWN_BUTTON);
    }

    canvas_draw_xbm(canvas, 95, 52, 10, 8, MOUSE_JIGGLER_IMAGE_BACK_BUTTON);
    furi_mutex_release(mouse_jiggler->mutex);
}

static void mouse_jiggler_input_callback(InputEvent* input_event, void* ctx) {
    furi_check(ctx);
    const MouseJiggler* mouse_jiggler = ctx;
    if(input_event->type == InputTypePress) {
        furi_message_queue_put(mouse_jiggler->event_queue, input_event, FuriWaitForever);
    }
}

MouseJiggler* mouse_jiggler_alloc() {
    MouseJiggler* instance = malloc(sizeof(MouseJiggler));

    instance->exit = false;
    instance->worker = mouse_jiggler_worker_alloc();
    instance->mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    instance->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    instance->view_port = view_port_alloc();
    view_port_draw_callback_set(instance->view_port, mouse_jiggler_render_callback, instance);
    view_port_input_callback_set(instance->view_port, mouse_jiggler_input_callback, instance);

    instance->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(instance->gui, instance->view_port, GuiLayerFullscreen);

    return instance;
}

void mouse_jiggler_free(MouseJiggler* instance) {
    gui_remove_view_port(instance->gui, instance->view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(instance->view_port);
    mouse_jiggler_worker_free(instance->worker);
    furi_message_queue_free(instance->event_queue);
    furi_mutex_free(instance->mutex);
    free(instance->worker);
    free(instance);
}

int32_t mouse_jiggler_app() {
    MouseJiggler* mouse_jiggler = mouse_jiggler_alloc();
    MouseJigglerWorker* worker = mouse_jiggler->worker;
    FuriHalUsbInterface* usb_mode_prev = furi_hal_usb_get_config();
    InputEvent event;
    furi_hal_usb_set_config(&usb_hid, NULL);
    mouse_jiggler_worker_start(worker);

    while(!mouse_jiggler->exit &&
          furi_message_queue_get(mouse_jiggler->event_queue, &event, FuriWaitForever) ==
              FuriStatusOk) {
        furi_check(furi_mutex_acquire(mouse_jiggler->mutex, FuriWaitForever) == FuriStatusOk);
        switch(event.key) {
        case InputKeyUp:
            if(!worker->is_jiggle && mouse_jiggler_worker_increase_delay(worker)) {
                view_port_update(mouse_jiggler->view_port);
            }
            break;
        case InputKeyDown:
            if(!worker->is_jiggle && mouse_jiggler_worker_decrease_delay(worker)) {
                view_port_update(mouse_jiggler->view_port);
            }
            break;
        case InputKeyOk:
            if(!worker->is_jiggle) {
                mouse_jiggler_worker_toggle_jiggle(worker);
                view_port_update(mouse_jiggler->view_port);
            }
            break;
        case InputKeyBack:
            if(!worker->is_jiggle) {
                mouse_jiggler->exit = true;
            } else {
                mouse_jiggler_worker_toggle_jiggle(worker);
                view_port_update(mouse_jiggler->view_port);
            }
            break;
        default:
            break;
        }
        furi_mutex_release(mouse_jiggler->mutex);
    }
    mouse_jiggler_worker_stop(worker);
    mouse_jiggler_free(mouse_jiggler);
    furi_hal_usb_set_config(usb_mode_prev, NULL);
    return 0;
}
