#pragma once

#include "period_tracker_models.h"
#include <storage/storage.h>

// Generate predictions for a girl for the next num_days
// Returns number of predictions generated
uint16_t generate_predictions(
    Storage* storage,
    const char* girl_name,
    Prediction* predictions,
    uint16_t max_predictions,
    uint16_t num_days);

// Generate predictions for all girls
uint16_t generate_all_predictions(
    Storage* storage,
    Prediction* predictions,
    uint16_t max_predictions,
    uint16_t num_days);

// Get predictions for today only (for daily digest)
// If shared_buffer is provided, uses it to avoid malloc (pass app->prediction_buffer)
// Otherwise falls back to local malloc
uint16_t get_today_predictions(
    Storage* storage,
    Prediction* predictions,
    uint16_t max_predictions,
    Prediction* shared_buffer,
    uint16_t shared_buffer_size);

// Check if a date falls within a prediction window
bool is_date_in_prediction(const Prediction* pred, const SimpleDate* date);
