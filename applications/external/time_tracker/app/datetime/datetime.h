#ifndef DATETIME_H
#define DATETIME_H
#include "../app.h"
#include <datetime/datetime.h>
#include <storage/storage.h>
void datetime_to_string(char* datetime_str, size_t size, const DateTime* datetime);

void datetime_to_string_iso8601(char* buffer, size_t size, const DateTime* datetime);

void string_to_datetime_iso8601(const char* str, DateTime* datetime);
int32_t calculate_time_difference_in_minutes(DateTime* start, DateTime* end);
int64_t calculate_time_difference_in_seconds(DateTime* start, DateTime* end);
void format_time_string(char* buffer, size_t size, int32_t total_minutes);
#endif // DATETIME_H
