#include "../period_tracker.h"
#include "../period_tracker_csv.h"
#include <storage/storage.h>

#define PIN_SETUP_STATE_FIRST_ENTRY   0
#define PIN_SETUP_STATE_CONFIRM_ENTRY 1
#define PIN_SETUP_STATE_RESULT_OK     2
#define PIN_SETUP_STATE_RESULT_ERR    3

#define PIN_SETUP_EVENT_SUCCESS   102
#define PIN_SETUP_EVENT_MISMATCH  103
#define PIN_SETUP_EVENT_SUBMITTED 104

static void pin_setup_done_callback(void* context, uint16_t pin) {
    PeriodTrackerApp* app = context;
    app->pin_draft = pin;
    view_dispatcher_send_custom_event(app->view_dispatcher, PIN_SETUP_EVENT_SUBMITTED);
}

static void pin_setup_show_first(PeriodTrackerApp* app) {
    scene_manager_set_scene_state(
        app->scene_manager, PeriodTrackerScenePinSetup, PIN_SETUP_STATE_FIRST_ENTRY);
    app->pin_first_entry = 0;

    pin_input_reset(app->pin_input);
    pin_input_set_header(app->pin_input, "Set new PIN");
    // Setup: require OK after 4 digits so the user can still Backspace the last one
    pin_input_set_auto_submit(app->pin_input, false);
    pin_input_set_result_callback(app->pin_input, pin_setup_done_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewPinInput);
}

static void pin_setup_show_confirm(PeriodTrackerApp* app) {
    scene_manager_set_scene_state(
        app->scene_manager, PeriodTrackerScenePinSetup, PIN_SETUP_STATE_CONFIRM_ENTRY);

    pin_input_reset(app->pin_input);
    pin_input_set_header(app->pin_input, "Confirm PIN");
    pin_input_set_auto_submit(app->pin_input, false);
    pin_input_set_result_callback(app->pin_input, pin_setup_done_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewPinInput);
}

void period_tracker_scene_pin_setup_on_enter(void* context) {
    PeriodTrackerApp* app = context;
    furi_assert(app);
    pin_setup_show_first(app);
}

bool period_tracker_scene_pin_setup_on_event(void* context, SceneManagerEvent event) {
    PeriodTrackerApp* app = context;
    furi_assert(app);
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        // Cancel setup — scene manager pops
        consumed = false;
    } else if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == PIN_SETUP_EVENT_SUBMITTED) {
            uint32_t pin = app->pin_draft;
            uint32_t state =
                scene_manager_get_scene_state(app->scene_manager, PeriodTrackerScenePinSetup);

            if(state == PIN_SETUP_STATE_FIRST_ENTRY) {
                app->pin_first_entry = pin;
                pin_setup_show_confirm(app);
                consumed = true;
            } else {
                if(pin == app->pin_first_entry) {
                    view_dispatcher_send_custom_event(
                        app->view_dispatcher, PIN_SETUP_EVENT_SUCCESS);
                } else {
                    view_dispatcher_send_custom_event(
                        app->view_dispatcher, PIN_SETUP_EVENT_MISMATCH);
                }
                consumed = true;
            }
        } else if(event.event == PIN_SETUP_EVENT_SUCCESS) {
            File* file = storage_file_alloc(app->storage);
            bool saved = false;

            if(storage_file_open(file, APP_DATA_PATH("pin.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
                char pin_str[8];
                // Always store as 4 digits so leading zeros are preserved
                snprintf(pin_str, sizeof(pin_str), "%04lu", (unsigned long)app->pin_first_entry);
                if(storage_file_write(file, pin_str, strlen(pin_str))) {
                    saved = true;
                }
                storage_file_close(file);
            }
            storage_file_free(file);

            if(saved) {
                app->pin_code = app->pin_first_entry;
                app->pin_set = true;

                AppSettings settings;
                settings_load(app->storage, &settings);
                settings.pin_enabled = true;
                settings_save(app->storage, &settings);

                scene_manager_set_scene_state(
                    app->scene_manager, PeriodTrackerScenePinSetup, PIN_SETUP_STATE_RESULT_OK);
                period_tracker_widget_show_message(
                    app,
                    "PIN Saved!\n\n"
                    "Your PIN has been\n"
                    "set successfully.",
                    FontPrimary);
            } else {
                scene_manager_set_scene_state(
                    app->scene_manager, PeriodTrackerScenePinSetup, PIN_SETUP_STATE_RESULT_ERR);
                period_tracker_widget_show_message(
                    app, "Error!\n\nFailed to save PIN.", FontPrimary);
            }
            consumed = true;
        } else if(event.event == PIN_SETUP_EVENT_MISMATCH) {
            scene_manager_set_scene_state(
                app->scene_manager, PeriodTrackerScenePinSetup, PIN_SETUP_STATE_RESULT_ERR);
            period_tracker_widget_show_message(
                app,
                "Error!\n\n"
                "PINs do not match.\n\n"
                "OK to try again.",
                FontPrimary);
            consumed = true;
        } else if(event.event == PERIOD_TRACKER_EVENT_WIDGET_DISMISS) {
            uint32_t state =
                scene_manager_get_scene_state(app->scene_manager, PeriodTrackerScenePinSetup);
            if(state == PIN_SETUP_STATE_RESULT_OK) {
                scene_manager_previous_scene(app->scene_manager);
            } else {
                pin_setup_show_first(app);
            }
            consumed = true;
        }
    }

    return consumed;
}

void period_tracker_scene_pin_setup_on_exit(void* context) {
    PeriodTrackerApp* app = context;
    furi_assert(app);
    pin_input_reset(app->pin_input);
    widget_reset(app->widget);
    app->pin_first_entry = 0;
}
