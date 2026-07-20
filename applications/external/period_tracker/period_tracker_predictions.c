#include "period_tracker_predictions.h"
#include "period_tracker_csv.h"
#include <furi.h>
#include <string.h>

#define TAG "PeriodTrackerPredictions"

// Generate predictions for a single girl
uint16_t generate_predictions(
    Storage* storage,
    const char* girl_name,
    Prediction* predictions,
    uint16_t max_predictions,
    uint16_t num_days) {
    uint16_t pred_count = 0;

    // Load profile
    GirlProfile profile;
    if(!profile_load(storage, girl_name, &profile)) {
        FURI_LOG_W(TAG, "Failed to load profile: %s", girl_name);
        return 0;
    }

    // Calculate cycle statistics for this profile
    CycleStatistics stats;
    calculate_cycle_statistics(storage, girl_name, &stats, 12); // Analyze last 12 cycles

    FURI_LOG_I(
        TAG,
        "Stats for %s: mean=%u, std=%u, regularity=%s",
        girl_name,
        stats.mean_cycle_length,
        stats.std_dev_cycle_length,
        cycle_regularity_to_string(stats.regularity));

    // Get today's date
    SimpleDate today;
    get_today(&today);

    // Generate predictions for num_days into the future
    SimpleDate end_date = today;
    add_days_to_date(&end_date, num_days);

    // Get last period start
    SimpleDate last_period;
    if(!events_get_last_period_start(storage, girl_name, &last_period)) {
        FURI_LOG_I(TAG, "No period data for %s", girl_name);
        return 0;
    }

    // Calculate days since last period
    int32_t days_since_last = days_between(&last_period, &today);

    // Use statistical mean if available, otherwise use profile default
    uint8_t cycle_length = stats.has_sufficient_data ? stats.mean_cycle_length :
                                                       profile.cycle_length_days;

    // Safety check: cycle_length must be at least 20 days to prevent infinite loops
    if(cycle_length < 20) {
        FURI_LOG_E(
            TAG, "Invalid cycle_length %u for %s, using default 28", cycle_length, girl_name);
        cycle_length = 28; // Safe default
    }

    // Calculate next period start date using statistical cycle length
    SimpleDate next_period = last_period;
    int32_t days_to_add = cycle_length;

    // If we're past the expected next period, add multiple cycles
    while(days_to_add <= days_since_last) {
        days_to_add += cycle_length;
    }

    add_days_to_date(&next_period, days_to_add);

    // Check if we're currently in an ongoing period
    // If days_since_last < period_length_days, we're still in the period
    SimpleDate current_period;
    if(days_since_last < profile.period_length_days) {
        // Include the current ongoing period
        current_period = last_period;
        FURI_LOG_I(
            TAG, "Currently in ongoing period for %s (day %ld)", girl_name, days_since_last + 1);
    } else {
        // Start from next period
        current_period = next_period;
    }
    while(compare_dates(&current_period, &end_date) <= 0 && pred_count < max_predictions) {
        // Period start prediction with confidence intervals
        if(pred_count < max_predictions) {
            Prediction* pred = &predictions[pred_count++];
            pred->date = current_period;
            pred->type = PredictionTypePeriodStart;
            pred->period_length_days = profile.period_length_days;
            strncpy(pred->girl_name, girl_name, MAX_NAME_LENGTH - 1);

            // Calculate confidence intervals based on statistics
            if(stats.has_sufficient_data) {
                // Use ±2σ for 95% confidence interval
                int16_t margin = stats.std_dev_cycle_length * 2;

                pred->earliest_date = current_period;
                add_days_to_date(&pred->earliest_date, -margin);

                pred->latest_date = current_period;
                add_days_to_date(&pred->latest_date, margin);

                // Calculate confidence level based on regularity
                switch(stats.regularity) {
                case CYCLE_REGULAR:
                    pred->confidence_level = 90; // High confidence
                    break;
                case CYCLE_MODERATE:
                    pred->confidence_level = 70; // Moderate confidence
                    break;
                case CYCLE_IRREGULAR:
                    pred->confidence_level = 50; // Low confidence
                    break;
                case CYCLE_CHAOTIC:
                    pred->confidence_level = 30; // Very low confidence
                    break;
                default:
                    pred->confidence_level = 60;
                    break;
                }

                snprintf(
                    pred->description,
                    sizeof(pred->description),
                    "Period starts (lasts %d days)",
                    profile.period_length_days);
            } else {
                // No statistics available, use default confidence
                pred->earliest_date = current_period;
                add_days_to_date(&pred->earliest_date, -3);
                pred->latest_date = current_period;
                add_days_to_date(&pred->latest_date, 3);
                pred->confidence_level = 60;
                snprintf(
                    pred->description,
                    sizeof(pred->description),
                    "Period starts (lasts %d days)",
                    profile.period_length_days);
            }
        }

        // PMS window: Day -3 to -1 before period (3 days).
        // Include if the window overlaps [today, end_date], even when PMS
        // started before today (still ongoing today / in lead window).
        SimpleDate pms_start = current_period;
        add_days_to_date(&pms_start, -3);
        SimpleDate pms_end = pms_start;
        add_days_to_date(&pms_end, 2);
        if(compare_dates(&pms_end, &today) >= 0 && compare_dates(&pms_start, &end_date) <= 0 &&
           pred_count < max_predictions) {
            Prediction* pred = &predictions[pred_count++];
            pred->date = pms_start;
            pred->type = PredictionTypePMS;
            pred->period_length_days = 3;
            strncpy(pred->girl_name, girl_name, MAX_NAME_LENGTH - 1);
            strcpy(pred->description, "PMS starts (lasts 3 days)");
            pred->earliest_date = pms_start;
            pred->latest_date = pms_end;
            pred->confidence_level = 60;
        }

        // Pain window: Day -1 to +1 around period start (3 days).
        // Same overlap rule as PMS so mid-window days are not dropped.
        SimpleDate pain_start = current_period;
        add_days_to_date(&pain_start, -1);
        SimpleDate pain_end = pain_start;
        add_days_to_date(&pain_end, 2);
        if(compare_dates(&pain_end, &today) >= 0 && compare_dates(&pain_start, &end_date) <= 0 &&
           pred_count < max_predictions) {
            Prediction* pred = &predictions[pred_count++];
            pred->date = pain_start;
            pred->type = PredictionTypePain;
            pred->period_length_days = 3;
            strncpy(pred->girl_name, girl_name, MAX_NAME_LENGTH - 1);
            strcpy(pred->description, "Pain/cramps likely (3 days)");
            pred->earliest_date = pain_start;
            pred->latest_date = pain_end;
            pred->confidence_level = 60;
        }

        // Ovulation: Adaptive calculation based on cycle length (if not menopausal or on contraception)
        if(!profile.is_menopausal && !profile.uses_contraception) {
            // Use adaptive ovulation day if statistics available
            uint8_t ovulation_day =
                stats.has_sufficient_data ? calculate_adaptive_ovulation_day(&stats) : 14;

            SimpleDate ovulation = current_period;
            add_days_to_date(&ovulation, ovulation_day);
            if(compare_dates(&ovulation, &today) >= 0 &&
               compare_dates(&ovulation, &end_date) <= 0 && pred_count < max_predictions) {
                Prediction* pred = &predictions[pred_count++];
                pred->date = ovulation;
                pred->type = PredictionTypeOvulation;
                strncpy(pred->girl_name, girl_name, MAX_NAME_LENGTH - 1);

                // Add confidence intervals for ovulation (±2 days typically)
                if(stats.has_sufficient_data) {
                    pred->earliest_date = ovulation;
                    add_days_to_date(&pred->earliest_date, -2);
                    pred->latest_date = ovulation;
                    add_days_to_date(&pred->latest_date, 2);
                    pred->confidence_level = 70; // Moderate confidence for ovulation
                    snprintf(
                        pred->description,
                        sizeof(pred->description),
                        "Ovulation (day %d)",
                        ovulation_day);
                } else {
                    pred->earliest_date = ovulation;
                    pred->latest_date = ovulation;
                    pred->confidence_level = 60;
                    strcpy(pred->description, "Ovulation (fertile window)");
                }
            }
        }

        // Move to next cycle using statistical cycle length
        add_days_to_date(&current_period, cycle_length);
    }

    FURI_LOG_I(TAG, "Generated %u predictions for %s", pred_count, girl_name);
    return pred_count;
}

