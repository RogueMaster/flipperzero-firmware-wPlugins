#include "bpc_util.h"

#include <stdbool.h>

#include "pair_timecode.h"

#define BPC_FRAME_SECONDS 60U
#define BPC_BLOCK_SECONDS 20U

static void bpc_set_block(
    RadioClockPulse* frame,
    uint8_t start_second,
    uint8_t block_index,
    uint8_t minute,
    uint8_t hour,
    uint8_t day,
    uint8_t month,
    uint8_t year_since_2000,
    uint8_t weekday) {
    bool p1;
    bool p2;

    pair_timecode_set_symbol(frame, start_second + 0U, RadioClockPulseMarker);
    pair_timecode_set_symbol(frame, start_second + 1U, pair_timecode_digit_to_pulse(block_index));
    pair_timecode_set_symbol(frame, start_second + 2U, RadioClockPulsePair00);
    pair_timecode_set_weighted(frame, start_second + 3U, (uint8_t)(hour % 12U), 8U, 4U);
    pair_timecode_set_weighted(frame, start_second + 4U, (uint8_t)(hour % 12U), 2U, 1U);
    pair_timecode_set_weighted(frame, start_second + 5U, minute, 32U, 16U);
    pair_timecode_set_weighted(frame, start_second + 6U, minute, 8U, 4U);
    pair_timecode_set_weighted(frame, start_second + 7U, minute, 2U, 1U);
    pair_timecode_set_weighted(frame, start_second + 8U, weekday, 0U, 4U);
    pair_timecode_set_weighted(frame, start_second + 9U, weekday, 2U, 1U);

    p1 = pair_timecode_even_parity_bits(frame, start_second + 1U, start_second + 9U);
    pair_timecode_set_bits(frame, start_second + 10U, hour >= 12U, p1);

    pair_timecode_set_symbol(frame, start_second + 11U, RadioClockPulsePair00);
    pair_timecode_set_weighted(frame, start_second + 12U, day, 8U, 4U);
    pair_timecode_set_weighted(frame, start_second + 13U, day, 2U, 1U);
    pair_timecode_set_weighted(frame, start_second + 14U, month, 8U, 4U);
    pair_timecode_set_weighted(frame, start_second + 15U, month, 2U, 1U);
    pair_timecode_set_weighted(frame, start_second + 16U, year_since_2000, 32U, 16U);
    pair_timecode_set_weighted(frame, start_second + 17U, year_since_2000, 8U, 4U);
    pair_timecode_set_weighted(frame, start_second + 18U, year_since_2000, 2U, 1U);

    p2 = pair_timecode_even_parity_bits(frame, start_second + 10U, start_second + 18U);
    pair_timecode_set_bits(frame, start_second + 19U, (year_since_2000 & 64U) != 0U, p2);
}

void set_bpc_timecode(
    RadioClockPulse* dest,
    uint8_t minute,
    uint8_t hour,
    uint8_t day,
    uint8_t month,
    uint8_t year_since_2000,
    uint8_t weekday) {
    pair_timecode_fill_frame(dest, BPC_FRAME_SECONDS, RadioClockPulsePair00);

    bpc_set_block(dest, 0U, 0U, minute, hour, day, month, year_since_2000, weekday);
    bpc_set_block(dest, 20U, 1U, minute, hour, day, month, year_since_2000, weekday);
    bpc_set_block(dest, 40U, 2U, minute, hour, day, month, year_since_2000, weekday);
}
