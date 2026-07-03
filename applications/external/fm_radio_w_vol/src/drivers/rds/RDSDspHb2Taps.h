/**
 * @file RDSDspHb2Taps.h
 * @brief Q15 half-band FIR stage 2 (31 taps) for the RDS decimation chain.
 *
 * Cascade position: second half-band after HB1 (~62.5 kHz effective sample rate).
 * Decimates 2:1 relative to the HB1 output grid.
 * Odd-index taps are zero; center tap is 16384 (Q15 unity).
 *
 * rds_hb2_pair_q15[] is indexed by ((offset + 1) >> 1) for symmetric offset pairs;
 * index 0 is unused — the center tap is applied separately in rds_halfband31_q15().
 */
#pragma once

#include <stdint.h>

#include "RDSDspTapsCommon.h"

#define RDS_HB2_TAPS 31U

static const int16_t rds_hb2_q15[RDS_HB2_TAPS] = {
    -3,    0, 27,    0, -105, 0, 291,  0, -668, 0, 1401, 0, -3020, 0, 10269, 16384,
    10269, 0, -3020, 0, 1401, 0, -668, 0, 291,  0, -105, 0, 27,    0, -3,
};

static const uint32_t rds_hb2_pair_q15[] = {
    0,
    RDS_PACK_I16_CONST(10269, 10269),
    RDS_PACK_I16_CONST(-3020, -3020),
    RDS_PACK_I16_CONST(1401, 1401),
    RDS_PACK_I16_CONST(-668, -668),
    RDS_PACK_I16_CONST(291, 291),
    RDS_PACK_I16_CONST(-105, -105),
    RDS_PACK_I16_CONST(27, 27),
    RDS_PACK_I16_CONST(-3, -3),
};
