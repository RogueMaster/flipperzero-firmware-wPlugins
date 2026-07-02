/**
 * @file RDSDspTapsCommon.h
 * @brief Shared helpers for RDS DSP coefficient headers (SMLAD pair packing).
 */
#pragma once

#include <stdint.h>

/** Pack two Q15 coefficients into one 32-bit word for ARM SMLAD MAC pairs. */
#define RDS_PACK_I16_CONST(lo, hi) \
    ((uint32_t)(uint16_t)(int16_t)(lo) | ((uint32_t)(uint16_t)(int16_t)(hi) << 16U))
