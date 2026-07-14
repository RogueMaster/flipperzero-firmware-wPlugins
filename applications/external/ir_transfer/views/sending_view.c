#include "sending_view.h"
#include <gui/elements.h>
#include <dialogs/dialogs.h>
#include <furi_hal.h>
#include <furi.h>
#include "../utils/draw_utils.h"
#include "../infrared/infrared_transfer.h"
#include "../ir_main.h"

static void Sending_draw_callback(Canvas* canvas, void* model) {
    SendingModel* send_model = (SendingModel*)model;
    canvas_clear(canvas);
    canvas_set_bitmap_mode(canvas, true);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 12, "IR FILE SENDER");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, 23, "file :");
    draw_truncated_text(canvas, 18, 23, furi_string_get_cstr(send_model->sending_path), 100);
    canvas_draw_str(canvas, 0, 34, "status :");
    draw_truncated_text(canvas, 35, 34, furi_string_get_cstr(send_model->status), 100);
    canvas_draw_str(canvas, 0, 44, "progress :");
    if(furi_string_cmp_str(send_model->progression, "not started") == 0) {
        canvas_draw_str(canvas, 45, 44, furi_string_get_cstr(send_model->progression));
        elements_button_right(canvas, "send");
        elements_button_left(canvas, "change file");

    } else {
        elements_progress_bar(canvas, 46, 37, 75, send_model->loading);
        elements_button_center(canvas, "cancel");
    }
}

static uint32_t sending_exit_callback(void* _context) {
    UNUSED(_context);
    return LandingView;
}

static void send_view_enter_callback(void* context) {
    App* app = (App*)context;
    FURI_LOG_I("Send_view", "entering send view before browser");

    if(furi_string_cmp_str(app->send_path, "none") == 0) {
        furi_string_set(app->send_path, "/ext");
        DialogsFileBrowserOptions browser_options;
        dialog_file_browser_set_basic_options(&browser_options, TEXT_VIEWER_EXTENSION, NULL);
        browser_options.hide_ext = false;
        DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);
        dialog_file_browser_show(
            dialogs, app->send_path, app->send_path, &browser_options); // &browser_options
        furi_record_close(RECORD_DIALOGS);
        bool redraw = true;
        with_view_model(
            app->send_view,
            SendingModel * _model,
            {
                furi_string_set(_model->sending_path, app->send_path);
                _model->loading = 0;
                furi_string_set(_model->status, "ready to send");
                furi_string_set(_model->progression, "not started");
            },
            redraw);
    }
}

static void send_view_exit_callback(void* context) {
    App* app = (App*)context;
    furi_string_set(app->send_path, "none");
    app->is_sending = false;
    if(app->send_thread) {
        if(furi_thread_get_state(app->send_thread) != FuriThreadStateStopped) {
            furi_thread_join(app->send_thread);
        }
        furi_thread_free(app->send_thread);
        app->send_thread = NULL;
    }
}

static bool send_view_custom_event_callback(uint32_t event, void* context) {
    App* app = (App*)context;
    switch(event) {
    case RedrawSendScreen: {
        bool redraw = true;
        with_view_model(app->send_view, SendingModel * _model, { UNUSED(_model); }, redraw);
        return true;
    }

    default:
        return false;
    }
}

static bool Sending_input_callback(InputEvent* event, void* context) {
    App* app = (App*)context;

    if(app->send_thread && furi_thread_get_state(app->send_thread) == FuriThreadStateStopped) {
        furi_thread_free(app->send_thread);
        app->send_thread = NULL;
        FURI_LOG_I("IRSender", "Thread terminé détecté, libéré proprement");
    }

    if(event->type == InputTypeShort) {
        if(event->key == InputKeyOk) {
            FURI_LOG_I("Send", "IR sending function");

            if(app->is_sending) {
                FURI_LOG_I("IRSender", "Demande d'arrêt du thread");
                app->is_sending = false;

                furi_delay_ms(10);

                if(app->send_thread) {
                    if(furi_thread_get_state(app->send_thread) != FuriThreadStateStopped) {
                        furi_thread_join(app->send_thread);
                        furi_thread_free(app->send_thread);
                        app->send_thread = NULL;
                    }
                }
                with_view_model(
                    app->send_view,
                    SendingModel * _model,
                    {
                        _model->loading = 0;
                        furi_string_set(_model->status, "ready to send");
                        furi_string_set(_model->progression, "not started");
                    },
                    true);

                return true;
            }

        } else if(event->key == InputKeyRight) {
            if(!app->is_sending) {
                if(!app->send_thread) {
                    FURI_LOG_I("IRSender", "Lancement du thread IRSendThread");
                    app->send_thread =
                        furi_thread_alloc_ex("IRSendThread", 1024, infrared_tx, app);
                    furi_thread_start(app->send_thread);
                } else {
                    FURI_LOG_E("IRSender", "Le thread est déjà actif !");
                }
            }
            return true;
        } else if(event->key == InputKeyLeft) {
            if(!app->is_sending) {
                DialogsFileBrowserOptions browser_options;
                dialog_file_browser_set_basic_options(
                    &browser_options, TEXT_VIEWER_EXTENSION, NULL);
                browser_options.hide_ext = false;
                DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);
                dialog_file_browser_show(
                    dialogs, app->send_path, app->send_path, &browser_options);
                furi_record_close(RECORD_DIALOGS);

                bool redraw = true;
                with_view_model(
                    app->send_view,
                    SendingModel * _model,
                    { furi_string_set(_model->sending_path, app->send_path); },
                    redraw);
            }
            return true;
        }
    }

    return false; // << ICI tu dois retourner false si aucun bouton n'a matché
}

View* sending_view_alloc(App* app) {
    View* sending_view = view_alloc();
    view_set_draw_callback(sending_view, Sending_draw_callback);
    view_set_input_callback(sending_view, Sending_input_callback);
    view_set_previous_callback(sending_view, sending_exit_callback);
    view_set_context(sending_view, app);
    view_set_enter_callback(sending_view, send_view_enter_callback);
    view_set_exit_callback(sending_view, send_view_exit_callback);
    view_set_custom_callback(sending_view, send_view_custom_event_callback);
    view_allocate_model(sending_view, ViewModelTypeLockFree, sizeof(SendingModel));
    SendingModel* sending_model = view_get_model(sending_view);
    sending_model->status = furi_string_alloc();
    sending_model->sending_path = furi_string_alloc();
    sending_model->progression = furi_string_alloc();
    furi_string_set(sending_model->sending_path, "none");
    return sending_view;
}

void sending_view_free(View* sending_view) {
    furi_assert(sending_view);
    SendingModel* sending_model = view_get_model(sending_view);
    furi_string_free(sending_model->sending_path);
    furi_string_free(sending_model->status);
    furi_string_free(sending_model->progression);
    view_free(sending_view);
}
