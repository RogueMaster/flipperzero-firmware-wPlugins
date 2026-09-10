#pragma once
#include <furi.h>
typedef struct {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint16_t year;
    uint8_t weekday;
} DateTime;
void furi_hal_rtc_get_datetime(DateTime* dt);
