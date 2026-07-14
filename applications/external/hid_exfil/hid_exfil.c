/* hid_exfil.c — Main application, UI, and entry point for HID Exfil.
 * Provides payload selection, target config, execution control, and data viewer. */

#include "hid_exfil.h"
#include "hid_exfil_worker.h"
#include "hid_exfil_payloads.h"
#include <furi_hal_usb_hid.h>
#include <stdio.h>

#define TAG "HidExfil"

/* ---- Label arrays (declared extern in hid_exfil.h) ---- */

const char* const payload_labels[] = {
    "WiFi Passwords",
    "Env Variables",
    "Clipboard",
    "System Info",
    "SSH Keys",
    "Browser Bookmarks",
    "Custom Script",
};

const char* const target_os_labels[] = {
    "Windows",
    "Linux",
    "Mac",
};

const char* const phase_labels[] = {
    "Idle",
    "Injecting",
    "Syncing",
    "Receiving",
    "Cleanup",
    "Done",
    "Error",
};

/* ========================================================================
 * Execution View (Custom View) - Shows live progress
 * ======================================================================== */

typedef struct {
    ExfilState state;
    PayloadType payload_type;
} ExecutionViewModel;

static void execution_view_draw_callback(Canvas* canvas, void* model) {
    ExecutionViewModel* m = model;
    ExfilState* s = &m->state;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 12, "HID Exfil");

    /* Payload label */
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 50, 12, hid_exfil_get_payload_label(m->payload_type));

    /* Phase */
    canvas_set_font(canvas, FontSecondary);
    char line[64];
    snprintf(
        line,
        sizeof(line),
        "Phase: %s%s",
        (s->phase < PHASE_LABELS_COUNT) ? phase_labels[s->phase] : "???",
        s->paused ? " [PAUSED]" : "");
    canvas_draw_str(canvas, 2, 24, line);

    /* Bytes received */
    if(s->bytes_received >= 1024) {
        snprintf(
            line,
            sizeof(line),
            "Received: %lu.%lu KB",
            (unsigned long)(s->bytes_received / 1024),
            (unsigned long)((s->bytes_received % 1024) * 10 / 1024));
    } else {
        snprintf(line, sizeof(line), "Received: %lu B", (unsigned long)s->bytes_received);
    }
    canvas_draw_str(canvas, 2, 34, line);

    /* Elapsed time and rate */
    uint32_t elapsed_ticks = furi_get_tick() - s->start_tick;
    uint32_t elapsed_sec = elapsed_ticks / furi_kernel_get_tick_frequency();
    if(elapsed_sec == 0) elapsed_sec = 1;
    uint32_t rate = s->bytes_received / elapsed_sec;

    snprintf(
        line,
        sizeof(line),
        "Time: %lus  Rate: %lu B/s",
        (unsigned long)elapsed_sec,
        (unsigned long)rate);
    canvas_draw_str(canvas, 2, 44, line);

    /* LED state indicators or firmware warning */
    if(s->led_channel_broken) {
        canvas_draw_str(canvas, 2, 54, "! FW bug: LED read broken");
    } else {
        snprintf(
            line,
            sizeof(line),
            "[N:%s] [C:%s] [S:%s]",
            s->led_num ? "ON" : "--",
            s->led_caps ? "ON" : "--",
            s->led_scroll ? "ON" : "--");
        canvas_draw_str(canvas, 2, 54, line);
    }

    /* Controls hint */
    canvas_set_font(canvas, FontSecondary);
    if(s->phase == PhaseDone || s->phase == PhaseError) {
        canvas_draw_str(canvas, 2, 64, "OK=View Data  Back=Exit");
    } else {
        canvas_draw_str(canvas, 2, 64, "OK=Pause  Back=Abort");
    }
}

