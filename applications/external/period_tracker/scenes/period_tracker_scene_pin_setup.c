#include "../period_tracker.h"
#include "../period_tracker_csv.h"
#include <storage/storage.h>

#define PIN_SETUP_STATE_FIRST_ENTRY   0
#define PIN_SETUP_STATE_CONFIRM_ENTRY 1

// Store state in scene state
#define SCENE_STATE_KEY 0

static void pin_setup_callback(void* context, int32_t number) {
    PeriodTrackerApp* app = context;

    uint32_t state = scene_manager_get_scene_state(app->scene_manager, PeriodTrackerScenePinSetup);

    if(state == PIN_SETUP_STATE_FIRST_ENTRY) {
        // First entry - store PIN and ask for confirmation
        app->pin_first_entry = (uint32_t)number;
        scene_manager_set_scene_state(
            app->scene_manager, PeriodTrackerScenePinSetup, PIN_SETUP_STATE_CONFIRM_ENTRY);
        view_dispatcher_send_custom_event(app->view_dispatcher, PIN_SETUP_STATE_CONFIRM_ENTRY);
    } else {
        // Second entry - verify match
        uint32_t second_entry = (uint32_t)number;

        if(second_entry == app->pin_first_entry) {
            // PINs match - save to file
            view_dispatcher_send_custom_event(app->view_dispatcher, 2); // Success event
        } else {
            // PINs don't match - show error
            view_dispatcher_send_custom_event(app->view_dispatcher, 3); // Error event
        }
    }
}

void period_tracker_scene_pin_setup_on_enter(void* context) {
    PeriodTrackerApp* app = context;
    furi_assert(app);

    // Initialize state
    scene_manager_set_scene_state(
        app->scene_manager, PeriodTrackerScenePinSetup, PIN_SETUP_STATE_FIRST_ENTRY);
    app->pin_first_entry = 0;

    // Setup number input for first entry
    number_input_set_header_text(app->number_input, "Enter new PIN (0000-9999):");
    number_input_set_result_callback(
        app->number_input,
        pin_setup_callback,
        app,
        0, // Start at 0
        0, // Min value
        9999); // Max value (4 digits)

    view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewNumberInput);
}

bool period_tracker_scene_pin_setup_on_event(void* context, SceneManagerEvent event) {
    PeriodTrackerApp* app = context;
    furi_assert(app);
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        // Allow canceling PIN setup - go back to previous scene
        consumed = false; // Let scene manager handle the back navigation
    } else if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == PIN_SETUP_STATE_CONFIRM_ENTRY) {
            // Show second entry screen
            number_input_set_header_text(app->number_input, "Confirm PIN:");
            number_input_set_result_callback(
                app->number_input,
                pin_setup_callback,
                app,
                0, // Start at 0
                0, // Min value
                9999); // Max value
            view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewNumberInput);
            consumed = true;
        } else if(event.event == 2) {
            // Success - save PIN to file
            File* file = storage_file_alloc(app->storage);
            bool saved = false;

            if(storage_file_open(file, APP_DATA_PATH("pin.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
                char pin_str[16];
                snprintf(pin_str, sizeof(pin_str), "%04lu", app->pin_first_entry);
                if(storage_file_write(file, pin_str, strlen(pin_str))) {
                    saved = true;
                }
                storage_file_close(file);
            }
            storage_file_free(file);

            if(saved) {
                // Update app state
                app->pin_code = app->pin_first_entry;
                app->pin_set = true;

                // Enable PIN protection in settings
                AppSettings settings;
                settings_load(app->storage, &settings);
                settings.pin_enabled = true;
                settings_save(app->storage, &settings);

                // Show success message
                widget_reset(app->widget);
                widget_add_string_multiline_element(
                    app->widget,
                    64,
                    32,
                    AlignCenter,
                    AlignCenter,
                    FontPrimary,
                    "PIN Saved!\n\n"
                    "Your PIN has been\n"
                    "set successfully.");
            } else {
                widget_reset(app->widget);
                widget_add_string_multiline_element(
                    app->widget,
                    64,
                    32,
                    AlignCenter,
                    AlignCenter,
                    FontPrimary,
                    "Error!\n\n"
                    "Failed to save PIN.");
            }
            view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewWidget);
            consumed = true;
        } else if(event.event == 3) {
            // Error - PINs don't match
            widget_reset(app->widget);
            widget_add_string_multiline_element(
                app->widget,
                64,
                32,
                AlignCenter,
                AlignCenter,
                FontPrimary,
                "Error!\n\n"
                "PINs do not match.\n\n"
                "Press Back to try again.");
            view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewWidget);
            consumed = true;
        }
    }

    return consumed;
}

void period_tracker_scene_pin_setup_on_exit(void* context) {
    PeriodTrackerApp* app = context;
    furi_assert(app);
    widget_reset(app->widget);
    app->pin_first_entry = 0;
}
