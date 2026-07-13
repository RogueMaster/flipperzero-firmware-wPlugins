#include "../period_tracker.h"
#include "../period_tracker_csv.h"
#include "../period_tracker_models.h"
#include "../period_tracker_predictions.h"
#include "../period_tracker_alert.h"

void period_tracker_scene_start_on_enter(void* context) {
    PeriodTrackerApp* app = context;
    furi_assert(app);

    FURI_LOG_I(TAG, "=== Start Scene: Entering ===");

    // Try to load settings
    AppSettings settings;
    app_settings_init(&settings);

    FURI_LOG_I(TAG, "Start: loading app settings");
    bool settings_loaded = settings_load(app->storage, &settings);
    FURI_LOG_I(
        TAG, "Start: settings loaded=%d, pin_enabled=%d", settings_loaded, settings.pin_enabled);

    // Check if PIN is locked
    FURI_LOG_I(TAG, "Start: checking PIN lock status");
    app->pin_locked = period_tracker_check_pin_locked(app);
    FURI_LOG_I(TAG, "Start: pin_locked=%d", app->pin_locked);

    if(app->pin_locked) {
        FURI_LOG_W(TAG, "Start: App is LOCKED due to failed PIN attempts");
        // Show locked message
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
    } else if(settings.pin_enabled) {
        FURI_LOG_I(TAG, "Start: PIN protection is enabled, checking PIN file");
        // PIN protection is enabled - check if PIN file exists
        app->pin_set = period_tracker_load_pin(app);
        FURI_LOG_I(TAG, "Start: pin_set=%d", app->pin_set);

        if(app->pin_set) {
            // PIN file exists, load fail counter and go to PIN entry
            app->pin_fails = period_tracker_load_pin_fails(app);
            FURI_LOG_I(
                TAG, "Start: PIN file found, going to PIN entry (fails=%u/4)", app->pin_fails);
            scene_manager_next_scene(app->scene_manager, PeriodTrackerScenePinEntry);
        } else {
            // PIN enabled but no PIN file - go to PIN setup to create one
            FURI_LOG_W(TAG, "Start: PIN enabled but no PIN file exists, going to PIN setup");
            scene_manager_next_scene(app->scene_manager, PeriodTrackerScenePinSetup);
        }
    } else {
        // PIN protection disabled, check if this is first run (no profiles)
        FURI_LOG_I(TAG, "Start: PIN protection disabled, checking for profiles");

        // Allocate names array on heap to avoid stack overflow (640 bytes)
        char(*names)[MAX_NAME_LENGTH] = malloc(sizeof(char[MAX_GIRLS][MAX_NAME_LENGTH]));
        if(!names) {
            FURI_LOG_E(TAG, "Failed to allocate memory for names array");
            return;
        }

        uint8_t profile_count = 0;
        bool has_profiles = profile_list_all(app->storage, names, &profile_count, MAX_GIRLS) &&
                            profile_count > 0;

        if(!has_profiles) {
            free(names);
            // First run - show onboarding
            FURI_LOG_I(TAG, "Start: First run detected, showing onboarding");
            furi_string_reset(app->text_box_store);
            text_box_reset(app->text_box);

            furi_string_cat_printf(
                app->text_box_store,
                "Welcome to Period Tracker!\n\n"
                "This app helps you track menstrual cycles for multiple people.\n\n"
                "=== Getting Started ===\n\n"
                "1. Create your first profile\n"
                "2. Log period start dates\n"
                "3. View predictions & alerts\n\n"
                "=== Features ===\n"
                "- Multi-user support\n"
                "- Cycle predictions\n"
                "- Statistical learning\n"
                "- Smart alerts\n"
                "- PIN protection\n\n"
                "=== Next Step ===\n"
                "Press Back to create your first profile!\n\n"
                "You can access Help/About from the Main Menu anytime for more information.");

            text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
            text_box_set_font(app->text_box, TextBoxFontText);
            view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewTextBox);
        } else {
            free(names); // Free names array before checking alerts
            // Normal startup - check for today's alerts
            FURI_LOG_I(TAG, "Start: Profiles exist, checking for alerts");

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
                        "Start: %u alerts for today, playing notification and navigating to Daily Digest",
                        alert_count);
                    // Play period alert notification (simple beeps, no LCD flash)
                    period_tracker_alert_notify_if_today(app->storage);
                    // Navigate to Daily Digest (on top of Main Menu)
                    scene_manager_next_scene(app->scene_manager, PeriodTrackerSceneDailyDigest);
                } else {
                    FURI_LOG_I(TAG, "Start: no alerts for today, staying on Main Menu");
                }
            } else {
                FURI_LOG_E(TAG, "Start: failed to allocate memory for today predictions");
                scene_manager_next_scene(app->scene_manager, PeriodTrackerSceneMainMenu);
            }
        }
    }

    FURI_LOG_I(TAG, "=== Start Scene: Completed ===");
}

bool period_tracker_scene_start_on_event(void* context, SceneManagerEvent event) {
    PeriodTrackerApp* app = context;
    furi_assert(app);
    bool consumed = false;

    // Handle OK button from onboarding screen
    if(event.type == SceneManagerEventTypeBack) {
        // User pressed Back/OK from onboarding text
        // Check if we're showing onboarding (no profiles exist)
        // Allocate names array on heap to avoid stack overflow
        char(*names)[MAX_NAME_LENGTH] = malloc(sizeof(char[MAX_GIRLS][MAX_NAME_LENGTH]));
        if(!names) {
            FURI_LOG_E(TAG, "Failed to allocate memory for names array");
            return consumed;
        }

        uint8_t profile_count = 0;
        bool has_profiles = profile_list_all(app->storage, names, &profile_count, MAX_GIRLS) &&
                            profile_count > 0;

        if(!has_profiles) {
            // First run - navigate to Add Profile
            FURI_LOG_I(TAG, "Start: User pressed OK on onboarding, navigating to Add Profile");
            scene_manager_next_scene(app->scene_manager, PeriodTrackerSceneAddGirl);
            consumed = true;
        }

        free(names);
    }

    return consumed;
}

void period_tracker_scene_start_on_exit(void* context) {
    PeriodTrackerApp* app = context;
    furi_assert(app);
    FURI_LOG_I(TAG, "Start scene: exiting");
    widget_reset(app->widget);
}