static bool execution_view_input_callback(InputEvent* event, void* context) {
    HidExfilApp* app = context;
    furi_assert(app);

    if(event->type != InputTypeShort) return false;

    ExfilState state = hid_exfil_worker_get_state(app->worker);

    if(event->key == InputKeyOk) {
        if(state.phase == PhaseDone) {
            /* Switch to data viewer */
            uint32_t data_len = 0;
            uint8_t* data = hid_exfil_worker_get_data(app->worker, &data_len);

            furi_string_reset(app->data_text);
            if(data_len > 0) {
                /* Copy data as string, null-terminate */
                for(uint32_t i = 0; i < data_len; i++) {
                    char c = (char)data[i];
                    if(c == '\0') c = '.';
                    furi_string_push_back(app->data_text, c);
                }
            } else {
                furi_string_set(app->data_text, "[No data received]");
            }
            text_box_set_text(app->data_viewer, furi_string_get_cstr(app->data_text));
            view_dispatcher_switch_to_view(app->view_dispatcher, HidExfilViewDataViewer);
            return true;
        } else if(state.phase == PhaseError) {
            /* Go back to payload select */
            view_dispatcher_switch_to_view(app->view_dispatcher, HidExfilViewPayloadSelect);
            return true;
        } else {
            /* Toggle pause */
            hid_exfil_worker_toggle_pause(app->worker);
            return true;
        }
    }

    if(event->key == InputKeyBack) {
        if(state.phase == PhaseDone || state.phase == PhaseError) {
            /* Restore USB and go back */
            if(app->usb_prev) {
                furi_hal_usb_set_config(app->usb_prev, NULL);
                app->usb_prev = NULL;
            }
            view_dispatcher_switch_to_view(app->view_dispatcher, HidExfilViewPayloadSelect);
            return true;
        } else {
            /* Abort */
            hid_exfil_worker_abort(app->worker);
            return true;
        }
    }

    return false;
}

/* Timer callback removed — worker callback handles view updates directly */

/* ========================================================================
 * Worker Callback - called from worker thread
 * ======================================================================== */

static void worker_callback(ExfilPhase phase, uint32_t bytes_received, void* context) {
    UNUSED(bytes_received);
    HidExfilApp* app = context;
    if(!app) return;

    /* Update the execution view model */
    ExfilState state = hid_exfil_worker_get_state(app->worker);

    with_view_model(
        app->execution_view, ExecutionViewModel * model, { model->state = state; }, true);

    FURI_LOG_I(
        TAG,
        "Worker phase: %s, bytes: %lu",
        (phase < PHASE_LABELS_COUNT) ? phase_labels[phase] : "???",
        (unsigned long)bytes_received);
}

/* ========================================================================
 * View navigation callbacks
 * ======================================================================== */

