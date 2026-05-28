#include "flo_data.h"
#include <furi.h>
#include <storage/storage.h>
#include <furi_hal_rtc.h>
#include <string.h>

/* ── Date utilities ─────────────────────────────────────── */

static bool is_leap_year(uint16_t year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

uint8_t flo_days_in_month(uint16_t year, uint8_t month) {
    static const uint8_t days[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if(month == 2 && is_leap_year(year)) return 29;
    if(month >= 1 && month <= 12) return days[month];
    return 0;
}

bool flo_date_is_valid(FloDate date) {
    if(date.year < 2020 || date.year > 2099) return false;
    if(date.month < 1 || date.month > 12) return false;
    if(date.day < 1 || date.day > flo_days_in_month(date.year, date.month)) return false;
    return true;
}

bool flo_date_equals(FloDate a, FloDate b) {
    return a.year == b.year && a.month == b.month && a.day == b.day;
}

/* Convert date to a day number (simple Julian Day Number approximation) */
static int32_t date_to_days(FloDate d) {
    int32_t y = d.year;
    int32_t m = d.month;
    int32_t day = d.day;
    /* Adjust for months Jan/Feb */
    if(m <= 2) {
        y -= 1;
        m += 12;
    }
    return 365 * y + y / 4 - y / 100 + y / 400 + (153 * (m - 3) + 2) / 5 + day - 306;
}

static FloDate days_to_date(int32_t g) {
    int32_t y = (10000 * (int64_t)g + 14780) / 3652425;
    int32_t doy = g - (365 * y + y / 4 - y / 100 + y / 400);
    if(doy < 0) {
        y -= 1;
        doy = g - (365 * y + y / 4 - y / 100 + y / 400);
    }
    int32_t mi = (100 * doy + 52) / 3060;
    int32_t month = (mi + 2) % 12 + 1;
    y = y + (mi + 2) / 12;

    FloDate result;
    result.year = (uint16_t)y;
    result.month = (uint8_t)month;
    result.day = (uint8_t)(doy - (mi * 306 + 5) / 10 + 1);
    return result;
}

int32_t flo_date_diff_days(FloDate from, FloDate to) {
    return date_to_days(to) - date_to_days(from);
}

FloDate flo_date_add_days(FloDate date, int32_t days) {
    return days_to_date(date_to_days(date) + days);
}

uint8_t flo_day_of_week(FloDate date) {
    /* Tomohiko Sakamoto's algorithm, returns 0=Sunday */
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    int y = date.year;
    int m = date.month;
    if(m < 3) y -= 1;
    return (uint8_t)((y + y / 4 - y / 100 + y / 400 + t[m - 1] + date.day) % 7);
}

FloDate flo_date_today(void) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    FloDate d;
    d.year = dt.year;
    d.month = dt.month;
    d.day = dt.day;
    return d;
}

const char* flo_month_name(uint8_t month) {
    static const char* names[] = {
        "",
        "January",
        "February",
        "March",
        "April",
        "May",
        "June",
        "July",
        "August",
        "September",
        "October",
        "November",
        "December"};
    if(month >= 1 && month <= 12) return names[month];
    return "???";
}

const char* flo_month_name_short(uint8_t month) {
    static const char* names[] = {
        "", "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    if(month >= 1 && month <= 12) return names[month];
    return "???";
}

/* ── Data management ────────────────────────────────────── */

void flo_data_init(FloData* data) {
    memset(data, 0, sizeof(FloData));
    data->magic = FLO_DATA_MAGIC;
    data->cycle_length = FLO_DEFAULT_CYCLE_LENGTH;
    data->default_period_duration = FLO_DEFAULT_PERIOD_DURATION;
    data->period_count = 0;
    data->cycle_stddev = 0;
    data->auto_cycle_length = true;
}

bool flo_data_save(FloData* data) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, FLO_SAVE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        uint16_t written = storage_file_write(file, data, sizeof(FloData));
        success = (written == sizeof(FloData));
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return success;
}

bool flo_data_load(FloData* data) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, FLO_SAVE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint16_t read = storage_file_read(file, data, sizeof(FloData));
        if(read == sizeof(FloData) && data->magic == FLO_DATA_MAGIC) {
            success = true;
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    if(!success) {
        flo_data_init(data);
    }
    return success;
}

/* Recalculate cycle stats using weighted average (recent cycles weighted more) */
static void flo_recalculate_cycle_stats(FloData* data) {
    if(!data->auto_cycle_length || data->period_count < 2) return;

    int32_t diffs[FLO_MAX_PERIODS];
    uint8_t count = 0;
    for(uint8_t i = 0; i < data->period_count - 1; i++) {
        int32_t diff = flo_date_diff_days(
            data->periods[i + 1].start_date, data->periods[i].start_date);
        if(diff > 0 && diff < 60) {
            diffs[count] = diff;
            count++;
        }
    }
    if(count == 0) return;

    /* Weighted average: weight = count - i (most recent gets highest weight) */
    int32_t weighted_sum = 0;
    int32_t weight_total = 0;
    for(uint8_t i = 0; i < count; i++) {
        int32_t w = count - i; /* most recent (i=0) gets highest weight */
        weighted_sum += diffs[i] * w;
        weight_total += w;
    }
    data->cycle_length = (uint8_t)(weighted_sum / weight_total);

    /* Standard deviation */
    int32_t mean = weighted_sum / weight_total;
    int32_t var_sum = 0;
    for(uint8_t i = 0; i < count; i++) {
        int32_t delta = diffs[i] - mean;
        var_sum += delta * delta;
    }
    /* Integer sqrt approximation */
    int32_t variance = var_sum / count;
    uint8_t stddev = 0;
    while((int32_t)(stddev + 1) * (stddev + 1) <= variance) stddev++;
    data->cycle_stddev = stddev;
}

void flo_data_add_period(FloData* data, FloDate start, uint8_t duration) {
    /* Check for duplicate: reject if start date matches any existing entry */
    for(uint8_t i = 0; i < data->period_count; i++) {
        if(flo_date_equals(data->periods[i].start_date, start)) {
            /* Update duration of existing entry instead */
            data->periods[i].duration_days = duration;
            flo_recalculate_cycle_stats(data);
            return;
        }
    }

    /* Shift existing entries down if full */
    if(data->period_count >= FLO_MAX_PERIODS) {
        data->period_count = FLO_MAX_PERIODS - 1;
    }
    /* Shift all entries to make room at index 0 (most recent first) */
    for(int i = data->period_count; i > 0; i--) {
        data->periods[i] = data->periods[i - 1];
    }
    data->periods[0].start_date = start;
    data->periods[0].duration_days = duration;
    data->period_count++;

    flo_recalculate_cycle_stats(data);
}

void flo_data_remove_last_period(FloData* data) {
    if(data->period_count > 0) {
        data->period_count--;
    }
}

/* ── Predictions ────────────────────────────────────────── */

FloDate flo_predict_nth_period(FloData* data, uint8_t n) {
    if(data->period_count == 0 || n == 0) {
        return flo_date_today();
    }
    return flo_date_add_days(data->periods[0].start_date, (int32_t)data->cycle_length * n);
}

FloDate flo_predict_next_period(FloData* data) {
    return flo_predict_nth_period(data, 1);
}

FloDate flo_predict_ovulation(FloData* data) {
    FloDate next = flo_predict_next_period(data);
    return flo_date_add_days(next, -14);
}

FloDate flo_fertile_window_start(FloData* data) {
    FloDate ovulation = flo_predict_ovulation(data);
    return flo_date_add_days(ovulation, -5);
}

FloDate flo_fertile_window_end(FloData* data) {
    FloDate ovulation = flo_predict_ovulation(data);
    return flo_date_add_days(ovulation, 1);
}

int32_t flo_current_cycle_day(FloData* data) {
    if(data->period_count == 0) return -1;
    FloDate today = flo_date_today();
    int32_t diff = flo_date_diff_days(data->periods[0].start_date, today);
    if(diff < 0) return -1;
    return diff + 1;
}

bool flo_is_cycle_irregular(FloData* data) {
    return data->cycle_stddev > 4;
}

bool flo_is_in_period(FloData* data, FloDate date) {
    for(uint8_t p = 0; p < data->period_count; p++) {
        int32_t diff = flo_date_diff_days(data->periods[p].start_date, date);
        if(diff >= 0 && diff < data->periods[p].duration_days) {
            return true;
        }
    }
    return false;
}

bool flo_is_in_fertile_window(FloData* data, FloDate date) {
    if(data->period_count == 0) return false;
    for(uint8_t n = 1; n <= 3; n++) {
        FloDate next = flo_predict_nth_period(data, n);
        FloDate ov = flo_date_add_days(next, -14);
        FloDate fw_start = flo_date_add_days(ov, -5);
        FloDate fw_end = flo_date_add_days(ov, 1);
        int32_t from_start = flo_date_diff_days(fw_start, date);
        int32_t from_end = flo_date_diff_days(date, fw_end);
        if(from_start >= 0 && from_end >= 0) return true;
    }
    return false;
}

bool flo_is_predicted_period(FloData* data, FloDate date) {
    if(data->period_count == 0) return false;
    for(uint8_t n = 1; n <= 3; n++) {
        FloDate predicted = flo_predict_nth_period(data, n);
        int32_t diff = flo_date_diff_days(predicted, date);
        if(diff >= 0 && diff < data->default_period_duration) return true;
    }
    return false;
}
