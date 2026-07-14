#include "mouse_jiggler_worker.h"

static int32_t mouse_jiggler_worker_thread_callback(void* context) {
    furi_check(context);
    MouseJigglerWorker* instance = context;
    while(instance->is_running) {
        if(instance->is_jiggle) {
            furi_hal_hid_mouse_move(instance->mouse_move_dx, 0);
            instance->mouse_move_dx = -(instance->mouse_move_dx);
            for(uint32_t delay = 0;
                instance->is_running && instance->is_jiggle && delay < instance->delay_value;
                delay += MOUSE_JIGGLER_DELAY_MS) {
                furi_delay_ms(MOUSE_JIGGLER_DELAY_MS);
            }
        } else {
            furi_delay_ms(MOUSE_JIGGLER_DELAY_MS);
        }
    }
    return 0;
}

MouseJigglerWorker* mouse_jiggler_worker_alloc() {
    MouseJigglerWorker* instance = malloc(sizeof(MouseJigglerWorker));
    instance->thread = furi_thread_alloc();
    furi_thread_set_name(instance->thread, "MouseJigglerWorker");
    furi_thread_set_stack_size(instance->thread, 1024);
    furi_thread_set_context(instance->thread, instance);
    furi_thread_set_callback(instance->thread, mouse_jiggler_worker_thread_callback);
    instance->is_running = false;
    instance->is_jiggle = false;
    instance->delay_index = 0;
    instance->delay_value = MOUSE_JIGGLER_INTERVAL_MS[0];
    instance->mouse_move_dx = MOUSE_JIGGLER_MOUSE_MOVE_DX;
    return instance;
}

void mouse_jiggler_worker_free(MouseJigglerWorker* instance) {
    furi_check(instance);
    furi_thread_free(instance->thread);
}

void mouse_jiggler_worker_start(MouseJigglerWorker* instance) {
    furi_check(instance);
    furi_check(instance->is_running == false);
    furi_thread_start(instance->thread);
    instance->is_running = true;
}

void mouse_jiggler_worker_stop(MouseJigglerWorker* instance) {
    furi_check(instance);
    furi_check(instance->is_running == true);
    instance->is_jiggle = false;
    instance->is_running = false;
    furi_thread_join(instance->thread);
}

void mouse_jiggler_worker_toggle_jiggle(MouseJigglerWorker* instance) {
    furi_check(instance);
    instance->is_jiggle = !(instance->is_jiggle);
}

bool mouse_jiggler_worker_increase_delay(MouseJigglerWorker* instance) {
    furi_check(instance);
    furi_check(instance->is_jiggle == false);
    if(instance->delay_index == MOUSE_JIGGLER_INTERVAL_N - 1) {
        return false;
    }
    instance->delay_index++;
    instance->delay_value = MOUSE_JIGGLER_INTERVAL_MS[instance->delay_index];
    return true;
}

bool mouse_jiggler_worker_decrease_delay(MouseJigglerWorker* instance) {
    furi_check(instance);
    furi_check(instance->is_jiggle == false);
    if(instance->delay_index == 0) {
        return false;
    }
    instance->delay_index--;
    instance->delay_value = MOUSE_JIGGLER_INTERVAL_MS[instance->delay_index];
    return true;
}
