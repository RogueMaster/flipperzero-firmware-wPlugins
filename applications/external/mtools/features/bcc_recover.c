#include "bcc_recover.h"
#include <furi.h>
#include <notification/notification_messages.h>

#define TAG "BccRecover"

static int32_t bcc_recover_worker(void* context) {
    MToolsApp* app = context;
    while(app->bcc_worker_running) {
        if(app->bcc_state == BccRecoverStateReading) {
            notification_message(app->notifications, &sequence_blink_start_blue);
            bool ok =
                mtools_mfu_recover_diagnose(app->nfc, &app->bcc_result, &app->bcc_worker_running);
            notification_message(app->notifications, &sequence_blink_stop);
            if(!app->bcc_worker_running) break;
            if(ok) {
                app->bcc_state = BccRecoverStateDiagnosed;
                notification_message(app->notifications, &sequence_success);
                view_dispatcher_send_custom_event(app->view_dispatcher, MTOOLS_EVENT_BCC_DONE);
            } else {
                furi_delay_ms(50);
            }
        } else if(app->bcc_state == BccRecoverStateWriting) {
            notification_message(app->notifications, &sequence_blink_start_blue);
            bool ok =
                mtools_mfu_recover_repair(app->nfc, &app->bcc_result, &app->bcc_worker_running);
            notification_message(app->notifications, &sequence_blink_stop);
            if(!app->bcc_worker_running) break;
            if(ok && app->bcc_result.written) {
                app->bcc_state = BccRecoverStateSuccess;
                notification_message(app->notifications, &sequence_success);
            } else {
                app->bcc_state = BccRecoverStateFailed;
                notification_message(app->notifications, &sequence_error);
            }
            view_dispatcher_send_custom_event(app->view_dispatcher, MTOOLS_EVENT_BCC_DONE);
        } else {
            furi_delay_ms(100);
        }
    }
    return 0;
}

void mtools_bcc_recover_start(MToolsApp* app) {
    mtools_bcc_recover_stop(app);
    app->bcc_state = BccRecoverStateReading;
    memset(&app->bcc_result, 0, sizeof(app->bcc_result));
    app->magic_anim_phase = 0;
    furi_timer_start(app->magic_anim_timer, furi_ms_to_ticks(180));

    app->bcc_worker_running = true;
    app->bcc_thread = furi_thread_alloc_ex("BccWorker", 2048, bcc_recover_worker, app);
    furi_thread_start(app->bcc_thread);
    view_commit_model(app->main_view, true);
}

void mtools_bcc_recover_stop(MToolsApp* app) {
    if(app->bcc_worker_running) {
        app->bcc_worker_running = false;
        if(app->bcc_thread) {
            furi_thread_join(app->bcc_thread);
            furi_thread_free(app->bcc_thread);
            app->bcc_thread = NULL;
        }
    }
    furi_timer_stop(app->magic_anim_timer);
    notification_message(app->notifications, &sequence_blink_stop);
}

bool mtools_bcc_recover_event(MToolsApp* app, uint32_t event) {
    if(event == MTOOLS_EVENT_BCC_DONE) {
        view_commit_model(app->main_view, true);
        return true;
    }
    if(event == 14 /* MTOOLS_EVENT_MAGIC_TICK */) {
        if(app->bcc_state == BccRecoverStateReading) {
            app->magic_anim_phase = (app->magic_anim_phase + 1) % 4;
            view_commit_model(app->main_view, true);
        }
        return true;
    }
    return false;
}

void mtools_bcc_recover_input(MToolsApp* app, InputKey key) {
    if(key == InputKeyOk) {
        if(app->bcc_state == BccRecoverStateDiagnosed) {
            if(app->bcc_result.needs_recovery) {
                app->bcc_state = BccRecoverStateWriting;
                view_commit_model(app->main_view, true);
            } else {
                app->bcc_state = BccRecoverStateReading;
                view_commit_model(app->main_view, true);
            }
        } else if(app->bcc_state == BccRecoverStateSuccess || app->bcc_state == BccRecoverStateFailed) {
            app->bcc_state = BccRecoverStateReading;
            view_commit_model(app->main_view, true);
        }
    }
}
