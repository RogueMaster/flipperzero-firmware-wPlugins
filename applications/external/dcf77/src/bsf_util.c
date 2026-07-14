#include "bsf_util.h"

#include <stdbool.h>
#include "pair_timecode.h"

#define BSF_FRAME_SECONDS 60U

void set_bsf_timecode(
    RadioClockPulse* dest,
    uint8_t minute,
    uint8_t hour,
    uint8_t day,
    uint8_t month,
    uint8_t year,
    uint8_t weekday,
    bool leap_second_warning,
    bool dst_active) {
    /* BSF weekdays are encoded as 0-6, with Sunday represented as 0. */
    weekday = (uint8_t)(weekday % 7U);

    pair_timecode_fill_frame(dest, BSF_FRAME_SECONDS, RadioClockPulsePair00);

    dest[39] = RadioClockPulseMarker;
    dest[59] = RadioClockPulseMarker;

    /* Taiwan's draft/public examples leave the public-information section zeroed. */
    dest[40] = RadioClockPulsePair01;
    pair_timecode_set_weighted(dest, 41U, minute, 32U, 16U);
    pair_timecode_set_weighted(dest, 42U, minute, 8U, 4U);
    pair_timecode_set_weighted(dest, 43U, minute, 2U, 1U);
    pair_timecode_set_weighted(dest, 44U, hour, 16U, 8U);
    pair_timecode_set_weighted(dest, 45U, hour, 4U, 2U);
    pair_timecode_set_bits(
        dest, 46U, (hour & 1U) != 0U, pair_timecode_even_parity_bits(dest, 41U, 45U));
    pair_timecode_set_weighted(dest, 47U, day, 16U, 8U);
    pair_timecode_set_weighted(dest, 48U, day, 4U, 2U);
    pair_timecode_set_bits(dest, 49U, (day & 1U) != 0U, (weekday & 4U) != 0U);
    pair_timecode_set_weighted(dest, 50U, weekday, 2U, 1U);
    pair_timecode_set_weighted(dest, 51U, month, 8U, 4U);
    pair_timecode_set_weighted(dest, 52U, month, 2U, 1U);
    pair_timecode_set_weighted(dest, 53U, year, 64U, 32U);
    pair_timecode_set_weighted(dest, 54U, year, 16U, 8U);
    pair_timecode_set_weighted(dest, 55U, year, 4U, 2U);
    pair_timecode_set_bits(
        dest, 56U, (year & 1U) != 0U, pair_timecode_even_parity_bits(dest, 47U, 55U));
    pair_timecode_set_bits(dest, 57U, false, leap_second_warning);
    pair_timecode_set_bits(dest, 58U, false, dst_active);
}
