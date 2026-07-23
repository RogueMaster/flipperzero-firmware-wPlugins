#include "../ir_share_app.h"
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_power.h>
#include <stdlib.h>

#include "ir_transport.h"
#include "ir_share.h"
#include "ir_share_scene.h"

#include <gui/gui.h>
#include <gui/view.h>
#include <gui/elements.h>

#define ISH_IDLE_OPERATION 50 //ms

#define TAG "IrShareSend"

typedef struct {
    uint32_t counter;
    bool reading_complete;
    FuriThread* worker_thread;
} FileReadingState;

static void update_timer_callback(void* context);
static void dialog_ex_callback(DialogExResult result, void* context);

static FileReadingState* file_state_alloc() {
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
    IrShareApp* app = context;
    FileReadingState* state = (FileReadingState*)app->file_reading_state;

    bool is_running = true;

    FURI_LOG_I(
        TAG,
        "file_read_worker_thread: file: %s, size: %zu bytes",
        app->selected_file_path,
        app->selected_file_size);

    ish_init_from_external_receive();

    while(is_running) {
        ish_idle();
        furi_delay_ms(ISH_IDLE_OPERATION);

        // Snapshot progress under the lock; guard against division by zero
        // (r_blocks_needed == 0 before the first ANNOUNCE is handled).
        ish_lock();
        uint32_t received = g.r_blocks_received;
        uint32_t needed = g.r_blocks_needed;
        bool finished = g.r_is_finished;
        ish_unlock();

        state->counter = needed ? (received * 100) / needed : 0;
        if(finished) {
            state->reading_complete = true;
        }

        // Check if we should stop
        if(furi_thread_flags_get() & ISH_WORKER_STOP_FLAG) {
            is_running = false;
        }
    }

    state->reading_complete = true;

    return 0;
}

// Graphical progress view (shown while locked and not finished)
static View* progress_view = NULL;
static bool progress_view_active = false;

