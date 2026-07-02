/**
 * @file RDSDspHb3Taps.h
 * @brief Q15 half-band FIR stage 3 (11 taps) for the RDS decimation chain.
 *
 * Cascade position: third half-band after HB2 (~31.25 kHz effective sample rate).
 * Decimates 2:1 relative to the HB2 output grid; combined HB1+HB2+HB3 yield 8:1
 * decimation from the 125 kHz ADC stream to ~15.625 kHz before FIR41.
 * Odd-index taps are zero; center tap is 16384 (Q15 unity).
 *
 * rds_hb3_pair_q15[] is indexed by ((offset + 1) >> 1) for symmetric offset pairs;
 * index 0 is unused — the center tap is applied separately in rds_halfband11_q15().
 */
#pragma once

#include <stdint.h>

#include "RDSDspTapsCommon.h"

#define RDS_HB3_TAPS 11U

static const int16_t rds_hb3_q15[RDS_HB3_TAPS] = {
    131,
    0,
    -1369,
    0,
    10441,
    16384,
    10441,
    0,
    -1369,
    0,
    131,
};

static const uint32_t rds_hb3_pair_q15[] = {
    0,
    RDS_PACK_I16_CONST(10441, 10441),
    RDS_PACK_I16_CONST(-1369, -1369),
    RDS_PACK_I16_CONST(131, 131),
};
