#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <datetime/datetime.h>

// Forward declarations
typedef struct Storage Storage;

// Maximum lengths
#define MAX_NAME_LENGTH         64
#define MAX_NOTES_LENGTH        256
#define MAX_SYMPTOM_NAME_LENGTH 32
#define MAX_SYMPTOMS            20
#define MAX_GIRLS               10
#define MAX_EVENTS              500

// Date structure (simplified, no time)
typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
} SimpleDate;

// Girl profile structure
typedef struct {
    char name[MAX_NAME_LENGTH];
    uint8_t cycle_length_days;
    uint8_t period_length_days;
    bool uses_contraception;
    bool has_pcos;
    bool is_menopausal;
    char notes[MAX_NOTES_LENGTH];
    bool archived;
} GirlProfile;

// Event types
typedef enum {
    EventTypePeriodStart,
    EventTypeSymptom,
} EventType;

// Event structure
typedef struct {
    SimpleDate date;
    EventType type;
    char value[MAX_SYMPTOM_NAME_LENGTH]; // Symptom name or "1" for period_start
} CycleEvent;

// Prediction types
typedef enum {
    PredictionTypePeriodStart,
    PredictionTypePMS,
    PredictionTypePain,
    PredictionTypeOvulation,
    PredictionTypeSymptom,
} PredictionType;

// Cycle regularity classification
typedef enum {
    CYCLE_REGULAR, // CV < 0.15 (within ±3 days typically)
    CYCLE_MODERATE, // CV 0.15-0.25
    CYCLE_IRREGULAR, // CV > 0.25
    CYCLE_CHAOTIC // CV > 0.4 or very erratic patterns
} CycleRegularity;

// Cycle statistics for learning algorithm
typedef struct {
    uint8_t mean_cycle_length; // Average cycle length
    uint8_t std_dev_cycle_length; // Standard deviation (use fixed-point: value/10)
    uint8_t min_cycle_length; // Shortest observed cycle
    uint8_t max_cycle_length; // Longest observed cycle
    uint16_t cycle_variability; // Coefficient of variation * 1000 (fixed-point)
    CycleRegularity regularity; // Regularity classification
    uint8_t num_cycles_analyzed; // Complete cycles (gaps between period starts)
    bool has_sufficient_data; // At least 3 complete cycles (4 period starts)
} CycleStatistics;

// Prediction structure with confidence intervals
typedef struct {
    SimpleDate date; // Expected date (mean prediction)
    SimpleDate earliest_date; // Lower bound (-2σ)
    SimpleDate latest_date; // Upper bound (+2σ)
    uint8_t confidence_level; // 0-100% confidence
    PredictionType type;
    char description[64];
    char girl_name[MAX_NAME_LENGTH];
    uint8_t period_length_days; // For period predictions, store the actual length
} Prediction;

// Settings structure
typedef struct {
    // Note: alarm_hour and alarm_minute removed - FAP apps cannot set RTC alarms
    char global_symptoms[MAX_SYMPTOMS][MAX_SYMPTOM_NAME_LENGTH];
    uint8_t symptom_count;
    bool pin_enabled;
    uint8_t alert_lead_days; // Days in advance to show alerts (default: 1)
} AppSettings;

// Function prototypes

// Date utilities
uint32_t simple_date_to_timestamp(const SimpleDate* date);
void timestamp_to_simple_date(uint32_t timestamp, SimpleDate* date);
int32_t days_between(const SimpleDate* date1, const SimpleDate* date2);
bool is_date_valid(const SimpleDate* date);
void get_today(SimpleDate* date);
bool dates_equal(const SimpleDate* date1, const SimpleDate* date2);
int compare_dates(const SimpleDate* date1, const SimpleDate* date2); // Returns -1, 0, or 1
void add_days_to_date(SimpleDate* date, int32_t days);
void format_date_string(const SimpleDate* date, char* buffer, size_t buffer_size);
bool parse_date_string(const char* str, SimpleDate* date);

// Profile functions
void girl_profile_init(GirlProfile* profile);
bool girl_profile_is_valid(const GirlProfile* profile);

// Event functions
void cycle_event_init(CycleEvent* event);

// Settings functions
void app_settings_init(AppSettings* settings);
void app_settings_add_default_symptoms(AppSettings* settings);

// Cycle statistics functions
void cycle_statistics_init(CycleStatistics* stats);
bool calculate_cycle_statistics(
    Storage* storage,
    const char* girl_name,
    CycleStatistics* stats,
    uint8_t max_cycles); // Typically 12 for last year
uint8_t calculate_adaptive_ovulation_day(const CycleStatistics* stats);
const char* cycle_regularity_to_string(CycleRegularity regularity);

// UI helper functions
const char* get_confidence_indicator(uint8_t confidence_level);
bool format_date_range(
    const SimpleDate* earliest,
    const SimpleDate* latest,
    char* buffer,
    size_t buffer_size);
