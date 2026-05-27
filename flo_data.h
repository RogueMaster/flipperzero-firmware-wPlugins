#pragma once

#include <stdint.h>
#include <stdbool.h>

#define FLO_MAX_PERIODS 24
#define FLO_DEFAULT_CYCLE_LENGTH 28
#define FLO_DEFAULT_PERIOD_DURATION 5
#define FLO_SAVE_PATH APP_DATA_PATH("flo_data.bin")

#define FLO_DATA_MAGIC 0x464C4F32 /* "FLO2" - v2 with weighted avg + stddev */

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
} FloDate;

typedef struct {
    FloDate start_date;
    uint8_t duration_days; /* actual duration of this period */
} FloPeriodEntry;

typedef struct {
    uint32_t magic;
    uint8_t cycle_length;          /* average cycle length in days */
    uint8_t default_period_duration; /* default/average period duration */
    uint8_t period_count;          /* number of logged periods */
    uint8_t cycle_stddev;          /* standard deviation of cycle lengths */
    bool auto_cycle_length;        /* true = auto-calculated, false = manual override */
    FloPeriodEntry periods[FLO_MAX_PERIODS]; /* most recent first */
} FloData;

/* Date utility functions */
bool flo_date_is_valid(FloDate date);
bool flo_date_equals(FloDate a, FloDate b);
int32_t flo_date_diff_days(FloDate from, FloDate to);
FloDate flo_date_add_days(FloDate date, int32_t days);
uint8_t flo_days_in_month(uint16_t year, uint8_t month);
uint8_t flo_day_of_week(FloDate date); /* 0=Sunday */
FloDate flo_date_today(void);
const char* flo_month_name(uint8_t month);
const char* flo_month_name_short(uint8_t month);

/* Data functions */
void flo_data_init(FloData* data);
bool flo_data_save(FloData* data);
bool flo_data_load(FloData* data);
void flo_data_add_period(FloData* data, FloDate start, uint8_t duration);
void flo_data_remove_last_period(FloData* data);

/* Prediction functions */
FloDate flo_predict_next_period(FloData* data);
FloDate flo_predict_ovulation(FloData* data);
FloDate flo_fertile_window_start(FloData* data);
FloDate flo_fertile_window_end(FloData* data);
int32_t flo_current_cycle_day(FloData* data);
FloDate flo_predict_nth_period(FloData* data, uint8_t n); /* predict n-th future period (1=next) */
bool flo_is_cycle_irregular(FloData* data); /* true if stddev > 4 days */
bool flo_is_in_period(FloData* data, FloDate date);
bool flo_is_in_fertile_window(FloData* data, FloDate date);
bool flo_is_predicted_period(FloData* data, FloDate date);
