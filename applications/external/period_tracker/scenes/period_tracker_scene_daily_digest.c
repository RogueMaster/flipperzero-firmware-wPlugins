#include "../period_tracker.h"
#include "../period_tracker_models.h"
#include "../period_tracker_predictions.h"
#include "../period_tracker_csv.h"

void period_tracker_scene_daily_digest_on_enter(void* context) {
    PeriodTrackerApp* app = context;
    furi_assert(app);

    FURI_LOG_I(TAG, "Daily Digest scene: entering");

    // Reset text box
    furi_string_reset(app->text_box_store);
    text_box_reset(app->text_box);

    // Load settings to get alert lead time
    AppSettings settings;
    app_settings_init(&settings);
    settings_load(app->storage, &settings);
    uint8_t lead_days = settings.alert_lead_days;
    if(lead_days == 0) lead_days = 1; // Minimum 1 day
    FURI_LOG_I(TAG, "Daily Digest: using alert lead time of %u days", lead_days);

// Only allocate what UI can display (20 items max shown in loop)
// Reduced from 50 predictions (7.4KB) to 20 (3KB) - saves 4.4KB per visit
#define DAILY_DIGEST_MAX_PREDICTIONS 20
    size_t alloc_size = sizeof(Prediction) * DAILY_DIGEST_MAX_PREDICTIONS;
    FURI_LOG_I(TAG, "Daily Digest: allocating %zu bytes for predictions", alloc_size);
    Prediction* upcoming_predictions = malloc(alloc_size);
    if(!upcoming_predictions) {
        FURI_LOG_E(TAG, "Daily Digest: malloc failed for %zu bytes", alloc_size);
        furi_string_cat_printf(app->text_box_store, "Error!\n\nOut of memory.");
        text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
        view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewTextBox);
        return;
    }
    FURI_LOG_I(TAG, "Daily Digest: predictions allocated at %p", (void*)upcoming_predictions);

    // Generate predictions for the next N days (based on alert lead time)
    uint16_t count = generate_all_predictions(
        app->storage, upcoming_predictions, DAILY_DIGEST_MAX_PREDICTIONS, lead_days);
    FURI_LOG_I(TAG, "Daily Digest: got %u predictions for next %u days", count, lead_days);

    // Get today's date
    SimpleDate today;
    get_today(&today);
    char today_str[16];
    format_date_string(&today, today_str, sizeof(today_str));

    // Build text
    furi_string_cat_printf(app->text_box_store, "Daily Digest\n%s\n", today_str);
    furi_string_cat_printf(
        app->text_box_store,
        "(Showing alerts %u day%s ahead)\n\n",
        lead_days,
        lead_days > 1 ? "s" : "");

    if(count == 0) {
        furi_string_cat_printf(
            app->text_box_store,
            "No alerts in the next %u day%s!",
            lead_days,
            lead_days > 1 ? "s" : "");
    } else {
        furi_string_cat_printf(
            app->text_box_store, "%u upcoming alert%s:\n\n", count, count > 1 ? "s" : "");

        // Group by girl
        char last_girl[MAX_NAME_LENGTH] = "";

        for(uint16_t i = 0; i < count && i < 20; i++) { // Limit to 20 items to prevent overflow
            // Check string size to prevent overflow
            if(furi_string_size(app->text_box_store) > 2000) {
                furi_string_cat_printf(app->text_box_store, "\n... (truncated)");
                break;
            }

            // Print girl name if different from last
            if(strcmp(upcoming_predictions[i].girl_name, last_girl) != 0) {
                furi_string_cat_printf(
                    app->text_box_store, "--- %s ---\n", upcoming_predictions[i].girl_name);
                strncpy(last_girl, upcoming_predictions[i].girl_name, MAX_NAME_LENGTH - 1);
            }

            // Format the prediction date
            char pred_date_str[16];
            format_date_string(
                &upcoming_predictions[i].date, pred_date_str, sizeof(pred_date_str));

            // Get confidence indicator
            const char* confidence =
                get_confidence_indicator(upcoming_predictions[i].confidence_level);

            // Check if we should show date range
            char range_str[64] = "";
            bool has_range = format_date_range(
                &upcoming_predictions[i].earliest_date,
                &upcoming_predictions[i].latest_date,
                range_str,
                sizeof(range_str));

            const char* type_str;
            switch(upcoming_predictions[i].type) {
            case PredictionTypePeriodStart:
                type_str = "[PERIOD]";
                break;
            case PredictionTypePMS:
                type_str = "[PMS]";
                break;
            case PredictionTypePain:
                type_str = "[PAIN]";
                break;
            case PredictionTypeOvulation:
                type_str = "[OVULATION]";
                break;
            default:
                type_str = "[ALERT]";
                break;
            }

            if(has_range) {
                furi_string_cat_printf(
                    app->text_box_store,
                    "%s %s %s\n(%s)\n%s\n",
                    pred_date_str,
                    type_str,
                    confidence,
                    range_str,
                    upcoming_predictions[i].description);
            } else {
                furi_string_cat_printf(
                    app->text_box_store,
                    "%s %s %s\n%s\n",
                    pred_date_str,
                    type_str,
                    confidence,
                    upcoming_predictions[i].description);
            }
        }
    }

    FURI_LOG_I(TAG, "Daily Digest: freeing predictions at %p", (void*)upcoming_predictions);
    free(upcoming_predictions);

    // Log final string size for debugging
    size_t final_size = furi_string_size(app->text_box_store);
    FURI_LOG_I(TAG, "Daily Digest: final text size = %zu bytes", final_size);

    // Configure text box for scrolling
    text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
    text_box_set_font(app->text_box, TextBoxFontText);

    view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewTextBox);
    FURI_LOG_I(TAG, "Daily Digest scene: switched to text box view");
}

bool period_tracker_scene_daily_digest_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        // Let scene manager handle back navigation
        FURI_LOG_I(TAG, "Daily Digest: back button pressed, returning to main menu");
        consumed = false; // Don't consume - let scene manager pop the scene
    }

    return consumed;
}

void period_tracker_scene_daily_digest_on_exit(void* context) {
    PeriodTrackerApp* app = context;
    furi_assert(app);

    FURI_LOG_I(TAG, "Daily Digest scene: exiting");
    text_box_reset(app->text_box);
    furi_string_reset(app->text_box_store);
}