static void progress_view_draw_callback(Canvas* canvas, void* context) {
    // model holds percent (0-100)
    uint8_t* model = (uint8_t*)context;
    uint8_t percent = model ? *model : 0;

    // Snapshot shared state under the lock (the RX thread mutates r_file_* and
    // ish_parts concurrently). Skip on contention rather than block the renderer.
    char fname[ISH_FILENAME_LENGTH];
    uint32_t fsize = 0;
    uint32_t rcv = 0, need = 0, start = 0, last_progress = 0;
    bool finalizing = false;
    uint8_t levels[ISH_PARTS_COUNT];
    if(ish_try_lock_ms(10)) {
        memcpy(fname, g.r_file_name, sizeof(fname));
        fname[sizeof(fname) - 1] = '\0';
        fsize = g.r_file_size;
        rcv = g.r_blocks_received;
        need = g.r_blocks_needed;
        start = g.r_start_ms;
        last_progress = g.r_last_progress_ms;
        finalizing = g.r_finalizing;
        ish_parts_levels_copy(levels);
        ish_unlock();
    } else {
        fname[0] = '\0';
        memset(levels, 0, sizeof(levels));
    }

    canvas_clear(canvas);

    // Header
    canvas_set_font(canvas, FontPrimary);
    canvas_set_color(canvas, ColorBlack);
    elements_multiline_text_aligned(
        canvas, 64, SCENE_HEADER_POSITION_Y, AlignCenter, AlignTop, "Receiving via IR...");

    // Filename on its own line (as-is; long names may overflow — accepted).
    canvas_set_font(canvas, FontSecondary);
    elements_multiline_text_aligned(canvas, 64, 20, AlignCenter, AlignTop, fname);

    // Size + percent + ETA on one line above the bar.
    // ETA = remaining / rate, where rate is the measured session average once
    // enough has elapsed; before that (warmup) it uses the nominal constant.
    // If no new block has arrived for ISH_STALL_MS, show "stalled" instead of a
    // number (this also avoids ETA blowing up / overflowing when recv is small
    // and elapsed keeps growing during a link outage).
    uint32_t rem_blocks = (need > rcv) ? (need - rcv) : 0;
    uint32_t rem_bytes = rem_blocks * ISH_DATA_LENGTH;
    uint32_t recv_bytes = rcv * ISH_DATA_LENGTH;
    uint32_t now = furi_get_tick();
    uint32_t elapsed_ms = (now > start) ? (now - start) : 0;

    bool stalled = (rem_blocks > 0) && (last_progress != 0) &&
                   ((now - last_progress) > ISH_STALL_MS);

    char info[48];
    if(finalizing) {
        // All blocks are in; the engine runs a chunked MD5 over the written file
        uint32_t hash_done, hash_total;
        if(ish_hash_progress_get(&hash_done, &hash_total) && hash_total > 0) {
            snprintf(
                info,
                sizeof(info),
                "Verifying... %lu%%",
                (unsigned long)(((uint64_t)hash_done * 100u) / hash_total));
        } else {
            snprintf(info, sizeof(info), "Verifying...");
        }
    } else if(stalled) {
        snprintf(
            info,
            sizeof(info),
            "%u%%  %lu KB  stalled",
            (unsigned int)percent,
            (unsigned long)(fsize / 1024));
    } else {
        // Clamp in uint64 before casting to guard against overflow.
        uint64_t e = (elapsed_ms >= ISH_ETA_WARMUP_MS && recv_bytes > 0) ?
                         ((uint64_t)rem_bytes * elapsed_ms / ((uint64_t)recv_bytes * 1000u)) :
                         ((uint64_t)rem_bytes / ISH_PAYLOAD_THROUGHPUT_BPS);
        if(e > ISH_ETA_MAX_SEC) e = ISH_ETA_MAX_SEC;
        char eta[16];
        ish_fmt_duration((uint32_t)e, eta, sizeof(eta));
        snprintf(
            info,
            sizeof(info),
            "%u%%  %lu KB  ETA %s",
            (unsigned int)percent,
            (unsigned long)(fsize / 1024),
            eta);
    }
    elements_multiline_text_aligned(canvas, 64, 36, AlignCenter, AlignTop, info);

    // Progress bar frame and fill
    const int x = 13;
    const int y = 50;
    const int w = 101; // frame width
    const int h = 12;

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_frame(canvas, x, y, w, h);

    // Torrent-style, but each column is filled from the bottom to a height
    // proportional to the fraction of blocks received in that part (levels[i] is
    // 0..255). Any received part shows at least 1px so early progress is visible.
    for(uint32_t i = 0; i < ISH_PARTS_COUNT; ++i) {
        uint8_t lv = levels[i];
        if(!lv) continue;
        int fill = (lv * h) / 255;
        if(fill < 1) fill = 1;
        if(fill > h) fill = h;
        canvas_draw_line(canvas, x + i + 1, y + h - fill, x + i + 1, y + h - 1);
    }
    // (nothing drawn below the bar: y+h ≈ 62, screen is 64px tall)
}

static bool progress_view_input_callback(InputEvent* event, void* context) {
    if(!context) return false;
    IrShareApp* app = context;

    FURI_LOG_I(TAG, "Progress view input: key=%d, type=%d", event->key, event->type);

    if(event->type == InputTypeShort || event->type == InputTypeLong) {
        if(event->key == InputKeyBack || event->key == InputKeyLeft) {
            FURI_LOG_I(TAG, "Back/Left button pressed in progress view, handling locally");

            FileReadingState* state = (FileReadingState*)app->file_reading_state;
            if(state && state->worker_thread) {
                FURI_LOG_I(TAG, "Stopping worker thread from input handler");
                furi_thread_flags_set(
                    furi_thread_get_id(state->worker_thread), ISH_WORKER_STOP_FLAG);
                furi_thread_join(state->worker_thread);
            }

            if(app->timer) {
                FURI_LOG_I(TAG, "Stopping timer from input handler");
                furi_timer_stop(app->timer);
                furi_timer_free(app->timer);
                app->timer = NULL;
            }

            progress_view_active = false;

            FURI_LOG_I(TAG, "Switching to dialog view");
            view_dispatcher_switch_to_view(app->view_dispatcher, IrShareViewIdShowFile);

            FURI_LOG_I(TAG, "Sending DialogExResultLeft event");
            view_dispatcher_send_custom_event(app->view_dispatcher, DialogExResultLeft);

            return true;
        }
    }
    return false;
}

