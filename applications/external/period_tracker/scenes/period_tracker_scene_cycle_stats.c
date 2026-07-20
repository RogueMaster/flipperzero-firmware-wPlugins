#include "../period_tracker.h"
#include "../period_tracker_models.h"
#include "../period_tracker_csv.h"

void period_tracker_scene_cycle_stats_on_enter(void* context) {
    PeriodTrackerApp* app = context;
    furi_assert(app);

    FURI_LOG_I(TAG, "Cycle Stats scene: entering for %s", app->current_girl_name);

    // Reset text box
    furi_string_reset(app->text_box_store);
    text_box_reset(app->text_box);

    // Load profile
    GirlProfile profile;
    if(!profile_load(app->storage, app->current_girl_name, &profile)) {
        FURI_LOG_E(TAG, "Cycle Stats: failed to load profile %s", app->current_girl_name);
        furi_string_cat_printf(app->text_box_store, "Error!\n\nCould not load profile.");
        text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
        view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewTextBox);
        return;
    }

    // Calculate cycle statistics
    CycleStatistics stats;
    calculate_cycle_statistics(app->storage, app->current_girl_name, &stats, 12);

    // Build header
    furi_string_cat_printf(
        app->text_box_store, "Cycle Statistics\n%s\n\n", app->current_girl_name);

    // Check if sufficient data (3 complete cycles = 4 period start dates)
    if(!stats.has_sufficient_data) {
        furi_string_cat_printf(
            app->text_box_store,
            "Getting Started\n\n"
            "Not enough data yet for\n"
            "statistical analysis.\n\n"
            "Complete cycles: %u\n"
            "Minimum needed: 3\n"
            "(4 period start dates)\n\n"
            "Keep logging period starts!\n"
            "More data = better predictions.\n\n"
            "Current profile settings:\n"
            "  Cycle length: %u days\n"
            "  Period length: %u days\n",
            stats.num_cycles_analyzed,
            profile.cycle_length_days,
            profile.period_length_days);

        text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
        text_box_set_font(app->text_box, TextBoxFontText);
        view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewTextBox);
        return;
    }

    // Show statistics
    furi_string_cat_printf(
        app->text_box_store, "Last %u cycles analyzed:\n\n", stats.num_cycles_analyzed);

    furi_string_cat_printf(
        app->text_box_store, "Average cycle: %u days\n", stats.mean_cycle_length);

    furi_string_cat_printf(
        app->text_box_store, "Range: %u-%u days\n", stats.min_cycle_length, stats.max_cycle_length);

    furi_string_cat_printf(
        app->text_box_store,
        "Std deviation: %u.%u days\n",
        stats.std_dev_cycle_length / 10,
        stats.std_dev_cycle_length % 10);

    // Show regularity classification
    const char* regularity_str = cycle_regularity_to_string(stats.regularity);
    const char* confidence_symbol;
    const char* confidence_desc;

    switch(stats.regularity) {
    case CYCLE_REGULAR:
        confidence_symbol = "\u25cf\u25cf\u25cf"; // ●●●
        confidence_desc = "High (90%)";
        break;
    case CYCLE_MODERATE:
        confidence_symbol = "\u25cf\u25cf\u25cb"; // ●●○
        confidence_desc = "Moderate (70%)";
        break;
    case CYCLE_IRREGULAR:
        confidence_symbol = "\u25cf\u25cb\u25cb"; // ●○○
        confidence_desc = "Low (50%)";
        break;
    case CYCLE_CHAOTIC:
        confidence_symbol = "\u25cb\u25cb\u25cb"; // ○○○
        confidence_desc = "Very Low (30%)";
        break;
    default:
        confidence_symbol = "\u25cf\u25cb\u25cb";
        confidence_desc = "Moderate (60%)";
        break;
    }

    furi_string_cat_printf(app->text_box_store, "Regularity: %s\n", regularity_str);

    furi_string_cat_printf(
        app->text_box_store, "Confidence: %s %s\n\n", confidence_symbol, confidence_desc);

    // Check for profile mismatch
    int16_t diff = (int16_t)stats.mean_cycle_length - (int16_t)profile.cycle_length_days;
    if(abs(diff) >= 5) {
        furi_string_cat_printf(
            app->text_box_store,
            "! Profile Mismatch Detected\n"
            "  Profile: %u days\n"
            "  Actual avg: %u days\n"
            "  Difference: %d days\n\n"
            "  Consider updating your profile\n"
            "  for more accurate predictions.\n\n",
            profile.cycle_length_days,
            stats.mean_cycle_length,
            diff);
    }

    // Calculated predictions
    furi_string_cat_printf(app->text_box_store, "Adaptive Predictions:\n");

    // Ovulation day
    if(!profile.is_menopausal && !profile.uses_contraception) {
        uint8_t ovulation_day = calculate_adaptive_ovulation_day(&stats);
        furi_string_cat_printf(app->text_box_store, "  Ovulation: Day %u\n", ovulation_day);
    } else if(profile.uses_contraception) {
        furi_string_cat_printf(app->text_box_store, "  Ovulation: N/A (contraception)\n");
    } else if(profile.is_menopausal) {
        furi_string_cat_printf(app->text_box_store, "  Ovulation: N/A (menopausal)\n");
    }

    furi_string_cat_printf(app->text_box_store, "  Next period quality: %s\n\n", confidence_desc);

    // Tips based on regularity
    furi_string_cat_printf(app->text_box_store, "About These Cycles:\n");

    switch(stats.regularity) {
    case CYCLE_REGULAR:
        furi_string_cat_printf(
            app->text_box_store,
            "Cycles are very consistent!\n"
            "Predictions are highly reliable.\n"
            "Typical variation: +/- 3 days\n");
        break;
    case CYCLE_MODERATE:
        furi_string_cat_printf(
            app->text_box_store,
            "Cycles show some variation.\n"
            "Predictions are fairly reliable.\n"
            "Typical variation: +/- 5 days\n");
        break;
    case CYCLE_IRREGULAR:
        furi_string_cat_printf(
            app->text_box_store,
            "Cycles vary significantly.\n"
            "Predictions have wider ranges.\n"
            "Track symptoms to identify\n"
            "patterns. Consider consulting\n"
            "a healthcare provider.\n");
        break;
    case CYCLE_CHAOTIC:
        furi_string_cat_printf(
            app->text_box_store,
            "Cycles are very irregular.\n"
            "Predictions are less reliable.\n"
            "Variation: +/- 10+ days\n\n"
            "Strongly recommend consulting\n"
            "a healthcare provider to\n"
            "identify underlying causes.\n");
        break;
    }

    // Configure text box for scrolling
    text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
    text_box_set_font(app->text_box, TextBoxFontText);

    view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewTextBox);
    FURI_LOG_I(TAG, "Cycle Stats scene: switched to text box view");
}

bool period_tracker_scene_cycle_stats_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        // Let scene manager handle back navigation
        FURI_LOG_I(TAG, "Cycle Stats: back button pressed, returning to girl menu");
        consumed = false; // Don't consume - let scene manager pop the scene
    }

    return consumed;
}

void period_tracker_scene_cycle_stats_on_exit(void* context) {
    PeriodTrackerApp* app = context;
    furi_assert(app);
    FURI_LOG_I(TAG, "Cycle Stats scene: exiting");
    text_box_reset(app->text_box);
    furi_string_reset(app->text_box_store);
}
