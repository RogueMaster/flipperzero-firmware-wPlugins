/**
 * @file RDSDspHb1Taps.h
 * @brief Q15 half-band FIR stage 1 (19 taps) for the RDS decimation chain.
 *
 * Cascade position: first half-band after 57 kHz carrier down-mix at 125 kHz input.
 * Decimates 2:1 (one output every two input samples via hb1_phase).
 * Odd-index taps are zero; center tap is 16384 (Q15 unity).
 *
 * rds_hb1_pair_q15[] is indexed by ((offset + 1) >> 1) for symmetric offset pairs;
 * index 0 is unused — the center tap is applied separately in rds_halfband19_q15().
 */
#pragma once

#include <stdint.h>

#include "RDSDspTapsCommon.h"

#define RDS_HB1_TAPS 19U

static const int16_t rds_hb1_q15[RDS_HB1_TAPS] = {
    4, 0, -117, 0, 649, 0, -2335, 0, 9991, 16384, 9991, 0, -2335, 0, 649, 0, -117, 0, 4,
};

static const uint32_t rds_hb1_pair_q15[] = {
    0,
    RDS_PACK_I16_CONST(9991, 9991),
    RDS_PACK_I16_CONST(-2335, -2335),
    RDS_PACK_I16_CONST(649, 649),
    RDS_PACK_I16_CONST(-117, -117),
    RDS_PACK_I16_CONST(4, 4),
};
