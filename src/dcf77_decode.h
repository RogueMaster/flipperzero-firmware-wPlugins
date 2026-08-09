#ifndef DCF77_DECODE_H
#define DCF77_DECODE_H

#include <stdbool.h>
#include <stdint.h>

/** Number of payload bits in one DCF77 minute (seconds 0..58). */
#define DCF77_FRAME_BITS 59

typedef struct {
    uint8_t minute; /* 0-59 */
    uint8_t hour; /* 0-23 */
    uint8_t day; /* 1-31 */
    uint8_t month; /* 1-12 */
    uint16_t year; /* full year, e.g. 2026 */
    uint8_t weekday; /* 1=Monday .. 7=Sunday (DCF77 encoding) */
    bool cest; /* true if CEST (UTC+2), false if CET (UTC+1) */
} Dcf77CivilTime;

/**
 * Decode a complete 59-bit DCF77 frame (bits[0]..bits[58], each 0 or 1).
 * Validates start bits, CET/CEST exclusivity, BCD ranges, and parity bits.
 * Returns true and fills out on success.
 */
bool dcf77_decode_frame(const uint8_t bits[DCF77_FRAME_BITS], Dcf77CivilTime* out);

#endif /* DCF77_DECODE_H */