// Generate predictions for all girls
uint16_t generate_all_predictions(
    Storage* storage,
    Prediction* predictions,
    uint16_t max_predictions,
    uint16_t num_days) {
    FURI_LOG_I(
        TAG, "generate_all_predictions: starting (max=%u, days=%u)", max_predictions, num_days);
    char names[MAX_GIRLS][MAX_NAME_LENGTH];
    uint8_t girl_count = 0;

    if(!profile_list_all(storage, names, &girl_count, MAX_GIRLS)) {
        FURI_LOG_I(TAG, "generate_all_predictions: profile_list_all failed or returned 0");
        return 0;
    }

    FURI_LOG_I(TAG, "generate_all_predictions: found %u profiles", girl_count);
    uint16_t total_predictions = 0;

    for(uint8_t i = 0; i < girl_count && total_predictions < max_predictions; i++) {
        FURI_LOG_I(
            TAG,
            "generate_all_predictions: processing profile %u/%u: %s",
            i + 1,
            girl_count,
            names[i]);
        uint16_t remaining = max_predictions - total_predictions;
        uint16_t count = generate_predictions(
            storage, names[i], &predictions[total_predictions], remaining, num_days);
        FURI_LOG_I(TAG, "generate_all_predictions: got %u predictions for %s", count, names[i]);
        total_predictions += count;
    }

    FURI_LOG_I(TAG, "Generated %u total predictions for %u girls", total_predictions, girl_count);
    return total_predictions;
}

