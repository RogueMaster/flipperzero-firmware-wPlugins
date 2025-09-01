#include "../flipper_share_app.h"
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_power.h>
#include <stdlib.h>

#include "subghz_share.h"
#include "flipper_share.h"

#define FS_IDLE_OPERATION 50 //ms

#define TAG "FlipperShareSend"

typedef struct {
    uint32_t counter;
    bool reading_complete;
    FuriThread* worker_thread;
} FileReadingState;

static void dialog_ex_callback(DialogExResult result, void* context);
static void update_timer_callback(void* context);

static FileReadingState* file_state_alloc() {
    FileReadingState* state = malloc(sizeof(FileReadingState));
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
    FlipperShareApp* app = context;
    FileReadingState* state = (FileReadingState*)app->file_reading_state;

    bool is_running = true;

    FURI_LOG_I(
        TAG,
        "file_read_worker_thread: file: %s, size: %zu bytes",
        app->selected_file_path,
        app->selected_file_size);

    fs_init_from_external_receive();

    while(is_running) {
        // fs_send_announce();
        fs_idle();
        furi_delay_ms(FS_IDLE_OPERATION);

        // "r_file_path=%s", g.r_file_path);
        state->counter = (g.r_blocks_received * 100) / g.r_blocks_needed;

        if(g.r_is_finished) {
            state->reading_complete = true;
        }

        // Check if we should stop
        if(furi_thread_flags_get() & 0x1) {
            is_running = false;
        }
    }

    state->reading_complete = true;

    return 0;
}

void flipper_share_scene_receive_on_enter(void* context) {
    FlipperShareApp* app = context;

    // Create state for the scene
    FileReadingState* state = file_state_alloc();
    app->file_reading_state = state;

    // Setup dialog to show progress
    dialog_ex_set_header(app->dialog_show_file, "Receiving...", 64, 10, AlignCenter, AlignCenter);
    dialog_ex_set_text(app->dialog_show_file, "Starting...", 64, 32, AlignCenter, AlignCenter);
    dialog_ex_set_left_button_text(app->dialog_show_file, "Back");
    dialog_ex_set_right_button_text(app->dialog_show_file, NULL); // Skip right button

    // Setup callback for dialog buttons
    dialog_ex_set_context(app->dialog_show_file, app);
    dialog_ex_set_result_callback(app->dialog_show_file, dialog_ex_callback);

    // Start thread for reading file
    state->worker_thread =
        furi_thread_alloc_ex("FileReadWorker", 2048, file_read_worker_thread, app);
    furi_thread_start(state->worker_thread);

    // Show dialog
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperShareViewIdShowFile);

    // Start timer for updating display
    app->timer = furi_timer_alloc(update_timer_callback, FuriTimerTypePeriodic, app);
    furi_timer_start(app->timer, 250);

    ss_subghz_init(); // TODO Move to thread?
}

// Callback for handling button presses in the dialog
static void dialog_ex_callback(DialogExResult result, void* context) {
    furi_assert(context);
    FlipperShareApp* app = context;

    if(result == DialogExResultLeft) {
        view_dispatcher_send_custom_event(app->view_dispatcher, DialogExResultLeft);
    }
}

static void update_timer_callback(void* context) {
    furi_assert(context);
    FlipperShareApp* app = context;

    FileReadingState* state = (FileReadingState*)app->file_reading_state;

    uint8_t parts_bits[FS_PARTS_BYTES];
    char bits_s[FS_PARTS_COUNT];    // debug
    uint32_t parts_total = fs_parts_count();

    if(state) {
        // char progress_text[64];
        char progress_text[255];
        if(state->reading_complete) {
            if(g.r_is_success) {
                dialog_ex_set_header(
                    app->dialog_show_file, "Success!", 64, 10, AlignCenter, AlignCenter);
            } else {
                dialog_ex_set_header(
                    app->dialog_show_file, "Hash failed", 64, 10, AlignCenter, AlignCenter);
            }
            const char* prefix = "Saved to:\n";
            // print prefix, then append path with limit
            int pref_len = snprintf(progress_text, sizeof(progress_text), "%s", prefix);
            if(pref_len < 0) pref_len = 0;
            int avail = (int)sizeof(progress_text) - pref_len - 1;
            if(avail < 0) avail = 0;
            snprintf(progress_text + pref_len, (size_t)avail + 1, "%.*s", avail, g.r_file_path);

            // dialog_ex_set_left_button_text(app->dialog_show_file, NULL); // Disable Cancel
            //  dialog_ex_set_right_button_text(app->dialog_show_file, "OK");
        } else {
            if(g.r_locked == true) { // show filename

                fs_parts_bitmap_copy(parts_bits);
                // uint32_t parts_full = 0;
                // for (uint32_t i = 0; i < parts_total; ++i) {
                //     uint8_t bit = (parts_bits[i >> 3] >> (i & 7u)) & 1u;
                //     parts_full += bit;
                // }

                // print to CLI all the bits 1 or 0 in one line for debug:                
                for (uint32_t i = 0; i < parts_total; ++i) {
                    uint8_t bit = (parts_bits[i >> 3] >> (i & 7u)) & 1u;
                    if (bit) {
                        bits_s[i] = '@';
                    } else {
                        bits_s[i] = '.';
                    }
                }
                bits_s[parts_total] = 0; // null-terminate
                FURI_LOG_I(TAG, "Parts: %s", bits_s);
                
                snprintf(
                    progress_text,
                    sizeof(progress_text),
                    "%s, %lu KB\n %lu%%",
                    g.r_file_name,
                    g.r_file_size / 1024,
                    state->counter);
            } else { // show "waiting for announce"
                snprintf(progress_text, sizeof(progress_text), "Waiting for announce...");
            }
        }
        // Upd
        dialog_ex_set_text(app->dialog_show_file, progress_text, 64, 32, AlignCenter, AlignCenter);
    }
}

bool flipper_share_scene_receive_on_event(void* context, SceneManagerEvent event) {
    FlipperShareApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == DialogExResultLeft) {
            // Cancel button pressed - stop reading and return to file info
            FileReadingState* state = (FileReadingState*)app->file_reading_state;
            if(state && state->worker_thread) {
                furi_thread_flags_set(furi_thread_get_id(state->worker_thread), 0x1);
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
            furi_thread_flags_set(furi_thread_get_id(state->worker_thread), 0x1);
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

void flipper_share_scene_receive_on_exit(void* context) {
    FlipperShareApp* app = context;

    if(ss_subghz_deinit()) {
        FURI_LOG_W(TAG, "ss_subghz_deinit reported error on scene exit");
    }

    // Clean up resources
    if(app->file_reading_state) {
        file_reading_state_free((FileReadingState*)app->file_reading_state);
        app->file_reading_state = NULL;
    }

    // Check if the timer is stopped
    if(app->timer) {
        furi_timer_stop(app->timer);
        furi_timer_free(app->timer);
        app->timer = NULL;
    }
}