static uint32_t navigation_exit_callback(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

static bool back_event_callback(void* context) {
    UNUSED(context);
    return false; /* Let the view dispatcher handle the back event normally */
}

static uint32_t navigation_payload_select_callback(void* context) {
    UNUSED(context);
    return HidExfilViewPayloadSelect;
}

/* navigation_config_callback removed — unused */

/* ========================================================================
 * Warning View - "FOR AUTHORIZED TESTING ONLY"
 * ======================================================================== */

static void warning_ok_callback(GuiButtonType result, InputType type, void* context) {
    UNUSED(result);
    HidExfilApp* app = context;
    if(type == InputTypeShort) {
        view_dispatcher_switch_to_view(app->view_dispatcher, HidExfilViewPayloadSelect);
    }
}

/* ========================================================================
 * Payload Select (Submenu)
 * ======================================================================== */

static void payload_select_callback(void* context, uint32_t index) {
    HidExfilApp* app = context;
    app->selected_payload = (PayloadType)index;
    view_dispatcher_switch_to_view(app->view_dispatcher, HidExfilViewConfig);
}

/* ========================================================================
 * Config (VariableItemList)
 * ======================================================================== */

static void config_target_os_changed(VariableItem* item) {
    HidExfilApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    if(idx >= TargetOSCOUNT) idx = 0;
    app->config.target_os = (TargetOS)idx;
    variable_item_set_current_value_text(item, target_os_labels[idx]);
}

static const char* speed_labels[] = {"5ms", "10ms", "20ms", "50ms", "100ms"};
static const uint32_t speed_values[] = {5, 10, 20, 50, 100};
#define SPEED_COUNT 5

static void config_speed_changed(VariableItem* item) {
    HidExfilApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    if(idx >= SPEED_COUNT) idx = 1;
    app->config.injection_speed_ms = speed_values[idx];
    variable_item_set_current_value_text(item, speed_labels[idx]);
}

static const char* bool_labels[] = {"OFF", "ON"};

static void config_cleanup_changed(VariableItem* item) {
    HidExfilApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->config.cleanup_enabled = (idx == 1);
    variable_item_set_current_value_text(item, bool_labels[idx]);
}

/* "Run" item in config - starts execution */
static void config_enter_callback(void* context, uint32_t index) {
    HidExfilApp* app = context;

    /* Index 3 = "Start Exfiltration" button */
    if(index == 3) {
        /* Set up USB HID — only save the original config on the first run.
         * If the user went Execution→DataViewer→Back without restoring USB,
         * usb_prev still holds the pre-HID config and must not be overwritten
         * with the current (HID) config, or the original is lost forever. */
        if(!app->usb_prev) {
            app->usb_prev = furi_hal_usb_get_config();
        }
        furi_hal_usb_unlock();
        furi_hal_usb_set_config(&usb_hid, NULL);
        {
            uint32_t t = furi_get_tick();
            while(!furi_hal_hid_is_connected() && (furi_get_tick() - t) < furi_ms_to_ticks(5000)) {
                furi_delay_ms(50);
            }
            if(furi_hal_hid_is_connected()) furi_delay_ms(1500);
        }

        /* Configure and start worker */
        hid_exfil_worker_configure(app->worker, app->selected_payload, &app->config);
        hid_exfil_worker_set_callback(app->worker, worker_callback, app);

        /* Reset execution view model */
        with_view_model(
            app->execution_view,
            ExecutionViewModel * model,
            {
                memset(&model->state, 0, sizeof(ExfilState));
                model->state.phase = PhaseIdle;
                model->payload_type = app->selected_payload;
            },
            true);

        if(!hid_exfil_worker_start(app->worker)) {
            /* Out of memory — restore USB and stay on config screen. */
            FURI_LOG_E(TAG, "Worker start failed: OOM");
            furi_hal_usb_set_config(app->usb_prev, NULL);
            app->usb_prev = NULL;
            return;
        }

        view_dispatcher_switch_to_view(app->view_dispatcher, HidExfilViewExecution);
    }
}

/* ========================================================================
 * App Alloc / Free
 * ======================================================================== */

static HidExfilApp* hid_exfil_app_alloc(void) {
    HidExfilApp* app = malloc(sizeof(HidExfilApp));
    furi_assert(app);
    memset(app, 0, sizeof(HidExfilApp));

    /* Default config */
    app->config.target_os = TargetOSWindows;
    app->config.injection_speed_ms = HID_EXFIL_DEFAULT_INJECT_SPEED_MS;
    app->config.cleanup_enabled = true;
    app->selected_payload = PayloadTypeWiFiPasswords;

    /* Data text for viewer */
    app->data_text = furi_string_alloc();

    /* Open records */
    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    /* View Dispatcher */
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, back_event_callback);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    /* ---- Warning View ---- */
    app->warning = widget_alloc();
    widget_add_string_multiline_element(
        app->warning, 64, 8, AlignCenter, AlignTop, FontPrimary, "WARNING");
    widget_add_string_multiline_element(
        app->warning,
        64,
        22,
        AlignCenter,
        AlignTop,
        FontSecondary,
        "FOR AUTHORIZED\nTESTING ONLY\n\nUnauthorized use is\nillegal and unethical.");
    widget_add_button_element(app->warning, GuiButtonTypeRight, "OK", warning_ok_callback, app);
    view_set_previous_callback(widget_get_view(app->warning), navigation_exit_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, HidExfilViewWarning, widget_get_view(app->warning));

    /* ---- Payload Select (Submenu) ---- */
    app->payload_select = submenu_alloc();
    submenu_set_header(app->payload_select, "Select Payload");
    for(int i = 0; i < PayloadTypeCOUNT; i++) {
        submenu_add_item(app->payload_select, payload_labels[i], i, payload_select_callback, app);
    }
    view_set_previous_callback(submenu_get_view(app->payload_select), navigation_exit_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, HidExfilViewPayloadSelect, submenu_get_view(app->payload_select));

    /* ---- Config (VariableItemList) ---- */
    app->config_list = variable_item_list_alloc();
    variable_item_list_set_enter_callback(app->config_list, config_enter_callback, app);

    /* Target OS */
    VariableItem* item_os = variable_item_list_add(
        app->config_list, "Target OS", TargetOSCOUNT, config_target_os_changed, app);
    variable_item_set_current_value_index(item_os, app->config.target_os);
    variable_item_set_current_value_text(item_os, target_os_labels[app->config.target_os]);

    /* Injection Speed */
    VariableItem* item_speed = variable_item_list_add(
        app->config_list, "Inject Speed", SPEED_COUNT, config_speed_changed, app);
    variable_item_set_current_value_index(item_speed, 1); /* default 10ms */
    variable_item_set_current_value_text(item_speed, speed_labels[1]);

    /* Cleanup */
    VariableItem* item_cleanup =
        variable_item_list_add(app->config_list, "Cleanup", 2, config_cleanup_changed, app);
    variable_item_set_current_value_index(item_cleanup, app->config.cleanup_enabled ? 1 : 0);
    variable_item_set_current_value_text(
        item_cleanup, bool_labels[app->config.cleanup_enabled ? 1 : 0]);

    /* Start button (no value change, activated via enter callback) */
    variable_item_list_add(app->config_list, ">> Start Exfiltration <<", 0, NULL, app);

    view_set_previous_callback(
        variable_item_list_get_view(app->config_list), navigation_payload_select_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, HidExfilViewConfig, variable_item_list_get_view(app->config_list));

    /* ---- Execution View (Custom) ---- */
    app->execution_view = view_alloc();
    view_allocate_model(app->execution_view, ViewModelTypeLocking, sizeof(ExecutionViewModel));
    view_set_draw_callback(app->execution_view, execution_view_draw_callback);
    view_set_input_callback(app->execution_view, execution_view_input_callback);
    view_set_context(app->execution_view, app);
    view_set_previous_callback(app->execution_view, navigation_payload_select_callback);
    view_dispatcher_add_view(app->view_dispatcher, HidExfilViewExecution, app->execution_view);

    /* ---- Data Viewer (TextBox) ---- */
    app->data_viewer = text_box_alloc();
    text_box_set_font(app->data_viewer, TextBoxFontText);
    view_set_previous_callback(
        text_box_get_view(app->data_viewer), navigation_payload_select_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, HidExfilViewDataViewer, text_box_get_view(app->data_viewer));

    /* ---- Worker ---- */
    app->worker = hid_exfil_worker_alloc();
    furi_assert(app->worker);

    return app;
}

