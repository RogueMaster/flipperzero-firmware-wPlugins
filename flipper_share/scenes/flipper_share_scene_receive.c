#include "../flipper_share_app.h"
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_power.h>
#include <stdlib.h>

#include "subghz_share.h"
#include "flipper_share.h"
#include "flipper_share_scene.h"

#include <gui/gui.h>
#include <gui/view.h>
#include <gui/elements.h>

#define FS_IDLE_OPERATION 50 //ms

#define TAG "FlipperShareSend"

typedef struct {
    uint32_t counter;
    bool reading_complete;
    FuriThread* worker_thread;
} FileReadingState;

static void update_timer_callback(void* context);
static void dialog_ex_callback(DialogExResult result, void* context);

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

// Graphical progress view (shown while locked and not finished)
static View* progress_view = NULL;
static bool progress_view_active = false;

static void progress_view_draw_callback(Canvas* canvas, void* context) {
    // model holds percent (0-100)
    uint8_t* model = (uint8_t*)context;
    uint8_t percent = model ? *model : 0;
    
    FURI_LOG_I(TAG, "Progress view draw: percent=%u", (unsigned int)percent);

    canvas_clear(canvas);

    // Header
    canvas_set_font(canvas, FontPrimary);
    canvas_set_color(canvas, ColorBlack);
    elements_multiline_text_aligned(canvas, 64, 4, AlignCenter, AlignTop, "Receiving...");

    // Filename (basename) g.r_file_name and g.r_file_size
    canvas_set_font(canvas, FontSecondary);
    char name_line[64];
    snprintf(name_line, sizeof(name_line), "%.*s, %lu KB", 48, g.r_file_name, (unsigned long)(g.r_file_size / 1024));
    elements_multiline_text_aligned(canvas, 64, 20, AlignCenter, AlignTop, name_line);

    // Show progress percent as text above bar
    snprintf(name_line, sizeof(name_line), "Progress: %u%%", (unsigned int)percent);
    elements_multiline_text_aligned(canvas, 64, 36, AlignCenter, AlignTop, name_line);

    // Progress bar frame and fill
    const int x = 13;
    const int y = 50;
    const int w = 101; // frame width
    const int h = 12;
    
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_frame(canvas, x, y, w, h);

    uint8_t parts_bits[FS_PARTS_BYTES];
    fs_parts_bitmap_copy(parts_bits);
    
    for (uint32_t i = 0; i < FS_PARTS_COUNT; ++i) {
        if ((parts_bits[i >> 3] >> (i & 7u)) & 1u) {    // bit value by number
            canvas_draw_line(canvas, x + i + 1, y, x + i + 1, y + h - 1);
        }
    }

    // Percent text below
    char pct[16];
    snprintf(pct, sizeof(pct), "%u%%", (unsigned int)percent);
    elements_multiline_text_aligned(canvas, 64, y + h + 8, AlignCenter, AlignTop, pct);
}

static bool progress_view_input_callback(InputEvent* event, void* context) {
    if(!context) return false;
    FlipperShareApp* app = context;
    
    FURI_LOG_I(TAG, "Progress view input: key=%d, type=%d", event->key, event->type);
    
    if(event->type == InputTypeShort || event->type == InputTypeLong) {
        if(event->key == InputKeyBack || event->key == InputKeyLeft) {
            FURI_LOG_I(TAG, "Back/Left button pressed in progress view, handling locally");
            
            FileReadingState* state = (FileReadingState*)app->file_reading_state;
            if(state && state->worker_thread) {
                FURI_LOG_I(TAG, "Stopping worker thread from input handler");
                furi_thread_flags_set(furi_thread_get_id(state->worker_thread), 0x1);
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
            view_dispatcher_switch_to_view(app->view_dispatcher, FlipperShareViewIdShowFile);
            
            FURI_LOG_I(TAG, "Sending DialogExResultLeft event");
            view_dispatcher_send_custom_event(app->view_dispatcher, DialogExResultLeft);
            
            return true;
        }
    }
    return false;
}

static void progress_view_init(FlipperShareApp* app) {
    if(progress_view) return;
    progress_view = view_alloc();
    view_set_context(progress_view, app);
    view_allocate_model(progress_view, ViewModelTypeLocking, sizeof(uint8_t));
    view_set_draw_callback(progress_view, progress_view_draw_callback);
    view_set_input_callback(progress_view, progress_view_input_callback);
    view_dispatcher_add_view(app->view_dispatcher, FlipperShareViewIdProgress, progress_view);
}

static void progress_view_deinit(FlipperShareApp* app) {
    if(!progress_view) return;
    view_dispatcher_remove_view(app->view_dispatcher, FlipperShareViewIdProgress);
    view_free(progress_view);
    progress_view = NULL;
    progress_view_active = false;
}

void flipper_share_scene_receive_on_enter(void* context) {
    FlipperShareApp* app = context;

    // Create state for the scene
    FileReadingState* state = file_state_alloc();
    app->file_reading_state = state;

    // Setup dialog to show progress (use same UI as send scene so buttons appear)
    dialog_ex_set_header(app->dialog_show_file, "Receiving...", 64, 10, AlignCenter, AlignCenter);
    dialog_ex_set_text(app->dialog_show_file, "Waiting for announce...", 64, 32, AlignCenter, AlignCenter);
    dialog_ex_set_left_button_text(app->dialog_show_file, "Back");
    dialog_ex_set_right_button_text(app->dialog_show_file, NULL);

    dialog_ex_set_context(app->dialog_show_file, app);
    dialog_ex_set_result_callback(app->dialog_show_file, dialog_ex_callback);

    // Start thread for reading file
    state->worker_thread =
        furi_thread_alloc_ex("FileReadWorker", 2048, file_read_worker_thread, app);
    furi_thread_start(state->worker_thread);

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperShareViewIdShowFile);

    // Start timer for updating display
    app->timer = furi_timer_alloc(update_timer_callback, FuriTimerTypePeriodic, app);
    furi_timer_start(app->timer, 250);

    ss_subghz_init(); // TODO Move to thread?
}

