#include "../led_remote.h"
#include "../helpers/led_remote_ir.h"
#include "../helpers/led_remote_storage.h"

typedef enum {
    LedRemoteCaptureEventSuccess,
} LedRemoteCaptureEvent;

static void led_remote_capture_rx_callback(void* context, InfraredWorkerSignal* received_signal) {
    LedRemoteApp* app = context;

    led_remote_ir_set_from_worker(app, app->learn_target, received_signal);
    app->learned[app->learn_target] = app->signals[app->learn_target].is_valid;
    view_dispatcher_send_custom_event(app->view_dispatcher, LedRemoteCaptureEventSuccess);
}

void led_remote_scene_capture_on_enter(void* context) {
    LedRemoteApp* app = context;

    snprintf(app->capture_label, sizeof(app->capture_label),
             "Learn: %s", app->button_names[app->learn_target]);

    popup_reset(app->popup);
    popup_set_header(app->popup, app->capture_label, 64, 10, AlignCenter, AlignTop);
    popup_set_text(
        app->popup, "Point remote\nand press button", 64, 36, AlignCenter, AlignCenter);

    infrared_worker_rx_enable_blink_on_receiving(app->ir_worker, true);
    infrared_worker_rx_enable_signal_decoding(app->ir_worker, true);
    infrared_worker_rx_set_received_signal_callback(
        app->ir_worker, led_remote_capture_rx_callback, app);
    infrared_worker_rx_start(app->ir_worker);

    view_dispatcher_switch_to_view(app->view_dispatcher, LedRemoteViewIdPopup);
}

bool led_remote_scene_capture_on_event(void* context, SceneManagerEvent event) {
    LedRemoteApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == LedRemoteCaptureEventSuccess) {
        led_remote_storage_save(app);
        scene_manager_search_and_switch_to_another_scene(
            app->scene_manager, LedRemoteSceneLearn);
    } else {
        scene_manager_previous_scene(app->scene_manager);
    }
    return true;
}

void led_remote_scene_capture_on_exit(void* context) {
    LedRemoteApp* app = context;
    infrared_worker_rx_stop(app->ir_worker);
    popup_reset(app->popup);
}
