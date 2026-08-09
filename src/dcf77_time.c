#include "dcf77_time.h"

#include "internet_time.h"

#include <stddef.h>

static bool dcf77_is_leap(uint16_t year) {
    return ((year % 4u) == 0u && (year % 100u) != 0u) || ((year % 400u) == 0u);
}

static uint8_t dcf77_days_in_month(uint16_t year, uint8_t month) {
    static const uint8_t mdays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if(month < 1 || month > 12) {
        return 0;
    }
    if(month == 2 && dcf77_is_leap(year)) {
        return 29;
    }
    return mdays[month - 1];
}

/* Convert civil Y-M-D H:M to minutes since 2000-01-01 00:00 (UTC-ish math). */
static bool dcf77_to_epoch_minutes(
    uint16_t year,
    uint8_t month,
    uint8_t day,
    uint8_t hour,
    uint8_t minute,
    int64_t* out_minutes) {
    if(year < 2000 || month < 1 || month > 12 || day < 1 || hour > 23 || minute > 59) {
        return false;
    }
    if(day > dcf77_days_in_month(year, month)) {
        return false;
    }

    int64_t days = 0;
    for(uint16_t y = 2000; y < year; y++) {
        days += dcf77_is_leap(y) ? 366 : 365;
    }
    for(uint8_t m = 1; m < month; m++) {
        days += dcf77_days_in_month(year, m);
    }
    days += (int64_t)day - 1;
    *out_minutes = days * 24 * 60 + (int64_t)hour * 60 + (int64_t)minute;
    return true;
}

static bool dcf77_from_epoch_minutes(int64_t minutes, Dcf77DateTime* out) {
    if(minutes < 0) {
        return false;
    }
    int64_t days = minutes / (24 * 60);
    int64_t tod = minutes % (24 * 60);
    out->hour = (uint8_t)(tod / 60);
    out->minute = (uint8_t)(tod % 60);
    out->second = 0;

    uint16_t year = 2000;
    for(;;) {
        const int64_t ydays = dcf77_is_leap(year) ? 366 : 365;
        if(days < ydays) {
            break;
        }
        days -= ydays;
        year++;
        if(year > 2099) {
            return false;
        }
    }
    uint8_t month = 1;
    for(;;) {
        const uint8_t dim = dcf77_days_in_month(year, month);
        if(days < dim) {
            break;
        }
        days -= dim;
        month++;
        if(month > 12) {
            return false;
        }
    }
    out->year = year;
    out->month = month;
    out->day = (uint8_t)(days + 1);

    /* Sakamoto weekday: 0=Sunday .. 6=Saturday → map to DCF77 1=Mon..7=Sun */
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    int y = (int)year;
    const int m = (int)month;
    const int d = (int)out->day;
    if(m < 3) {
        y -= 1;
    }
    const int w = (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
    out->weekday = (w == 0) ? 7 : (uint8_t)w;
    return true;
}

bool dcf77_civil_to_local(
    const Dcf77CivilTime* civil,
    int16_t utc_offset_minutes,
    Dcf77DateTime* out) {
    if(!civil || !out) {
        return false;
    }
    if(!internet_time_offset_valid(utc_offset_minutes)) {
        return false;
    }

    int64_t civil_minutes = 0;
    if(!dcf77_to_epoch_minutes(
           civil->year, civil->month, civil->day, civil->hour, civil->minute, &civil_minutes)) {
        return false;
    }

    const int64_t zone_minutes = civil->cest ? 120 : 60;
    const int64_t utc_minutes = civil_minutes - zone_minutes;
    const int64_t local_minutes = utc_minutes + (int64_t)utc_offset_minutes;
    return dcf77_from_epoch_minutes(local_minutes, out);
}

bool dcf77_should_auto_sync(
    uint32_t now_epoch_s,
    uint32_t last_success_epoch_s,
    bool enabled,
    uint32_t interval_s) {
    if(!enabled) {
        return false;
    }
    if(last_success_epoch_s == 0) {
        return true;
    }
    if(now_epoch_s < last_success_epoch_s) {
        return true; /* clock went backwards; resync */
    }
    return (now_epoch_s - last_success_epoch_s) >= interval_s;
}