static void update_timer_callback(void* context) {
    furi_assert(context);
    FlipperShareApp* app = context;
    FileReadingState* state = (FileReadingState*)app->file_reading_state;
    char progress_text[256];
    
    if(state) {
        // FURI_LOG_I(
        //     TAG, 
        //     "Timer: counter=%u, complete=%d, locked=%d, finished=%d", 
        //     (unsigned int)state->counter, 
        //     state->reading_complete,
        //     g.r_locked,
        //     g.r_is_finished);
            
        if(state->reading_complete) {
            if (g.r_is_success) {
                dialog_ex_set_header(app->dialog_show_file, "Success!", 64, 10, AlignCenter, AlignCenter);
            } else {
                dialog_ex_set_header(app->dialog_show_file, "Hash failed", 64, 10, AlignCenter, AlignCenter);
            }
            snprintf(progress_text, sizeof(progress_text), "Saved to:\n%.*s", 64, g.r_file_path);
            // dialog_ex_set_right_button_text(app->dialog_show_file, "OK");
            
            // If completed and still showing progress view, switch back to dialog
            if(progress_view_active) {
                FURI_LOG_I(TAG, "Transfer complete, switching to dialog");
                view_dispatcher_switch_to_view(app->view_dispatcher, FlipperShareViewIdShowFile);
                progress_view_active = false;
            }
        } else {
            if(g.r_locked) {
                snprintf(
                    progress_text,
                    sizeof(progress_text),
                    "%.*s, %lu KB\n%u%%",
                    64,
                    g.r_file_name,
                    (unsigned long)(g.r_file_size / 1024),
                    (unsigned int)state->counter);
                    
                // If locked and not finished, show graphical progress view instead of dialog
                if(!progress_view) {
                    FURI_LOG_I(TAG, "Initializing progress view");
                    progress_view_init(app);
                }
                
                if(!progress_view_active) {
                    FURI_LOG_I(TAG, "Switching to progress view");
                    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperShareViewIdProgress);
                    progress_view_active = true;
                }
                
                // Update progress view model
                with_view_model(progress_view, uint8_t* model, {
                    *model = (uint8_t)state->counter;
                    FURI_LOG_I(TAG, "Updating progress model: %u%%", (unsigned int)state->counter);
                }, true);
            } else {
                snprintf(progress_text, sizeof(progress_text), "Waiting for announce...");
                
                // If we're no longer locked but the progress view is active, switch back to dialog
                if(progress_view_active) {
                    FURI_LOG_I(TAG, "No longer locked, switching to dialog");
                    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperShareViewIdShowFile);
                    progress_view_active = false;
                }
            }
        }
        dialog_ex_set_text(app->dialog_show_file, progress_text, 64, 32, AlignCenter, AlignCenter);
    }
}

// Callback for DialogEx buttons
static void dialog_ex_callback(DialogExResult result, void* context) {
    furi_assert(context);
    FlipperShareApp* app = context;

    if(result == DialogExResultLeft) {
        view_dispatcher_send_custom_event(app->view_dispatcher, DialogExResultLeft);
    } else if(result == DialogExResultRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, DialogExResultRight);
    }
}

bool flipper_share_scene_receive_on_event(void* context, SceneManagerEvent event) {
    FlipperShareApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == DialogExResultLeft) {
            // Back button pressed - stop reading and return to file info
            FileReadingState* state = (FileReadingState*)app->file_reading_state;
            if(state && state->worker_thread) {
                FURI_LOG_I(TAG, "Stopping worker thread");
                furi_thread_flags_set(furi_thread_get_id(state->worker_thread), 0x1);
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
    // Ensure progress view is deinitialized if it was created
    progress_view_deinit(app);

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
