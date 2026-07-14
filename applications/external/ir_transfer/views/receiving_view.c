#include "receiving_view.h"
#include <gui/elements.h>
#include "../utils/draw_utils.h"
#include "../infrared/infrared_transfer.h"
#include "../ir_main.h"
#include <furi.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <input/input.h>
#include <storage/storage.h>
#include <string.h>
#include <notification/notification_messages.h>

static bool rx_in_data_phase(const ReceivingModel* m) {
    if(!m || !m->controller) return false;
    return m->controller->is_receiving;
}

static void reset_reception_state(InfraredController* c) {
    // Arrêter proprement la session en cours
    c->is_receiving = false;
    c->processing_signal = false;
    c->size_file_count = 0;
    memset(c->size_file_buffer, 0, sizeof(c->size_file_buffer));
    c->file_size = 0;
    c->bytes_received = 0;
    c->progress = 0.0f;

    // Nom de fichier
    c->file_name_index = 0;
    memset(c->file_name_buffer, 0, sizeof(c->file_name_buffer));
}

static void Receiving_draw_callback(Canvas* canvas, void* model) {
    ReceivingModel* rec_model = (ReceivingModel*)model;
    canvas_clear(canvas);
    canvas_set_bitmap_mode(canvas, true);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 12, "IR FILE RECEIVER");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, 23, "path :");
    canvas_draw_str(canvas, 28, 23, furi_string_get_cstr(rec_model->receiving_path));
    canvas_draw_str(canvas, 0, 34, "status :");
    draw_truncated_text(canvas, 35, 34, furi_string_get_cstr(rec_model->status), 98);
    canvas_draw_str(canvas, 0, 44, "progress :");
    if(furi_string_cmp_str(rec_model->progression, "not started") == 0) {
        canvas_draw_str(canvas, 45, 44, furi_string_get_cstr(rec_model->progression));
    } else {
        elements_progress_bar(canvas, 46, 37, 75, rec_model->loading);
    }

    if(rx_in_data_phase(rec_model)) {
        elements_button_center(canvas, "cancel");
    } else {
        elements_button_left(canvas, "back");
    }
}

static void rec_view_exit_callback(void* context) {
    App* app = (App*)context;
    ReceivingModel* model = view_get_model(app->rec_view);

    if(model->controller && model->controller->notifications && model->controller->led_blinking) {
        notification_message(model->controller->notifications, &sequence_blink_stop);
        model->controller->led_blinking = false;
    }

    if(model->controller) {
        infrared_controller_free(model->controller);
        model->controller = NULL;
    }
}

static bool Receiving_input_callback(InputEvent* event, void* context) {
    App* app = (App*)context;
    ReceivingModel* model = view_get_model(app->rec_view);
    const bool in_data = rx_in_data_phase(model);

    if(event->type == InputTypeShort) {
        // OK = Cancel only while receiving
        if(event->key == InputKeyOk) {
            if(!in_data) return false; // OK inactive when idle

            // Close and remove temp if any
            if(model->controller && model->controller->file) {
                storage_file_close(model->controller->file);
            }
            Storage* storage = furi_record_open(RECORD_STORAGE);
            if(storage_file_exists(storage, FILE_PATH_RECV)) {
                storage_common_remove(storage, FILE_PATH_RECV);
            }
            furi_record_close(RECORD_STORAGE);

            // Reset reception state (mutex-protected)
            if(model->controller) {
                if(furi_mutex_acquire(model->controller->state_mutex, FuriWaitForever) ==
                   FuriStatusOk) {
                    reset_reception_state(model->controller);
                    furi_mutex_release(model->controller->state_mutex);
                } else {
                    reset_reception_state(model->controller);
                }
            }

            if(model->controller && model->controller->notifications &&
               model->controller->led_blinking) {
                notification_message(model->controller->notifications, &sequence_blink_stop);
                model->controller->led_blinking = false;
            }

            // UI back to idle
            with_view_model(
                app->rec_view,
                ReceivingModel * _m,
                {
                    _m->loading = 0.0f;
                    furi_string_set(_m->status, "transfer canceled");
                    furi_string_set(_m->progression, "not started");
                    furi_string_set(_m->receiving_path, FILE_PATH_FOLDER);
                },
                true);
            return true;
        }

        // Left = Back only when idle
        if(event->key == InputKeyLeft) {
            if(in_data) return false; // Back inactive during RX
            view_dispatcher_switch_to_view(app->view_dispatcher, LandingView);
            return true;
        }
    }

    return false;
}

static void rec_view_enter_callback(void* context) {
    App* app = (App*)context;
    ReceivingModel* model = view_get_model(app->rec_view);

    FURI_LOG_I("rec_view_enter", "about to allocate infrared controller");

    // Reset UI first
    with_view_model(
        app->rec_view,
        ReceivingModel * _model,
        {
            _model->loading = 0;
            furi_string_set(_model->status, "awaiting signal");
            furi_string_set(_model->receiving_path, FILE_PATH_FOLDER);
            furi_string_set(_model->progression, "not started");
        },
        true);

    // If a previous controller somehow exists, stop its LED and free it safely
    if(model->controller) {
        if(model->controller->notifications && model->controller->led_blinking) {
            notification_message(model->controller->notifications, &sequence_blink_stop);
            model->controller->led_blinking = false;
        }
        infrared_controller_free(model->controller);
        model->controller = NULL;
    }

    // Allocate a fresh controller for this view session
    model->controller = infrared_controller_alloc(app);
}

static uint32_t receiving_exit_callback(void* _context) {
    UNUSED(_context);
    return LandingView;
}

View* receiving_view_alloc(App* app) {
    View* receiving_view = view_alloc();
    view_set_draw_callback(receiving_view, Receiving_draw_callback);
    view_set_input_callback(receiving_view, Receiving_input_callback);
    view_set_previous_callback(receiving_view, receiving_exit_callback);
    view_set_context(receiving_view, app);
    view_set_enter_callback(receiving_view, rec_view_enter_callback);
    view_set_exit_callback(receiving_view, rec_view_exit_callback);
    view_allocate_model(receiving_view, ViewModelTypeLockFree, sizeof(ReceivingModel));
    ReceivingModel* receving_model = view_get_model(receiving_view);
    receving_model->status = furi_string_alloc();
    receving_model->receiving_path = furi_string_alloc();
    receving_model->progression = furi_string_alloc();
    receving_model->controller = NULL;
    return receiving_view;
}

void receiving_view_free(View* receiving_view) {
    furi_assert(receiving_view);
    ReceivingModel* receving_model = view_get_model(receiving_view);
    furi_string_free(receving_model->status);
    furi_string_free(receving_model->receiving_path);
    furi_string_free(receving_model->progression);
    view_free(receiving_view);
}
