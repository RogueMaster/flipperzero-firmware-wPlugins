#include "../period_tracker.h"
#include "../period_tracker_predictions.h"
#include "../period_tracker_alert.h"
#include <furi.h>

#define PIN_ENTRY_EVENT_BACK          0xFFFFFFFF
#define PIN_ENTRY_EVENT_ERROR_TIMEOUT 0xFFFFFFFE

static void error_timer_callback(void* context) {
    PeriodTrackerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, PIN_ENTRY_EVENT_ERROR_TIMEOUT);
}

static void period_tracker_scene_pin_entry_number_input_callback(void* context, int32_t number) {
    PeriodTrackerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, (uint32_t)number);
}

void period_tracker_scene_pin_entry_on_enter(void* context) {
    PeriodTrackerApp* app = context;
    furi_assert(app);

    FURI_LOG_I(TAG, "PIN Entry scene: entering");

    // Create error timer if it doesn't exist
    if(!app->pin_error_timer) {
        app->pin_error_timer = furi_timer_alloc(error_timer_callback, FuriTimerTypeOnce, app);
        FURI_LOG_I(TAG, "PIN Entry: error timer allocated at %p", (void*)app->pin_error_timer);
    } else {
        FURI_LOG_I(
            TAG, "PIN Entry: reusing existing error timer at %p", (void*)app->pin_error_timer);
    }

    // Use NumberInput for PIN entry
    number_input_set_header_text(app->number_input, "Enter PIN:");
    number_input_set_result_callback(
        app->number_input,
        period_tracker_scene_pin_entry_number_input_callback,
        app,
        0, // Start at 0
        0, // Min value
        9999); // Max value (4 digits)

    view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewNumberInput);
}

bool period_tracker_scene_pin_entry_on_event(void* context, SceneManagerEvent event) {
    PeriodTrackerApp* app = context;
    furi_assert(app);
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        FURI_LOG_I(TAG, "PIN Entry: back button pressed, stopping app");
        // Stop timer if running
        if(app->pin_error_timer) {
            furi_timer_stop(app->pin_error_timer);
            FURI_LOG_I(TAG, "PIN Entry: stopped error timer");
        }
        // Allow exiting the app by pressing back
        view_dispatcher_stop(app->view_dispatcher);
        consumed = true;
    } else if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == PIN_ENTRY_EVENT_ERROR_TIMEOUT) {
            FURI_LOG_I(TAG, "PIN Entry: error timeout expired, returning to PIN entry");
            // Timer expired - re-show the PIN input without touching the scene
            // stack (re-pushing this scene would grow the stack on every retry)
            number_input_set_header_text(app->number_input, "Enter PIN:");
            number_input_set_result_callback(
                app->number_input,
                period_tracker_scene_pin_entry_number_input_callback,
                app,
                0,
                0,
                9999);
            view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewNumberInput);
            consumed = true;
        } else {
            // Validate PIN - event.event contains the entered number
            uint32_t entered_pin = event.event;

            FURI_LOG_I(TAG, "PIN Entry: validating entered PIN");
            if(entered_pin == app->pin_code) {
                FURI_LOG_I(TAG, "PIN Entry: correct PIN entered, checking for alerts");
                // PIN correct, reset fail counter
                period_tracker_reset_pin_fails(app);

                // Check for today's alerts
                // Allocate output buffer on heap (20 predictions = ~3KB, too large for stack)
                Prediction* today_predictions = malloc(sizeof(Prediction) * 20);
                if(today_predictions) {
                    // Pass shared buffer to eliminate malloc/free inside get_today_predictions
                    uint16_t alert_count = get_today_predictions(
                        app->storage,
                        today_predictions,
                        20,
                        app->prediction_buffer,
                        app->prediction_buffer_size);
                    free(today_predictions);

                    // Navigate to Main Menu first (so back button works correctly)
                    scene_manager_next_scene(app->scene_manager, PeriodTrackerSceneMainMenu);

                    if(alert_count > 0) {
                        FURI_LOG_I(
                            TAG,
                            "PIN Entry: %u alerts for today, playing notification and navigating to Daily Digest",
                            alert_count);
                        // Play period alert notification (simple beeps, no LCD flash)
                        period_tracker_alert_notify_if_today(app->storage);
                        // Navigate to Daily Digest (on top of Main Menu)
                        scene_manager_next_scene(
                            app->scene_manager, PeriodTrackerSceneDailyDigest);
                    } else {
                        FURI_LOG_I(TAG, "PIN Entry: no alerts for today, staying on Main Menu");
                    }
                } else {
                    FURI_LOG_E(TAG, "PIN Entry: failed to allocate memory for today predictions");
                    scene_manager_next_scene(app->scene_manager, PeriodTrackerSceneMainMenu);
                }
                consumed = true;
            } else {
                // PIN incorrect
                app->pin_fails++;
                period_tracker_save_pin_fails(app, app->pin_fails);
                FURI_LOG_W(TAG, "PIN Entry: wrong PIN entered, fails=%u/4", app->pin_fails);

                if(app->pin_fails >= 4) {
                    FURI_LOG_W(TAG, "PIN Entry: maximum failures reached, locking app");
                    // Lock the app - replace pin.txt with pin.locked
                    File* file = storage_file_alloc(app->storage);

                    // Create pin.locked file
                    if(storage_file_open(
                           file, APP_DATA_PATH("pin.locked"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
                        storage_file_write(file, "locked", 6);
                        storage_file_close(file);
                        FURI_LOG_I(TAG, "PIN Entry: pin.locked file created");
                    } else {
                        FURI_LOG_E(TAG, "PIN Entry: failed to create pin.locked file");
                    }
                    storage_file_free(file);

                    // Delete pin.txt and pin.fails
                    storage_simply_remove(app->storage, APP_DATA_PATH("pin.txt"));
                    storage_simply_remove(app->storage, APP_DATA_PATH("pin.fails"));
                    FURI_LOG_I(TAG, "PIN Entry: removed pin.txt and pin.fails");

                    // Show locked message and exit
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
                    FURI_LOG_I(
                        TAG,
                        "PIN Entry: showing error message, %d attempts remaining",
                        4 - app->pin_fails);
                    // Show error for 2 seconds, then return to PIN entry
                    widget_reset(app->widget);
                    char error_msg[64];
                    snprintf(
                        error_msg,
                        sizeof(error_msg),
                        "Wrong PIN!\n\n"
                        "%d attempt%s remaining",
                        4 - app->pin_fails,
                        (4 - app->pin_fails) == 1 ? "" : "s");
                    widget_add_string_multiline_element(
                        app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, error_msg);
                    view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewWidget);

                    // Start 2 second timer
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

    // Stop timer if running
    if(app->pin_error_timer) {
        furi_timer_stop(app->pin_error_timer);
        FURI_LOG_I(TAG, "PIN Entry: stopped error timer on exit");
    }

    widget_reset(app->widget);
}
