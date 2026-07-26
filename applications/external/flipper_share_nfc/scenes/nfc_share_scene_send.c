#include "../nfc_share_app.h"
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_power.h>
#include <stdlib.h>

#include "nfc_transport.h"
#include "nfc_share.h"

#define NSH_IDLE_OPERATION 5 //ms (sends block in the transport mailbox when the poller is slower)

#define TAG "NfcShareSend"

typedef struct {
    uint32_t counter;
    bool reading_complete;
    FuriThread* worker_thread;
} FileReadingState;

static void dialog_ex_callback(DialogExResult result, void* context);
static void update_timer_callback(void* context);

static FileReadingState* file_reading_state_alloc() {
    FileReadingState* state = malloc(sizeof(FileReadingState));
    if(!state) return NULL;
    state->counter = 0;
    state->reading_complete = false;
    state->worker_thread = NULL;
    return state;
}

static void file_reading_state_free(FileReadingState* state) {
    if(state->worker_thread) {
        furi_thread_free(state->worker_thread);
    }
    free(state);
}

static int32_t file_read_worker_thread(void* context) {
    NfcShareApp* app = context;
    FileReadingState* state = (FileReadingState*)app->file_reading_state;

    bool is_running = true;

    if(!nsh_init_from_external_transmit(app->selected_file_path)) {
        // cancelled while hashing or failed to open — nothing to send
        return 0;
    }

    state->counter = g.s_file_size;

    while(is_running) {
        nsh_idle();

        furi_delay_ms(NSH_IDLE_OPERATION);

        // Check if we should stop
        if(furi_thread_flags_get() & NSH_WORKER_STOP_FLAG) {
            is_running = false;
        }
    }

    state->reading_complete = true;

    return 0;
}

void nfc_share_scene_send_on_enter(void* context) {
    NfcShareApp* app = context;

    // Create the shared-state lock BEFORE starting the worker/radio threads.
    nsh_lock_ensure();

    // Create state for the scene
    FileReadingState* state = file_reading_state_alloc();
    if(!state) {
        FURI_LOG_E(TAG, "send_on_enter: out of memory");
        return;
    }
    app->file_reading_state = state;

    // Setup dialog to show progress
    dialog_ex_set_header(
        app->dialog_show_file,
        "Sending via NFC...",
        64,
        SCENE_HEADER_POSITION_Y,
        AlignCenter,
        AlignTop);
    dialog_ex_set_text(app->dialog_show_file, "Starting...", 64, 32, AlignCenter, AlignCenter);
    dialog_ex_set_left_button_text(app->dialog_show_file, "Cancel");
    dialog_ex_set_right_button_text(app->dialog_show_file, NULL); // Skip right button

    // Setup callback for dialog buttons
    dialog_ex_set_context(app->dialog_show_file, app);
    dialog_ex_set_result_callback(app->dialog_show_file, dialog_ex_callback);

    // Start thread for reading file
    state->worker_thread =
        furi_thread_alloc_ex("FileReadWorker", 2048, file_read_worker_thread, app);
    furi_thread_start(state->worker_thread);

    // Show dialog
    view_dispatcher_switch_to_view(app->view_dispatcher, NfcShareViewIdShowFile);

    // Start timer for updating display
    app->timer = furi_timer_alloc(update_timer_callback, FuriTimerTypePeriodic, app);
    furi_timer_start(app->timer, SCENE_UI_UPDATE_PERIOD_MS);

    // Sender role: emulate a card, the receiving side polls it.
    nfc_transport_init(NfcTransportModeListener);
}

// Callback for handling button presses in the dialog
static void dialog_ex_callback(DialogExResult result, void* context) {
    furi_assert(context);
    NfcShareApp* app = context;

    if(result == DialogExResultLeft) {
        view_dispatcher_send_custom_event(app->view_dispatcher, DialogExResultLeft);
    }
}

