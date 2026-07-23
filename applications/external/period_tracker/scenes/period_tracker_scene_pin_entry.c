#include "../period_tracker.h"
#include "../period_tracker_predictions.h"
#include "../period_tracker_alert.h"
#include "../period_tracker_csv.h"
#include "../period_tracker_models.h"
#include <furi.h>

#define PIN_ENTRY_EVENT_ERROR_TIMEOUT 0xFE01u
#define PIN_ENTRY_EVENT_SUBMITTED     0xFE02u

static void error_timer_callback(void* context) {
    PeriodTrackerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, PIN_ENTRY_EVENT_ERROR_TIMEOUT);
}

static void pin_entry_done_callback(void* context, uint16_t pin) {
    PeriodTrackerApp* app = context;
    app->pin_draft = pin;
    view_dispatcher_send_custom_event(app->view_dispatcher, PIN_ENTRY_EVENT_SUBMITTED);
}

static void pin_entry_show_input(PeriodTrackerApp* app) {
    pin_input_reset(app->pin_input);
    pin_input_set_header(app->pin_input, "Enter PIN");
    pin_input_set_auto_submit(app->pin_input, true); // unlock: submit on 4th digit
    pin_input_set_result_callback(app->pin_input, pin_entry_done_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewPinInput);
}

void period_tracker_scene_pin_entry_on_enter(void* context) {
    PeriodTrackerApp* app = context;
    furi_assert(app);

    FURI_LOG_I(TAG, "PIN Entry scene: entering");

    if(!app->pin_error_timer) {
        app->pin_error_timer = furi_timer_alloc(error_timer_callback, FuriTimerTypeOnce, app);
        FURI_LOG_I(TAG, "PIN Entry: error timer allocated at %p", (void*)app->pin_error_timer);
    }

    pin_entry_show_input(app);
}

bool period_tracker_scene_pin_entry_on_event(void* context, SceneManagerEvent event) {
    PeriodTrackerApp* app = context;
    furi_assert(app);
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        FURI_LOG_I(TAG, "PIN Entry: back button pressed, stopping app");
        if(app->pin_error_timer) {
            furi_timer_stop(app->pin_error_timer);
        }
        view_dispatcher_stop(app->view_dispatcher);
        consumed = true;
    } else if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == PIN_ENTRY_EVENT_ERROR_TIMEOUT) {
            FURI_LOG_I(TAG, "PIN Entry: error timeout expired, returning to PIN entry");
            pin_entry_show_input(app);
            consumed = true;
        } else if(event.event == PIN_ENTRY_EVENT_SUBMITTED) {
            uint32_t entered_pin = app->pin_draft;

            FURI_LOG_I(TAG, "PIN Entry: validating entered PIN");
            if(entered_pin == app->pin_code) {
                FURI_LOG_I(TAG, "PIN Entry: correct PIN entered, checking for alerts");
                period_tracker_reset_pin_fails(app);

                AppSettings settings;
                app_settings_init(&settings);
                settings_load(app->storage, &settings);
                uint8_t lead_days = settings.alert_lead_days;
                if(lead_days == 0) lead_days = 1;

                Prediction* alert_predictions = malloc(sizeof(Prediction) * 20);
                if(alert_predictions) {
                    uint16_t alert_count = get_alert_predictions(
                        app->storage,
                        alert_predictions,
                        20,
                        app->prediction_buffer,
                        app->prediction_buffer_size,
                        lead_days);
                    free(alert_predictions);

                    scene_manager_next_scene(app->scene_manager, PeriodTrackerSceneMainMenu);

                    if(alert_count > 0) {
                        FURI_LOG_I(
                            TAG,
                            "PIN Entry: %u alert(s) in next %u day(s), opening Daily Digest",
                            alert_count,
                            lead_days);
                        period_tracker_alert_notify_if_today(app->storage);
                        scene_manager_next_scene(
                            app->scene_manager, PeriodTrackerSceneDailyDigest);
                    } else {
                        FURI_LOG_I(
                            TAG,
                            "PIN Entry: no alerts in next %u day(s), staying on Main Menu",
                            lead_days);
                    }
                } else {
                    FURI_LOG_E(TAG, "PIN Entry: failed to allocate memory for alert predictions");
                    scene_manager_next_scene(app->scene_manager, PeriodTrackerSceneMainMenu);
                }
                consumed = true;
            } else {
                app->pin_fails++;
                period_tracker_save_pin_fails(app, app->pin_fails);
                FURI_LOG_W(TAG, "PIN Entry: wrong PIN entered, fails=%u/4", app->pin_fails);

                if(app->pin_fails >= 4) {
                    FURI_LOG_W(TAG, "PIN Entry: maximum failures reached, locking app");
                    File* file = storage_file_alloc(app->storage);

                    if(storage_file_open(
                           file, APP_DATA_PATH("pin.locked"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
                        storage_file_write(file, "locked", 6);
                        storage_file_close(file);
                        FURI_LOG_I(TAG, "PIN Entry: pin.locked file created");
                    } else {
                        FURI_LOG_E(TAG, "PIN Entry: failed to create pin.locked file");
                    }
                    storage_file_free(file);

                    storage_simply_remove(app->storage, APP_DATA_PATH("pin.txt"));
                    storage_simply_remove(app->storage, APP_DATA_PATH("pin.fails"));
                    FURI_LOG_I(TAG, "PIN Entry: removed pin.txt and pin.fails");

                    widget_reset(app->widget);
                    widget_add_string_multiline_element(
                        app->widget,
                        64,
                        32,
                        AlignCenter,
                        AlignCenter,
                        FontPrimary,
                        "App is Locked!\n\n"
                        "Too many failed\n"
                        "login attempts.");
                    view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewWidget);

                    app->pin_locked = true;
                    app->pin_set = false;
                } else {
                    char error_msg[64];
                    snprintf(
                        error_msg,
                        sizeof(error_msg),
                        "Wrong PIN!\n\n"
                        "%d attempt%s remaining",
                        4 - app->pin_fails,
                        (4 - app->pin_fails) == 1 ? "" : "s");
                    widget_reset(app->widget);
                    widget_add_string_multiline_element(
                        app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, error_msg);
                    view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewWidget);

                    FURI_LOG_I(TAG, "PIN Entry: starting 2 second error timer");
                    furi_timer_start(app->pin_error_timer, furi_ms_to_ticks(2000));
                }
                consumed = true;
            }
        }
    }

    return consumed;
}

void period_tracker_scene_pin_entry_on_exit(void* context) {
    PeriodTrackerApp* app = context;
    furi_assert(app);

    FURI_LOG_I(TAG, "PIN Entry scene: exiting");

    if(app->pin_error_timer) {
        furi_timer_stop(app->pin_error_timer);
    }

    pin_input_reset(app->pin_input);
    widget_reset(app->widget);
}
