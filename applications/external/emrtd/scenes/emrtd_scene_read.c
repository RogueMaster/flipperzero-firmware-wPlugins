/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * The read itself.
 *
 * The worker runs on the NFC stack's thread and reports through a callback
 * that does two things and no more: it copies the progress into the
 * application object and posts a custom event. Everything the user sees is
 * drawn from here, on the GUI thread, when that event arrives. The poller is
 * never stopped from inside its own callback - that deadlocks - so it is
 * stopped in on_exit, after the scene has been left.
 */
#include "../emrtd_i.h"

#include <dolphin/dolphin.h>

#define TAG "EmrtdSceneRead"

static bool emrtd_scene_read_worker_callback(const EmrtdWorkerProgress* progress, void* context) {
    furi_assert(progress);
    furi_assert(context);
    Emrtd* app = context;

    const EmrtdWorkerStage previous = app->progress.stage;
    app->progress = *progress;

    uint32_t event = EmrtdCustomEventWorkerProgress;
    if(progress->stage == EmrtdWorkerStageDone) {
        event = EmrtdCustomEventWorkerSuccess;
    } else if(progress->stage == EmrtdWorkerStageError) {
        event = EmrtdCustomEventWorkerError;
    } else if(
        previous <= EmrtdWorkerStageAuthenticating &&
        progress->stage > EmrtdWorkerStageAuthenticating &&
        progress->stage < EmrtdWorkerStageDone) {
        /*
         * The first stage past authentication, whether or not the worker
         * stopped on Authenticating long enough to report it. This is the
         * first moment the access method can be named on screen; failure is
         * already handled above, so reaching here means a driver succeeded.
         */
        event = EmrtdCustomEventWorkerAuthenticated;
    }

    view_dispatcher_send_custom_event(app->view_dispatcher, event);

    /* Stopping is asked for through emrtd_worker_stop() from the GUI thread;
     * returning false here would abort the read on every redraw. */
    return true;
}

static void emrtd_scene_read_view_callback(void* context) {
    furi_assert(context);
    Emrtd* app = context;

    view_dispatcher_send_custom_event(app->view_dispatcher, EmrtdCustomEventViewExit);
}

/**
 * Refuse the read rather than let the allocator take the device down.
 *
 * pvPortMalloc does not return NULL on this firmware. Both of its failure
 * exits - no block large enough, and not enough left in total - end in
 * furi_crash("out of memory"), which reboots. So every `if(p != NULL)` after
 * an allocation is unreachable, and the only place a shortage can be handled
 * is here, before the first byte is asked for.
 *
 * @return true when the read may start
 */
static bool emrtd_scene_read_have_memory(Emrtd* app) {
    const size_t free_min = EMRTD_HEAP_FREE_MIN;
    const size_t block_min = EMRTD_HEAP_BLOCK_MIN;

    app->heap_free = memmgr_get_free_heap();
    app->heap_largest_block = memmgr_heap_get_max_free_block();
    app->heap_host_connected = false;

    if(app->heap_free >= free_min && app->heap_largest_block >= block_min) {
        return true;
    }

    /*
     * Only now, and never on the way to a successful read: this allocates an
     * event flag and blocks on the USB thread. The USB interface is locked by
     * exactly one thing in the whole firmware - rpc_cli_command_start_session
     * - so a lock means a session is open, which is a sharper thing to say
     * than "a cable is plugged in".
     */
    app->heap_host_connected = furi_hal_usb_is_locked();

    FURI_LOG_E(
        TAG,
        "read refused: free %zu (need %zu), largest block %zu (need %zu), host %s",
        app->heap_free,
        free_min,
        app->heap_largest_block,
        block_min,
        app->heap_host_connected ? "connected" : "absent");

    return false;
}

