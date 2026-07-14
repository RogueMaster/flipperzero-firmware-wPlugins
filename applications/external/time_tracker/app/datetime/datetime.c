#include "datetime.h"

void datetime_to_string(char* datetime_str, size_t size, const DateTime* datetime) {
    snprintf(
        datetime_str,
        size,
        "%04d-%02d-%02d %02d:%02d:%02d",
        datetime->year,
        datetime->month,
        datetime->day,
        datetime->hour,
        datetime->minute,
        datetime->second);
}

void datetime_to_string_iso8601(char* buffer, size_t size, const DateTime* datetime) {
    snprintf(
        buffer,
        size,
        "%04d-%02d-%02dT%02d:%02d:%02d",
        datetime->year,
        datetime->month,
        datetime->day,
        datetime->hour,
        datetime->minute,
        datetime->second);
}

// Parse ISO 8601 string to DateTime
void string_to_datetime_iso8601(const char* str, DateTime* datetime) {
    int year, month, day, hour, minute, second;
    sscanf(str, "%04d-%02d-%02dT%02d:%02d:%02d", &year, &month, &day, &hour, &minute, &second);
    datetime->year = (uint16_t)year;
    datetime->month = (uint16_t)month;
    datetime->day = (uint16_t)day;
    datetime->hour = (uint16_t)hour;
    datetime->minute = (uint16_t)minute;
    datetime->second = (uint16_t)second;
}

// Calculate the difference in minutes between two DateTimes
int32_t calculate_time_difference_in_minutes(DateTime* start, DateTime* end) {
    // Convert start and end DateTime to UNIX timestamps
    uint32_t start_timestamp = datetime_datetime_to_timestamp(start);
    uint32_t end_timestamp = datetime_datetime_to_timestamp(end);

    // Calculate the difference in seconds
    int32_t difference_seconds = end_timestamp - start_timestamp;

    // Convert the difference from seconds to minutes
    int32_t difference_minutes = difference_seconds / 60;

    return difference_minutes;
}

// Calculate the difference in seconds between two DateTimes
int64_t calculate_time_difference_in_seconds(DateTime* start, DateTime* end) {
    // Convert start and end DateTime to UNIX timestamps in seconds
    uint64_t start_timestamp_s = datetime_datetime_to_timestamp(start);
    uint64_t end_timestamp_s = datetime_datetime_to_timestamp(end);

    // Calculate the difference in seconds
    int64_t difference_seconds = end_timestamp_s - start_timestamp_s;

    return difference_seconds;
}

void format_time_string(char* buffer, size_t size, int32_t total_minutes) {
    int32_t days = total_minutes / (24 * 60);
    total_minutes %= (24 * 60);
    int32_t hours = total_minutes / 60;
    int32_t minutes = total_minutes % 60;

    if(days > 0) {
        snprintf(
            buffer,
            size,
            "%ld day%s %ldh %ldmin",
            (long)days,
            (days > 1) ? "s" : "",
            (long)hours,
            (long)minutes);
    } else if(hours > 0) {
        snprintf(buffer, size, "%ldh %ldmin", (long)hours, (long)minutes);
    } else {
        snprintf(buffer, size, "%ldmin", (long)minutes);
    }
}
