#ifndef DCF77_TIME_H
#define DCF77_TIME_H

#include "dcf77_decode.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * Calendar fields compatible with Flipper DateTime (no Furi dependency).
 * weekday: 1=Monday .. 7=Sunday to match DCF77 / common EU encoding.
 */
typedef struct {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint16_t year;
    uint8_t weekday;
} Dcf77DateTime;

/**
 * Convert German civil time (CET/CEST) to UTC, then to Flipper-local fields
 * using utc_offset_minutes (same convention as AppSettings).
 * second is set to 0 (DCF77 minute boundary).
 */
bool dcf77_civil_to_local(
    const Dcf77CivilTime* civil,
    int16_t utc_offset_minutes,
    Dcf77DateTime* out);

/** True when last successful sync is older than interval_s (or never synced). */
bool dcf77_should_auto_sync(
    uint32_t now_epoch_s,
    uint32_t last_success_epoch_s,
    bool enabled,
    uint32_t interval_s);

#endif /* DCF77_TIME_H */
