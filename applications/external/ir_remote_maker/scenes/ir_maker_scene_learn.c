#include "../ir_maker.h"

#define IR_MAKER_LEARN_EVENT_RECEIVED 0U

/* Runs on the InfraredWorker thread. Copy the captured signal into the staging
 * slot, then hand control back to the UI thread via a custom event. */
static void ir_maker_scene_learn_signal_callback(void* context, InfraredWorkerSignal* signal) {
    furi_assert(context);
    IrMakerApp* app = context;

    if(infrared_worker_signal_is_decoded(signal)) {
        infrared_signal_set_message(app->pending, infrared_worker_get_decoded_signal(signal));
    } else {
        const uint32_t* timings;
        size_t timings_size;
        infrared_worker_get_raw_signal(signal, &timings, &timings_size);
        infrared_signal_set_raw_signal(
            app->pending,
            timings,
            timings_size,
            INFRARED_COMMON_CARRIER_FREQUENCY,
            INFRARED_COMMON_DUTY_CYCLE);
    }

    view_dispatcher_send_custom_event(app->view_dispatcher, IR_MAKER_LEARN_EVENT_RECEIVED);
}

void ir_maker_scene_learn_on_enter(void* context) {
    furi_assert(context);
    IrMakerApp* app = context;

    /* Allocate the worker here and free it on exit: the IR HAL is only held
     * while this scene is on screen. */
    app->worker = infrared_worker_alloc();
    infrared_worker_rx_enable_signal_decoding(app->worker, true);
    infrared_worker_rx_set_received_signal_callback(
        app->worker, ir_maker_scene_learn_signal_callback, app);
    infrared_worker_rx_start(app->worker);

    notification_message(app->notifications, &sequence_blink_start_cyan);

    dialog_ex_reset(app->dialog_ex);
    dialog_ex_set_header(app->dialog_ex, "Learn button", 64, 4, AlignCenter, AlignTop);
    dialog_ex_set_text(
        app->dialog_ex,
        "Point the remote at\nthe IR port and press\nthe button",
        64,
        30,
        AlignCenter,
        AlignTop);
    view_dispatcher_switch_to_view(app->view_dispatcher, IrMakerViewDialogEx);
}

bool ir_maker_scene_learn_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    IrMakerApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == IR_MAKER_LEARN_EVENT_RECEIVED) {
            notification_message(app->notifications, &sequence_success);
            scene_manager_next_scene(app->scene_manager, IrMakerSceneLearnResult);
            consumed = true;
        }
    }

    return consumed;
}

void ir_maker_scene_learn_on_exit(void* context) {
    furi_assert(context);
    IrMakerApp* app = context;

    if(app->worker) {
        infrared_worker_rx_set_received_signal_callback(app->worker, NULL, NULL);
        infrared_worker_rx_stop(app->worker);
        infrared_worker_free(app->worker);
        app->worker = NULL;
    }
    notification_message(app->notifications, &sequence_blink_stop);
    dialog_ex_reset(app->dialog_ex);
}
