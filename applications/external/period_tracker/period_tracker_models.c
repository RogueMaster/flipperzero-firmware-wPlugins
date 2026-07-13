#include "period_tracker_models.h"
#include <furi.h>
#include <furi_hal.h>
#include <stdio.h>
#include <string.h>

// Convert SimpleDate to UNIX timestamp (midnight UTC)
uint32_t simple_date_to_timestamp(const SimpleDate* date) {
    DateTime dt = {0};
    dt.year = date->year;
    dt.month = date->month;
    dt.day = date->day;
    dt.hour = 0;
    dt.minute = 0;
    dt.second = 0;
    return datetime_datetime_to_timestamp(&dt);
}

// Convert UNIX timestamp to SimpleDate
void timestamp_to_simple_date(uint32_t timestamp, SimpleDate* date) {
    DateTime dt;
    datetime_timestamp_to_datetime(timestamp, &dt);
    date->year = dt.year;
    date->month = dt.month;
    date->day = dt.day;
}

// Calculate days between two dates
int32_t days_between(const SimpleDate* date1, const SimpleDate* date2) {
    uint32_t ts1 = simple_date_to_timestamp(date1);
    uint32_t ts2 = simple_date_to_timestamp(date2);
    // Cast to int64_t to handle negative differences correctly
    int64_t diff = (int64_t)ts2 - (int64_t)ts1;
    return (int32_t)(diff / 86400); // 86400 seconds in a day
}

// Check if date is valid
bool is_date_valid(const SimpleDate* date) {
    if(date->year < 2000 || date->year > 2099) return false;
    if(date->month < 1 || date->month > 12) return false;
    if(date->day < 1 || date->day > 31) return false;

    // Check days per month
    uint8_t days_in_month =
        datetime_get_days_per_month(datetime_is_leap_year(date->year), date->month);
    if(date->day > days_in_month) return false;

    return true;
}

// Get today's date
void get_today(SimpleDate* date) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    date->year = dt.year;
    date->month = dt.month;
    date->day = dt.day;
}

// Check if two dates are equal
bool dates_equal(const SimpleDate* date1, const SimpleDate* date2) {
    return (
        date1->year == date2->year && date1->month == date2->month && date1->day == date2->day);
}

// Compare dates (-1 if date1 < date2, 0 if equal, 1 if date1 > date2)
int compare_dates(const SimpleDate* date1, const SimpleDate* date2) {
    if(date1->year != date2->year) {
        return date1->year < date2->year ? -1 : 1;
    }
    if(date1->month != date2->month) {
        return date1->month < date2->month ? -1 : 1;
    }
    if(date1->day != date2->day) {
        return date1->day < date2->day ? -1 : 1;
    }
    return 0;
}

// Add days to a date
void add_days_to_date(SimpleDate* date, int32_t days) {
    uint32_t timestamp = simple_date_to_timestamp(date);
    timestamp += days * 86400;
    timestamp_to_simple_date(timestamp, date);
}

// Format date as YYYY-MM-DD
void format_date_string(const SimpleDate* date, char* buffer, size_t buffer_size) {
    snprintf(buffer, buffer_size, "%04u-%02u-%02u", date->year, date->month, date->day);
}

// Get confidence visual indicator based on confidence level
const char* get_confidence_indicator(uint8_t confidence_level) {
    if(confidence_level >= 85) {
        return "\u25cf\u25cf\u25cf"; // ●●● High (90%)
    } else if(confidence_level >= 65) {
        return "\u25cf\u25cf\u25cb"; // ●●○ Moderate (70%)
    } else if(confidence_level >= 45) {
        return "\u25cf\u25cb\u25cb"; // ●○○ Low (50%)
    } else {
        return "\u25cb\u25cb\u25cb"; // ○○○ Very Low (30%)
    }
}

// Format date range for uncertain predictions
// Returns true if range should be shown (margin > 3 days)
bool format_date_range(
    const SimpleDate* earliest,
    const SimpleDate* latest,
    char* buffer,
    size_t buffer_size) {
    // Calculate day difference
    int32_t days_earliest = earliest->year * 365 + earliest->month * 30 + earliest->day;
    int32_t days_latest = latest->year * 365 + latest->month * 30 + latest->day;
    int32_t margin = (days_latest - days_earliest) / 2;

    // Only show range if uncertainty is significant (> 3 days margin)
    if(margin <= 3) {
        return false;
    }

    // Format as "range YYYY-MM-DD to YYYY-MM-DD"
    snprintf(
        buffer,
        buffer_size,
        "range %04u-%02u-%02u to %04u-%02u-%02u",
        earliest->year,
        earliest->month,
        earliest->day,
        latest->year,
        latest->month,
        latest->day);

    return true;
}

// Parse date string (YYYY-MM-DD format)
bool parse_date_string(const char* str, SimpleDate* date) {
    if(!str) return false;

    int year, month, day;
    if(sscanf(str, "%d-%d-%d", &year, &month, &day) != 3) {
        return false;
    }

    date->year = (uint16_t)year;
    date->month = (uint8_t)month;
    date->day = (uint8_t)day;

    return is_date_valid(date);
}

// Initialize girl profile with defaults
void girl_profile_init(GirlProfile* profile) {
    memset(profile, 0, sizeof(GirlProfile));
    profile->cycle_length_days = 28;
    profile->period_length_days = 5;
}

// Validate girl profile
bool girl_profile_is_valid(const GirlProfile* profile) {
    if(strlen(profile->name) == 0) return false;
    if(profile->cycle_length_days < 20 || profile->cycle_length_days > 45) return false;
    if(profile->period_length_days < 2 || profile->period_length_days > 10) return false;
    return true;
}

// Initialize cycle event
void cycle_event_init(CycleEvent* event) {
    memset(event, 0, sizeof(CycleEvent));
    get_today(&event->date);
}

// Initialize app settings with defaults
void app_settings_init(AppSettings* settings) {
    memset(settings, 0, sizeof(AppSettings));
    settings->symptom_count = 0;
    settings->pin_enabled = false;
    settings->alert_lead_days = 1; // Default: notify 1 day in advance
}

// Add default symptoms to settings
void app_settings_add_default_symptoms(AppSettings* settings) {
    const char* default_symptoms[] = {
        "cramps",
        "headache",
        "bloating",
        "mood_swings",
        "fatigue",
        "tender_breasts",
        "acne",
        "back_pain",
        "nausea",
        "irritability"};

    settings->symptom_count = 0;
    for(size_t i = 0; i < COUNT_OF(default_symptoms) && i < MAX_SYMPTOMS; i++) {
        strncpy(
            settings->global_symptoms[settings->symptom_count],
            default_symptoms[i],
            MAX_SYMPTOM_NAME_LENGTH - 1);
        settings->symptom_count++;
    }
}