static void update_timer_callback(void* context) {
    furi_assert(context);
    NfcShareApp* app = context;
    FileReadingState* state = (FileReadingState*)app->file_reading_state;
    if(!state) return;

    char progress_text[128];

    if(state->reading_complete) {
        snprintf(progress_text, sizeof(progress_text), "Complete! %lu bytes read", state->counter);
        dialog_ex_set_right_button_text(app->dialog_show_file, "OK");
    } else {
        // While nsh_init hashes the file (chunked MD5) show its progress; the
        // regular name/size/ETA text takes over once the hash completes.
        uint32_t hash_done, hash_total;
        if(!nsh_hash_progress_get(&hash_done, &hash_total)) return;
        if(hash_total > 0 && hash_done < hash_total) {
            snprintf(
                progress_text,
                sizeof(progress_text),
                "Calc checksum...%lu%%",
                (unsigned long)(((uint64_t)hash_done * 100u) / hash_total));
            dialog_ex_set_text(
                app->dialog_show_file, progress_text, 64, 32, AlignCenter, AlignCenter);
            return;
        }

        // Filename on line 1, size on line 2 (bytes if < 1 KB, else KB).
        // s_file_name/s_file_size are set once in nsh_init and stable, but snapshot
        // under the lock for consistency; skip this tick on contention.
        char fname[NSH_FILENAME_LENGTH];
        uint32_t fsize;
        if(!nsh_try_lock_ms(20)) return;
        memcpy(fname, g.s_file_name, sizeof(fname));
        fname[sizeof(fname) - 1] = '\0';
        fsize = g.s_file_size;
        nsh_unlock();

        // nsh_init has not published state yet (hashing not started or init
        // failed) — keep the "Starting..." text from on_enter.
        if(fsize == 0) return;

        // Rough ETA by the nominal constant only (the sender has no receiver-side
        // progress). Pure division by a nonzero constant; clamp for a sane display.
        uint32_t eta_sec = fsize / NSH_PAYLOAD_THROUGHPUT_BPS;
        if(eta_sec > NSH_ETA_MAX_SEC) eta_sec = NSH_ETA_MAX_SEC;
        char eta[16];
        nsh_fmt_duration(eta_sec, eta, sizeof(eta));

        if(fsize < 1024) {
            snprintf(
                progress_text,
                sizeof(progress_text),
                "%s\n%lu B  ~ %s",
                fname,
                (unsigned long)fsize,
                eta);
        } else {
            snprintf(
                progress_text,
                sizeof(progress_text),
                "%s\n%lu KB  ~ %s",
                fname,
                (unsigned long)(fsize / 1024),
                eta);
        }
    }

    dialog_ex_set_text(app->dialog_show_file, progress_text, 64, 32, AlignCenter, AlignCenter);
}

bool nfc_share_scene_send_on_event(void* context, SceneManagerEvent event) {
    NfcShareApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == DialogExResultLeft) {
            // Cancel button pressed - stop reading and return to file info
            FileReadingState* state = (FileReadingState*)app->file_reading_state;
            if(state && state->worker_thread) {
                furi_thread_flags_set(
                    furi_thread_get_id(state->worker_thread), NSH_WORKER_STOP_FLAG);
                furi_thread_join(state->worker_thread);
            }

            // Stop timer
            if(app->timer) {
                furi_timer_stop(app->timer);
                furi_timer_free(app->timer);
                app->timer = NULL;
            }

            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
        } else if(event.event == DialogExResultRight) {
            // OK button pressed - return to file info
            // Only available when completed

            // Stop timer
            if(app->timer) {
                furi_timer_stop(app->timer);
                furi_timer_free(app->timer);
                app->timer = NULL;
            }

            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        // Back button - same as Cancel
        FileReadingState* state = (FileReadingState*)app->file_reading_state;
        if(state && state->worker_thread) {
            furi_thread_flags_set(furi_thread_get_id(state->worker_thread), NSH_WORKER_STOP_FLAG);
            furi_thread_join(state->worker_thread);
        }

        // Stop timer
        if(app->timer) {
            furi_timer_stop(app->timer);
            furi_timer_free(app->timer);
            app->timer = NULL;
        }

        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void nfc_share_scene_send_on_exit(void* context) {
    NfcShareApp* app = context;

    nfc_transport_deinit();

    // Free resources
    if(app->file_reading_state) {
        file_reading_state_free((FileReadingState*)app->file_reading_state);
        app->file_reading_state = NULL;
    }

    // Just in case, check that the timer is stopped
    if(app->timer) {
        furi_timer_stop(app->timer);
        furi_timer_free(app->timer);
        app->timer = NULL;
    }

    // Worker thread is joined in on_event and the NFC stack is stopped above,
    // so free the shared context and the shared-state lock.
    nsh_deinit();
}
