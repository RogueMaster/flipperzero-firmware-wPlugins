#include "../period_tracker.h"
#include "../period_tracker_models.h"
#include "../period_tracker_predictions.h"
#include "../period_tracker_csv.h"

void period_tracker_scene_view_predictions_on_enter(void* context) {
    PeriodTrackerApp* app = context;
    furi_assert(app);

    FURI_LOG_I(TAG, "View Predictions scene: entering");

    // Reset text box
    furi_string_reset(app->text_box_store);
    text_box_reset(app->text_box);

// Allocate 30 predictions (4.4KB) - UI truncates at ~30 items anyway (3000 byte text limit)
// Reduced from 50 (7.4KB) - saves 3KB per screen visit
#define VIEW_PREDICTIONS_MAX 30
    size_t alloc_size = sizeof(Prediction) * VIEW_PREDICTIONS_MAX;
    FURI_LOG_I(TAG, "View Predictions: allocating %zu bytes for predictions", alloc_size);
    Prediction* predictions = malloc(alloc_size);
    if(!predictions) {
        FURI_LOG_E(TAG, "View Predictions: malloc failed for %zu bytes", alloc_size);
        furi_string_cat_printf(app->text_box_store, "Error!\n\nOut of memory.");
        text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
        view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewTextBox);
        return;
    }
    FURI_LOG_I(TAG, "View Predictions: predictions allocated at %p", (void*)predictions);

    uint16_t count;

    // Check if we're viewing all girls or just current girl
    if(strlen(app->current_girl_name) > 0) {
        FURI_LOG_I(TAG, "View Predictions: generating for girl '%s'", app->current_girl_name);
        count = generate_predictions(
            app->storage, app->current_girl_name, predictions, VIEW_PREDICTIONS_MAX, 60);
    } else {
        FURI_LOG_I(TAG, "View Predictions: generating for all girls");
        count = generate_all_predictions(app->storage, predictions, VIEW_PREDICTIONS_MAX, 60);
    }
    FURI_LOG_I(TAG, "View Predictions: generated %u predictions", count);

    // Build display text
    if(strlen(app->current_girl_name) > 0) {
        furi_string_cat_printf(app->text_box_store, "Predictions: %s\n\n", app->current_girl_name);
    } else {
        furi_string_cat_printf(app->text_box_store, "Predictions (All Profiles)\n\n");
    }

    if(count == 0) {
        furi_string_cat_printf(
            app->text_box_store, "No predictions available.\n\nLog a period start first!");
    } else {
        // Show all predictions (scrolling will handle overflow)
        for(uint16_t i = 0; i < count && i < VIEW_PREDICTIONS_MAX; i++) {
            // Check string size to prevent overflow
            if(furi_string_size(app->text_box_store) > 3000) {
                furi_string_cat_printf(app->text_box_store, "... (truncated)\n");
                break;
            }

            char date_str[16];
            format_date_string(&predictions[i].date, date_str, sizeof(date_str));

            // Get confidence indicator
            const char* confidence = get_confidence_indicator(predictions[i].confidence_level);

            // Check if we should show date range
            char range_str[64] = "";
            bool has_range = format_date_range(
                &predictions[i].earliest_date,
                &predictions[i].latest_date,
                range_str,
                sizeof(range_str));

            const char* type_icon;
            switch(predictions[i].type) {
            case PredictionTypePeriodStart:
                type_icon = "[P]";
                break;
            case PredictionTypePMS:
                type_icon = "[PMS]";
                break;
            case PredictionTypePain:
                type_icon = "[PAIN]";
                break;
            case PredictionTypeOvulation:
                type_icon = "[OV]";
                break;
            default:
                type_icon = "[?]";
                break;
            }

            if(strlen(app->current_girl_name) == 0) {
                // Show girl name for "all girls" view
                if(has_range) {
                    furi_string_cat_printf(
                        app->text_box_store,
                        "%s %s %s\n(%s)\n%s: %s\n\n",
                        date_str,
                        type_icon,
                        confidence,
                        range_str,
                        predictions[i].girl_name,
                        predictions[i].description);
                } else {
                    furi_string_cat_printf(
                        app->text_box_store,
                        "%s %s %s\n%s: %s\n\n",
                        date_str,
                        type_icon,
                        confidence,
                        predictions[i].girl_name,
                        predictions[i].description);
                }
            } else {
                // Single girl view
                if(has_range) {
                    furi_string_cat_printf(
                        app->text_box_store,
                        "%s %s %s\n(%s)\n%s\n\n",
                        date_str,
                        type_icon,
                        confidence,
                        range_str,
                        predictions[i].description);
                } else {
                    furi_string_cat_printf(
                        app->text_box_store,
                        "%s %s %s\n%s\n\n",
                        date_str,
                        type_icon,
                        confidence,
                        predictions[i].description);
                }
            }
        }
    }

    FURI_LOG_I(TAG, "View Predictions: freeing predictions at %p", (void*)predictions);
    free(predictions);

    // Log final string size for debugging
    size_t final_size = furi_string_size(app->text_box_store);
    FURI_LOG_I(TAG, "View Predictions: final text size = %zu bytes", final_size);

    // Configure text box for scrolling
    text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
    text_box_set_font(app->text_box, TextBoxFontText);

    view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewTextBox);
    FURI_LOG_I(TAG, "View Predictions scene: switched to text box view");
}

bool period_tracker_scene_view_predictions_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void period_tracker_scene_view_predictions_on_exit(void* context) {
    PeriodTrackerApp* app = context;
    furi_assert(app);
    FURI_LOG_I(TAG, "View Predictions scene: exiting");
    text_box_reset(app->text_box);
    furi_string_reset(app->text_box_store);
}