void emrtd_scene_read_on_enter(void* context) {
    furi_assert(context);
    Emrtd* app = context;

    /* Nothing of a previous read may show through: the view keeps its model
     * between visits, and a stale stage line on a retry would be a lie. The
     * result is cleared with the secure primitive because it carries the
     * previous session's keys; the progress never holds a secret. */
    emrtd_secure_wipe_object(&app->result);
    memset(&app->progress, 0, sizeof(app->progress));
    app->progress.stage = EmrtdWorkerStageWaitingForCard;

    if(!emrtd_scene_read_have_memory(app)) {
        app->result.error = EmrtdErrorOutOfMemory;
        app->result.error_file = EmrtdFileCount;
        /* A scene cannot leave itself from inside on_enter, so the error
         * screen is reached through the event loop instead. */
        view_dispatcher_send_custom_event(app->view_dispatcher, EmrtdCustomEventReadRefused);
        return;
    }

    dolphin_deed(DolphinDeedNfcRead);

    emrtd_read_view_set_callback(app->read_view, emrtd_scene_read_view_callback, app);
    emrtd_read_view_set_access(app->read_view, NULL);
    emrtd_read_view_set_progress(app->read_view, &app->progress);

    /*
     * The largest allocation first, before anything else has had a chance to
     * eat into the block the check above measured. emrtd_worker_alloc() takes
     * about two kilobytes of its own, and on a heap whose largest free block
     * only just cleared EMRTD_HEAP_BLOCK_MIN that is enough to leave the
     * eight kilobyte stack below without a home - which on this firmware is a
     * reboot, not a failure. So the order here is part of the check.
     */
    /*
     * The radio is taken for the length of the read and given back in
     * on_exit, so that browsing saved reads does not hold the NFC hardware.
     */
    app->nfc = nfc_alloc();

    app->worker = emrtd_worker_alloc(&app->result);
    emrtd_worker_set_config(app->worker, &app->config);
    emrtd_worker_set_callback(app->worker, emrtd_scene_read_worker_callback, app);

    emrtd_worker_start(app->worker, app->nfc);

    emrtd_blink_start(app);

    view_dispatcher_switch_to_view(app->view_dispatcher, EmrtdViewRead);
}

bool emrtd_scene_read_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    Emrtd* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case EmrtdCustomEventReadRefused:
            /* on_enter allocated nothing, so there is nothing to unwind; the
             * error is already in app->result. */
            scene_manager_next_scene(app->scene_manager, EmrtdSceneReadError);
            consumed = true;
            break;

        case EmrtdCustomEventWorkerProgress:
            emrtd_read_view_set_progress(app->read_view, &app->progress);
            consumed = true;
            break;

        case EmrtdCustomEventWorkerAuthenticated: {
            emrtd_read_view_set_progress(app->read_view, &app->progress);

            /*
             * The worker writes straight into app->result, and the access
             * outcome was complete before this event was posted, so it can be
             * read here even though the rest of the record is not final until
             * the worker stops.
             */
            if(app->result.access.summary[0] != '\0') {
                emrtd_read_view_set_access(app->read_view, app->result.access.summary);
            }
            consumed = true;
            break;
        }

        case EmrtdCustomEventWorkerSuccess:
            /* app->result is the record the worker has been filling, so it
             * needs no rescuing before on_exit frees the worker. */
            dolphin_deed(DolphinDeedNfcReadSuccess);
            scene_manager_next_scene(app->scene_manager, EmrtdSceneReadSuccess);
            consumed = true;
            break;

        case EmrtdCustomEventWorkerError:
            if(app->result.error == EmrtdErrorNone) {
                /* The worker reported a failure it cannot describe. Saying so
                 * is still better than an error screen with no error on it. */
                app->result.error = EmrtdErrorInternal;
            }

            if(app->result.error == EmrtdErrorCancelled) {
                /* The user left; there is nothing to report. */
                scene_manager_previous_scene(app->scene_manager);
            } else {
                scene_manager_next_scene(app->scene_manager, EmrtdSceneReadError);
            }
            consumed = true;
            break;

        case EmrtdCustomEventViewExit:
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
            break;

        default:
            break;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void emrtd_scene_read_on_exit(void* context) {
    furi_assert(context);
    Emrtd* app = context;

    emrtd_blink_stop(app);

    /*
     * Order matters. The worker has to be stopped before it is freed, and the
     * poller has to be idle before nfc_free(), which checks that and aborts
     * the application if it is not.
     */
    if(app->worker != NULL) {
        emrtd_worker_stop(app->worker);
        emrtd_worker_free(app->worker);
        app->worker = NULL;
    }
    if(app->nfc != NULL) {
        nfc_free(app->nfc);
        app->nfc = NULL;
    }

    /*
     * The credentials are not cleared here. The worker's own copy goes when it
     * is freed above, and the one on the app outlives the read on purpose: a
     * failed read needs it for what the error screen shows and for Retry, and
     * a read that worked needs it for the next document of the same person.
     * It is wiped when the application closes (emrtd_free) and by Document -
     * Forget stored data, which clears the card as well. See docs/security.md.
     */
}