static void progress_view_init(IrShareApp* app) {
    if(progress_view) return;
    progress_view = view_alloc();
    view_set_context(progress_view, app);
    view_allocate_model(progress_view, ViewModelTypeLocking, sizeof(uint8_t));
    view_set_draw_callback(progress_view, progress_view_draw_callback);
    view_set_input_callback(progress_view, progress_view_input_callback);
    view_dispatcher_add_view(app->view_dispatcher, IrShareViewIdProgress, progress_view);
}

static void progress_view_deinit(IrShareApp* app) {
    if(!progress_view) return;
    view_dispatcher_remove_view(app->view_dispatcher, IrShareViewIdProgress);
    view_free(progress_view);
    progress_view = NULL;
    progress_view_active = false;
}

void ir_share_scene_receive_on_enter(void* context) {
    IrShareApp* app = context;

    // Create the shared-state lock BEFORE starting the worker thread and the
    // SubGhz RX worker, so both threads see a valid mutex from their first tick.
    ish_lock_ensure();

    // Create state for the scene
    FileReadingState* state = file_state_alloc();
    if(!state) {
        FURI_LOG_E(TAG, "receive_on_enter: out of memory");
        return;
    }
    app->file_reading_state = state;

    // Setup dialog to show progress (use same UI as send scene so buttons appear)
    dialog_ex_set_header(
        app->dialog_show_file,
        "Receiving via IR...",
        64,
        SCENE_HEADER_POSITION_Y,
        AlignCenter,
        AlignTop);
    dialog_ex_set_text(
        app->dialog_show_file, "Waiting for announce...", 64, 32, AlignCenter, AlignCenter);
    dialog_ex_set_left_button_text(app->dialog_show_file, "Back");
    dialog_ex_set_right_button_text(app->dialog_show_file, NULL);

    dialog_ex_set_context(app->dialog_show_file, app);
    dialog_ex_set_result_callback(app->dialog_show_file, dialog_ex_callback);

    // Start thread for reading file
    state->worker_thread =
        furi_thread_alloc_ex("FileReadWorker", 2048, file_read_worker_thread, app);
    furi_thread_start(state->worker_thread);

    view_dispatcher_switch_to_view(app->view_dispatcher, IrShareViewIdShowFile);

    // Start timer for updating display
    app->timer = furi_timer_alloc(update_timer_callback, FuriTimerTypePeriodic, app);
    furi_timer_start(app->timer, SCENE_UI_UPDATE_PERIOD_MS);

    ir_transport_init(); // TODO Move to thread?
}