static void hid_exfil_app_free(HidExfilApp* app) {
    furi_assert(app);

    /* Stop worker if running */
    if(hid_exfil_worker_is_running(app->worker)) {
        hid_exfil_worker_stop(app->worker);
    }
    hid_exfil_worker_free(app->worker);

    /* Restore USB if we changed it */
    if(app->usb_prev) {
        furi_hal_usb_set_config(app->usb_prev, NULL);
        app->usb_prev = NULL;
    }

    /* Remove views */
    view_dispatcher_remove_view(app->view_dispatcher, HidExfilViewWarning);
    view_dispatcher_remove_view(app->view_dispatcher, HidExfilViewPayloadSelect);
    view_dispatcher_remove_view(app->view_dispatcher, HidExfilViewConfig);
    view_dispatcher_remove_view(app->view_dispatcher, HidExfilViewExecution);
    view_dispatcher_remove_view(app->view_dispatcher, HidExfilViewDataViewer);

    /* Free views */
    widget_free(app->warning);
    submenu_free(app->payload_select);
    variable_item_list_free(app->config_list);
    view_free(app->execution_view);
    text_box_free(app->data_viewer);

    /* Free view dispatcher */
    view_dispatcher_free(app->view_dispatcher);

    /* Close records */
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);

    /* Free data */
    furi_string_free(app->data_text);
    free(app);
}

/* ========================================================================
 * Entry Point
 * ======================================================================== */

int32_t hid_exfil_app(void* p) {
    UNUSED(p);

    HidExfilApp* app = hid_exfil_app_alloc();

    /* Start with the authorization warning */
    view_dispatcher_switch_to_view(app->view_dispatcher, HidExfilViewWarning);

    /* Run the ViewDispatcher (blocks until exit) */
    view_dispatcher_run(app->view_dispatcher);

    hid_exfil_app_free(app);
    return 0;
}