// True if prediction window overlaps any day in [window_start, window_end] inclusive.
static bool prediction_overlaps_date_range(
    const Prediction* pred,
    const SimpleDate* window_start,
    const SimpleDate* window_end) {
    SimpleDate d = *window_start;
    // Cap scan: lead time is 1–7 days; include a small margin for multi-day windows
    for(uint8_t i = 0; i < 16; i++) {
        if(compare_dates(&d, window_end) > 0) {
            break;
        }
        if(is_date_in_prediction(pred, &d)) {
            return true;
        }
        add_days_to_date(&d, 1);
    }
    return false;
}

// Predictions in the alert lead window (same horizon as Daily Digest)
uint16_t get_alert_predictions(
    Storage* storage,
    Prediction* predictions,
    uint16_t max_predictions,
    Prediction* shared_buffer,
    uint16_t shared_buffer_size,
    uint8_t lead_days) {
    if(lead_days == 0) {
        lead_days = 1;
    }
    if(lead_days > 7) {
        lead_days = 7;
    }

    // Buffer large enough for multi-day multi-profile predictions
    uint16_t alloc_size = max_predictions + 10;
    if(alloc_size > 40) alloc_size = 40;
    if(alloc_size < 20) alloc_size = 20;

    Prediction* all_predictions = NULL;
    bool using_shared_buffer = false;

    if(shared_buffer && shared_buffer_size >= alloc_size) {
        all_predictions = shared_buffer;
        using_shared_buffer = true;
        FURI_LOG_D(TAG, "get_alert_predictions: shared buffer (%u)", alloc_size);
    } else {
        all_predictions = malloc(sizeof(Prediction) * alloc_size);
        if(!all_predictions) {
            FURI_LOG_E(TAG, "get_alert_predictions: malloc failed");
            return 0;
        }
        FURI_LOG_D(TAG, "get_alert_predictions: malloc (%u)", alloc_size);
    }

    // Same generation window as Daily Digest
    uint16_t count =
        generate_all_predictions(storage, all_predictions, alloc_size, lead_days);

    SimpleDate today;
    get_today(&today);
    SimpleDate window_end = today;
    add_days_to_date(&window_end, lead_days);

    uint16_t alert_count = 0;
    for(uint16_t i = 0; i < count && alert_count < max_predictions; i++) {
        // Keep predictions that touch any day in [today, today+lead_days]
        if(prediction_overlaps_date_range(&all_predictions[i], &today, &window_end)) {
            predictions[alert_count++] = all_predictions[i];
        }
    }

    if(!using_shared_buffer) {
        free(all_predictions);
    }

    FURI_LOG_I(
        TAG,
        "get_alert_predictions: %u alerts in next %u day(s) (generated %u)",
        alert_count,
        lead_days,
        count);
    return alert_count;
}

uint16_t get_today_predictions(
    Storage* storage,
    Prediction* predictions,
    uint16_t max_predictions,
    Prediction* shared_buffer,
    uint16_t shared_buffer_size) {
    // lead_days=1: today through tomorrow (matches default alert lead)
    return get_alert_predictions(
        storage, predictions, max_predictions, shared_buffer, shared_buffer_size, 1);
}

// Check if a date falls within a prediction window
bool is_date_in_prediction(const Prediction* pred, const SimpleDate* date) {
    switch(pred->type) {
    case PredictionTypePeriodStart: {
        // Period lasts multiple days - check if date is within period duration
        // Use actual period length from the prediction
        uint8_t period_length = pred->period_length_days > 0 ? pred->period_length_days : 5;
        for(int i = 0; i < period_length; i++) {
            SimpleDate check_date = pred->date;
            add_days_to_date(&check_date, i);
            if(dates_equal(&check_date, date)) {
                return true;
            }
        }
        return false;
    }

    case PredictionTypePMS:
        // PMS window is 3 days (-3 to -1)
        for(int i = 0; i < 3; i++) {
            SimpleDate check_date = pred->date;
            add_days_to_date(&check_date, i);
            if(dates_equal(&check_date, date)) {
                return true;
            }
        }
        return false;

    case PredictionTypePain:
        // Pain window is 3 days (-1 to +1)
        for(int i = 0; i < 3; i++) {
            SimpleDate check_date = pred->date;
            add_days_to_date(&check_date, i);
            if(dates_equal(&check_date, date)) {
                return true;
            }
        }
        return false;

    case PredictionTypeOvulation:
        // Ovulation is a single day
        return dates_equal(&pred->date, date);

    default:
        return dates_equal(&pred->date, date);
    }
}
