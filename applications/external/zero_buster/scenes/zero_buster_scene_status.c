#include "zero_buster_scene.h"
#include "zero_buster.h"
#include "helpers/zero_buster_worker.h"

bool zero_buster_scene_status_on_event(ZeroBusterApp* app, InputEvent* event) {
    if(event->type == InputTypeShort) {
        if(app->state == State_Completed || app->state == State_Error) {
            if(event->key == InputKeyOk || event->key == InputKeyBack) {
                app->state = State_Idle;
            }
        } else {
            if(event->key == InputKeyBack) {
                app->worker_stop = true;
                if(app->worker_thread) {
                    furi_thread_flags_set(furi_thread_get_id(app->worker_thread), WORKER_FLAG_RX);
                }
                zero_buster_release_usb(app);
                app->state = State_Idle;
            }
        }
    }
    return true;
}