static void update_timer_callback(void* context) {
    furi_assert(context);
    IrShareApp* app = context;
    FileReadingState* state = (FileReadingState*)app->file_reading_state;
    if(!state) return;

    // IMPORTANT: this runs on the FreeRTOS timer daemon, whose stack is only
    // ~1KB (configTIMER_TASK_STACK_DEPTH). Keep stack use minimal — ONE buffer,
    // formatted directly from `g` under the lock (no extra path/name copies).
    char progress_text[192];
    bool complete = state->reading_complete;
    bool is_success = false;
    bool is_locked = false;
    bool is_finalizing = false;

    if(!ish_try_lock_ms(20)) return; // skip this tick on contention
    if(complete) {
        is_success = g.r_is_success;
        // Actual receive throughput + elapsed time (r_start_ms..r_finish_ms).
        uint32_t st = g.r_start_ms, fin = g.r_finish_ms, fsz = g.r_file_size;
        uint32_t el_ms = (fin > st) ? (fin - st) : 0;
        uint32_t bps = (el_ms > 0) ? (uint32_t)((uint64_t)fsz * 1000u / el_ms) : 0;
        char tbuf[16];
        ish_fmt_duration(el_ms / 1000u, tbuf, sizeof(tbuf));
        snprintf(
            progress_text,
            sizeof(progress_text),
            "Saved to:\n%.*s\n%lu Bps  %s",
            72,
            g.r_file_path,
            (unsigned long)bps,
            tbuf);
    } else {
        is_locked = g.r_locked;
        // finalization drops r_locked, but the progress view must stay up
        // while the engine verifies the file hash
        is_finalizing = g.r_finalizing;
        if(is_locked || is_finalizing) {
            snprintf(
                progress_text,
                sizeof(progress_text),
                "%.*s, %lu KB\n%u%%",
                64,
                g.r_file_name,
                (unsigned long)(g.r_file_size / 1024),
                (unsigned int)state->counter);
        }
    }
    ish_unlock();

    if(complete) {
        dialog_ex_set_header(
            app->dialog_show_file,
            is_success ? "Success!" : "Hash failed",
            64,
            SCENE_HEADER_POSITION_Y,
            AlignCenter,
            AlignTop);

        // If completed and still showing progress view, switch back to dialog
        if(progress_view_active) {
            view_dispatcher_switch_to_view(app->view_dispatcher, IrShareViewIdShowFile);
            progress_view_active = false;
        }
    } else if(is_locked || is_finalizing) {
        // If locked and not finished, show graphical progress view instead of dialog
        if(!progress_view) {
            progress_view_init(app);
        }
        if(!progress_view_active) {
            view_dispatcher_switch_to_view(app->view_dispatcher, IrShareViewIdProgress);
            progress_view_active = true;
        }

        // Update progress view model
        with_view_model(
            progress_view, uint8_t * model, { *model = (uint8_t)state->counter; }, true);
    } else {
        snprintf(progress_text, sizeof(progress_text), "Waiting for announce...");

        // If we're no longer locked but the progress view is active, switch back to dialog
        if(progress_view_active) {
            view_dispatcher_switch_to_view(app->view_dispatcher, IrShareViewIdShowFile);
            progress_view_active = false;
        }
    }
    dialog_ex_set_text(app->dialog_show_file, progress_text, 64, 32, AlignCenter, AlignCenter);
}

// Callback for DialogEx buttons
static void dialog_ex_callback(DialogExResult result, void* context) {
    furi_assert(context);
    IrShareApp* app = context;

    if(result == DialogExResultLeft) {
        view_dispatcher_send_custom_event(app->view_dispatcher, DialogExResultLeft);
    } else if(result == DialogExResultRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, DialogExResultRight);
    }
}

bool ir_share_scene_receive_on_event(void* context, SceneManagerEvent event) {
    IrShareApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == DialogExResultLeft) {
            // Back button pressed - stop reading and return to file info
            FileReadingState* state = (FileReadingState*)app->file_reading_state;
            if(state && state->worker_thread) {
                FURI_LOG_I(TAG, "Stopping worker thread");
                furi_thread_flags_set(
                    furi_thread_get_id(state->worker_thread), ISH_WORKER_STOP_FLAG);
                furi_thread_join(state->worker_thread);
            }

            // Stop timer
            if(app->timer) {
                FURI_LOG_I(TAG, "Stopping timer");
                furi_timer_stop(app->timer);
                furi_timer_free(app->timer);
                app->timer = NULL;
            }

            FURI_LOG_I(TAG, "Returning to previous scene");
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
        } else if(event.event == DialogExResultRight) {
            FURI_LOG_I(TAG, "Receive scene: DialogExResultRight (OK) received");
            // OK button pressed - return to file info
            // Only available when completed

            // Stop timer
            if(app->timer) {
                FURI_LOG_I(TAG, "Stopping timer");
                furi_timer_stop(app->timer);
                furi_timer_free(app->timer);
                app->timer = NULL;
            }

            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        // Back button - same as Cancel
        FURI_LOG_I(TAG, "Receive scene: Back button event received");
        FileReadingState* state = (FileReadingState*)app->file_reading_state;
        if(state && state->worker_thread) {
            FURI_LOG_I(TAG, "Stopping worker thread");
            furi_thread_flags_set(furi_thread_get_id(state->worker_thread), ISH_WORKER_STOP_FLAG);
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

void ir_share_scene_receive_on_exit(void* context) {
    IrShareApp* app = context;
    // Ensure progress view is deinitialized if it was created
    progress_view_deinit(app);

    ir_transport_deinit();

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

    // Worker thread is joined in on_event and the SubGhz RX worker is stopped
    // above, so no thread touches `g` anymore: free the block map/parts and the
    // shared-state lock.
    ish_deinit();
}
