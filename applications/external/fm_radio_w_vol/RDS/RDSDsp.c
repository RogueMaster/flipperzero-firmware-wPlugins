#ifdef HOST_BUILD
#define _POSIX_C_SOURCE 200809L
#endif

/* DSP hot path: override -Os from build system; release-quality, target only.
 * Hosted/non-ARM builds keep default optimisation. */
#if !defined(HOST_BUILD) && (defined(__arm__) || defined(__thumb__))
#pragma GCC optimize("O3")
#endif

#include "RDSDsp.h"

#include <limits.h>
#include <string.h>

#ifdef HOST_BUILD
#include <time.h>
#elif defined(RDS_RUNTIME_ENABLE_STAGE_CYCLE_COUNTERS)
#include <stm32wbxx.h>
#endif

#define RDS_BITRATE_Q16                       0x04A38000UL /* 1187.5 * 65536 */
#define RDS_PILOT_TRACK_TOLERANCE_HZ          2U
#define RDS_CARRIER_MIN_HZ                    (RDS_CARRIER_NOMINAL_HZ - (RDS_PILOT_TRACK_TOLERANCE_HZ * 3U))
#define RDS_CARRIER_MAX_HZ                    (RDS_CARRIER_NOMINAL_HZ + (RDS_PILOT_TRACK_TOLERANCE_HZ * 3U))
#define RDS_PILOT_TRACK_MIN_HZ                (RDS_PILOT_HZ - RDS_PILOT_TRACK_TOLERANCE_HZ)
#define RDS_PILOT_TRACK_MAX_HZ                (RDS_PILOT_HZ + RDS_PILOT_TRACK_TOLERANCE_HZ)
#define RDS_CARRIER_MANUAL_OFFSET_MIN_CENTIHZ (-600)
#define RDS_CARRIER_MANUAL_OFFSET_MAX_CENTIHZ (600)
#define RDS_RUNTIME_DECIMATION_FACTOR         8U
#define RDS_FILTER_INPUT_SHIFT                4U
/* FIR41 lowpass 3.8 kHz @ 15.625 kHz, Q15 unity-gain. Tap quantization headroom
 * already keeps |acc| within int16_t after >>15, so no extra output shift. */
#define RDS_FIR41_OUTPUT_SHIFT                0U
/* The symbol sampler runs after HB1 -> HB2 -> HB3 -> FIR41. The initial phase
 * must therefore compensate the fractional group delay of the full cascade,
 * not just the last FIR stage. With the current filters the total delay is
 * 219 input samples, which is ~2.0805 symbols at 125 kHz / 1187.5 bps; only
 * the fractional remainder (~0.0805 symbol) should be preloaded. */
#define RDS_HB1_GROUP_DELAY_INPUT_SAMPLES     (((RDS_HB1_TAPS - 1U) / 2U))
#define RDS_HB2_GROUP_DELAY_INPUT_SAMPLES     ((((RDS_HB2_TAPS - 1U) / 2U) * 2U))
#define RDS_HB3_GROUP_DELAY_INPUT_SAMPLES     ((((RDS_HB3_TAPS - 1U) / 2U) * 4U))
#define RDS_FIR41_GROUP_DELAY_INPUT_SAMPLES \
    ((((RDS_EN50067_FIR_TAPS - 1U) / 2U) * RDS_RUNTIME_DECIMATION_FACTOR))
#define RDS_TOTAL_GROUP_DELAY_INPUT_SAMPLES                                  \
    (RDS_HB1_GROUP_DELAY_INPUT_SAMPLES + RDS_HB2_GROUP_DELAY_INPUT_SAMPLES + \
     RDS_HB3_GROUP_DELAY_INPUT_SAMPLES + RDS_FIR41_GROUP_DELAY_INPUT_SAMPLES)
#define RDS_PACK_I16_CONST(lo, hi) \
    ((uint32_t)(uint16_t)(int16_t)(lo) | ((uint32_t)(uint16_t)(int16_t)(hi) << 16U))

#if defined(HOST_BUILD) && defined(RDS_SANDBOX_RUNTIME_PARITY)
#ifndef RDS_SANDBOX_FAST_PILOT_ERROR
#define RDS_SANDBOX_FAST_PILOT_ERROR 1
#endif
#ifndef RDS_SANDBOX_DISABLE_EXTRA_DIAGNOSTICS
#define RDS_SANDBOX_DISABLE_EXTRA_DIAGNOSTICS 1
#endif
#endif

#if defined(HOST_BUILD) && defined(RDS_SANDBOX_DISABLE_STAGE_TIMING)
#define RDS_STAGE_TIMING_ENABLED 0
#else
#define RDS_STAGE_TIMING_ENABLED 1
#endif

/* Pilot tracking now runs on every HB1-output cycle (decimation 2:1 of the
 * input rate). The legacy RDS_PILOT_SAMPLE_STRIDE / RDS_SANDBOX_PILOT_STRIDE
 * pre-AB sub-sampling knobs were removed when pilot tracking moved to the
 * post-HB1 grid. */

#if defined(HOST_BUILD) && defined(RDS_SANDBOX_FAST_PILOT_ERROR)
#define RDS_FAST_PILOT_ERROR 1
#elif !defined(HOST_BUILD)
#define RDS_FAST_PILOT_ERROR 1
#else
#define RDS_FAST_PILOT_ERROR 0
#endif

#if defined(HOST_BUILD) && defined(RDS_SANDBOX_FAST_CONFIDENCE)
#define RDS_FAST_CONFIDENCE 1
#else
#define RDS_FAST_CONFIDENCE 0
#endif

#if defined(HOST_BUILD)
#if defined(RDS_SANDBOX_DISABLE_EXTRA_DIAGNOSTICS)
#define RDS_EXTRA_DIAGNOSTICS_ENABLED 0
#else
#define RDS_EXTRA_DIAGNOSTICS_ENABLED 1
#endif
#else
#if defined(RDS_RUNTIME_ENABLE_EXTRA_DIAGNOSTICS)
#define RDS_EXTRA_DIAGNOSTICS_ENABLED 1
#else
#define RDS_EXTRA_DIAGNOSTICS_ENABLED 0
#endif
#if defined(RDS_RUNTIME_ENABLE_STAGE_CYCLE_COUNTERS)
#define RDS_RUNTIME_STAGE_CYCLE_COUNTERS_ENABLED 1
#else
#define RDS_RUNTIME_STAGE_CYCLE_COUNTERS_ENABLED 0
#endif
#endif

#include "RDSDspFir41Taps.h"

#define RDS_NCO_LUT_BITS 7U
#define RDS_NCO_LUT_SIZE (1U << RDS_NCO_LUT_BITS)

static const uint32_t rds_cos_sin_q15[RDS_NCO_LUT_SIZE] = {
    RDS_PACK_I16_CONST(32767, 0),       RDS_PACK_I16_CONST(32728, 1608),
    RDS_PACK_I16_CONST(32609, 3212),    RDS_PACK_I16_CONST(32413, 4808),
    RDS_PACK_I16_CONST(32138, 6393),    RDS_PACK_I16_CONST(31785, 7962),
    RDS_PACK_I16_CONST(31356, 9512),    RDS_PACK_I16_CONST(30852, 11039),
    RDS_PACK_I16_CONST(30273, 12539),   RDS_PACK_I16_CONST(29622, 14010),
    RDS_PACK_I16_CONST(28899, 15447),   RDS_PACK_I16_CONST(28106, 16846),
    RDS_PACK_I16_CONST(27245, 18205),   RDS_PACK_I16_CONST(26318, 19519),
    RDS_PACK_I16_CONST(25329, 20787),   RDS_PACK_I16_CONST(24279, 22005),
    RDS_PACK_I16_CONST(23170, 23170),   RDS_PACK_I16_CONST(22005, 24279),
    RDS_PACK_I16_CONST(20787, 25329),   RDS_PACK_I16_CONST(19519, 26318),
    RDS_PACK_I16_CONST(18205, 27245),   RDS_PACK_I16_CONST(16846, 28106),
    RDS_PACK_I16_CONST(15447, 28899),   RDS_PACK_I16_CONST(14010, 29622),
    RDS_PACK_I16_CONST(12539, 30273),   RDS_PACK_I16_CONST(11039, 30852),
    RDS_PACK_I16_CONST(9512, 31356),    RDS_PACK_I16_CONST(7962, 31785),
    RDS_PACK_I16_CONST(6393, 32138),    RDS_PACK_I16_CONST(4808, 32413),
    RDS_PACK_I16_CONST(3212, 32609),    RDS_PACK_I16_CONST(1608, 32728),
    RDS_PACK_I16_CONST(0, 32767),       RDS_PACK_I16_CONST(-1608, 32728),
    RDS_PACK_I16_CONST(-3212, 32609),   RDS_PACK_I16_CONST(-4808, 32413),
    RDS_PACK_I16_CONST(-6393, 32138),   RDS_PACK_I16_CONST(-7962, 31785),
    RDS_PACK_I16_CONST(-9512, 31356),   RDS_PACK_I16_CONST(-11039, 30852),
    RDS_PACK_I16_CONST(-12539, 30273),  RDS_PACK_I16_CONST(-14010, 29622),
    RDS_PACK_I16_CONST(-15447, 28899),  RDS_PACK_I16_CONST(-16846, 28106),
    RDS_PACK_I16_CONST(-18205, 27245),  RDS_PACK_I16_CONST(-19519, 26318),
    RDS_PACK_I16_CONST(-20787, 25329),  RDS_PACK_I16_CONST(-22005, 24279),
    RDS_PACK_I16_CONST(-23170, 23170),  RDS_PACK_I16_CONST(-24279, 22005),
    RDS_PACK_I16_CONST(-25329, 20787),  RDS_PACK_I16_CONST(-26318, 19519),
    RDS_PACK_I16_CONST(-27245, 18205),  RDS_PACK_I16_CONST(-28106, 16846),
    RDS_PACK_I16_CONST(-28899, 15447),  RDS_PACK_I16_CONST(-29622, 14010),
    RDS_PACK_I16_CONST(-30273, 12539),  RDS_PACK_I16_CONST(-30852, 11039),
    RDS_PACK_I16_CONST(-31356, 9512),   RDS_PACK_I16_CONST(-31785, 7962),
    RDS_PACK_I16_CONST(-32138, 6393),   RDS_PACK_I16_CONST(-32413, 4808),
    RDS_PACK_I16_CONST(-32609, 3212),   RDS_PACK_I16_CONST(-32728, 1608),
    RDS_PACK_I16_CONST(-32767, 0),      RDS_PACK_I16_CONST(-32728, -1608),
    RDS_PACK_I16_CONST(-32609, -3212),  RDS_PACK_I16_CONST(-32413, -4808),
    RDS_PACK_I16_CONST(-32138, -6393),  RDS_PACK_I16_CONST(-31785, -7962),
    RDS_PACK_I16_CONST(-31356, -9512),  RDS_PACK_I16_CONST(-30852, -11039),
    RDS_PACK_I16_CONST(-30273, -12539), RDS_PACK_I16_CONST(-29622, -14010),
    RDS_PACK_I16_CONST(-28899, -15447), RDS_PACK_I16_CONST(-28106, -16846),
    RDS_PACK_I16_CONST(-27245, -18205), RDS_PACK_I16_CONST(-26318, -19519),
    RDS_PACK_I16_CONST(-25329, -20787), RDS_PACK_I16_CONST(-24279, -22005),
    RDS_PACK_I16_CONST(-23170, -23170), RDS_PACK_I16_CONST(-22005, -24279),
    RDS_PACK_I16_CONST(-20787, -25329), RDS_PACK_I16_CONST(-19519, -26318),
    RDS_PACK_I16_CONST(-18205, -27245), RDS_PACK_I16_CONST(-16846, -28106),
    RDS_PACK_I16_CONST(-15447, -28899), RDS_PACK_I16_CONST(-14010, -29622),
    RDS_PACK_I16_CONST(-12539, -30273), RDS_PACK_I16_CONST(-11039, -30852),
    RDS_PACK_I16_CONST(-9512, -31356),  RDS_PACK_I16_CONST(-7962, -31785),
    RDS_PACK_I16_CONST(-6393, -32138),  RDS_PACK_I16_CONST(-4808, -32413),
    RDS_PACK_I16_CONST(-3212, -32609),  RDS_PACK_I16_CONST(-1608, -32728),
    RDS_PACK_I16_CONST(0, -32767),      RDS_PACK_I16_CONST(1608, -32728),
    RDS_PACK_I16_CONST(3212, -32609),   RDS_PACK_I16_CONST(4808, -32413),
    RDS_PACK_I16_CONST(6393, -32138),   RDS_PACK_I16_CONST(7962, -31785),
    RDS_PACK_I16_CONST(9512, -31356),   RDS_PACK_I16_CONST(11039, -30852),
    RDS_PACK_I16_CONST(12539, -30273),  RDS_PACK_I16_CONST(14010, -29622),
    RDS_PACK_I16_CONST(15447, -28899),  RDS_PACK_I16_CONST(16846, -28106),
    RDS_PACK_I16_CONST(18205, -27245),  RDS_PACK_I16_CONST(19519, -26318),
    RDS_PACK_I16_CONST(20787, -25329),  RDS_PACK_I16_CONST(22005, -24279),
    RDS_PACK_I16_CONST(23170, -23170),  RDS_PACK_I16_CONST(24279, -22005),
    RDS_PACK_I16_CONST(25329, -20787),  RDS_PACK_I16_CONST(26318, -19519),
    RDS_PACK_I16_CONST(27245, -18205),  RDS_PACK_I16_CONST(28106, -16846),
    RDS_PACK_I16_CONST(28899, -15447),  RDS_PACK_I16_CONST(29622, -14010),
    RDS_PACK_I16_CONST(30273, -12539),  RDS_PACK_I16_CONST(30852, -11039),
    RDS_PACK_I16_CONST(31356, -9512),   RDS_PACK_I16_CONST(31785, -7962),
    RDS_PACK_I16_CONST(32138, -6393),   RDS_PACK_I16_CONST(32413, -4808),
    RDS_PACK_I16_CONST(32609, -3212),   RDS_PACK_I16_CONST(32728, -1608),
};

static const int16_t rds_hb1_q15[RDS_HB1_TAPS] = {
    4, 0, -117, 0, 649, 0, -2335, 0, 9991, 16384, 9991, 0, -2335, 0, 649, 0, -117, 0, 4,
};

static const int16_t rds_hb2_q15[RDS_HB2_TAPS] = {
    -3,    0, 27,    0, -105, 0, 291,  0, -668, 0, 1401, 0, -3020, 0, 10269, 16384,
    10269, 0, -3020, 0, 1401, 0, -668, 0, 291,  0, -105, 0, 27,    0, -3,
};

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

/* Indexed by ((offset + 1) >> 1) for offset = 1, 3, 5, 7, 9: indices 1..5.
 * Index 0 is unused (offset 0 is the centre tap, handled separately). */
static const uint32_t rds_hb1_pair_q15[] = {
    0,
    RDS_PACK_I16_CONST(9991, 9991),
    RDS_PACK_I16_CONST(-2335, -2335),
    RDS_PACK_I16_CONST(649, 649),
    RDS_PACK_I16_CONST(-117, -117),
    RDS_PACK_I16_CONST(4, 4),
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

static const uint32_t rds_hb3_pair_q15[] = {
    0,
    RDS_PACK_I16_CONST(10441, 10441),
    RDS_PACK_I16_CONST(-1369, -1369),
    RDS_PACK_I16_CONST(131, 131),
};

static inline uint32_t rds_abs_i32(int32_t value) {
    uint32_t uvalue = (uint32_t)value;
    uint32_t sign_mask = (uint32_t) - (int32_t)(uvalue >> 31U);
    return (uvalue ^ sign_mask) - sign_mask;
}

static inline uint32_t rds_ema_u32(uint32_t avg, uint32_t sample, uint8_t shift) {
    return (uint32_t)((int32_t)avg + (((int32_t)sample - (int32_t)avg) >> shift));
}

static inline int32_t rds_clamp_i32(int32_t value, int32_t min_value, int32_t max_value) {
    if(value < min_value) return min_value;
    if(value > max_value) return max_value;
    return value;
}

static inline int16_t rds_sat_i16(int32_t value) {
    if(value > INT16_MAX) return INT16_MAX;
    if(value < INT16_MIN) return INT16_MIN;
    return (int16_t)value;
}

static inline uint32_t rds_pack_i16(int16_t lo, int16_t hi) {
    return (uint32_t)(uint16_t)lo | ((uint32_t)(uint16_t)hi << 16U);
}

static inline int16_t rds_packed_cos_q15(uint32_t packed_cos_sin) {
    return (int16_t)(packed_cos_sin & 0xFFFFU);
}

static inline int16_t rds_packed_sin_q15(uint32_t packed_cos_sin) {
    return (int16_t)(packed_cos_sin >> 16U);
}

static inline int16_t rds_scale_hp_to_q15(int32_t hp_q8) {
    return rds_sat_i16(hp_q8 >> RDS_FILTER_INPUT_SHIFT);
}

static inline int32_t rds_smlad_q15(uint32_t packed_samples, uint32_t packed_coeffs, int32_t acc) {
#if defined(__arm__) || defined(__thumb__)
    __asm volatile("smlad %0, %1, %2, %0" : "+r"(acc) : "r"(packed_samples), "r"(packed_coeffs));
    return acc;
#else
    int16_t sample_lo = (int16_t)(packed_samples & 0xFFFFU);
    int16_t sample_hi = (int16_t)(packed_samples >> 16U);
    int16_t coeff_lo = (int16_t)(packed_coeffs & 0xFFFFU);
    int16_t coeff_hi = (int16_t)(packed_coeffs >> 16U);
    return acc + ((int32_t)sample_lo * coeff_lo) + ((int32_t)sample_hi * coeff_hi);
#endif
}

static inline uint32_t rds_phase_step_q32(uint32_t freq_hz, uint32_t sample_rate_hz) {
    if(sample_rate_hz == 0U) {
        return 0U;
    }

    return (uint32_t)((((uint64_t)freq_hz) << 32U) / sample_rate_hz);
}

static inline uint32_t rds_pilot_tracking_sample_rate_hz(uint32_t sample_rate_hz) {
    return sample_rate_hz / 2U;
}

static uint32_t rds_carrier_step_from_pilot_q32(uint32_t pilot_step_q32, uint32_t sample_rate_hz);

static uint32_t
    rds_carrier_offset_step_from_centihz_q32(int16_t offset_centihz, uint32_t sample_rate_hz) {
    if(sample_rate_hz == 0U || offset_centihz == 0) {
        return 0U;
    }

    uint32_t abs_centihz = (uint32_t)((offset_centihz < 0) ? -offset_centihz : offset_centihz);
    uint64_t step = (((uint64_t)abs_centihz) << 32U) / ((uint64_t)sample_rate_hz * 100ULL);
    if(step > 0xFFFFFFFFULL) {
        step = 0xFFFFFFFFULL;
    }
    return (uint32_t)step;
}

static uint32_t rds_apply_manual_carrier_offset_step_q32(
    uint32_t carrier_step_q32,
    int16_t offset_centihz,
    uint32_t sample_rate_hz) {
    uint32_t min_step = rds_phase_step_q32(RDS_CARRIER_MIN_HZ, sample_rate_hz);
    uint32_t max_step = rds_phase_step_q32(RDS_CARRIER_MAX_HZ, sample_rate_hz);
    uint32_t offset_step_q32 =
        rds_carrier_offset_step_from_centihz_q32(offset_centihz, sample_rate_hz);
    uint32_t result = carrier_step_q32;

    if(offset_centihz < 0) {
        result = (offset_step_q32 > result) ? 0U : (result - offset_step_q32);
    } else if(offset_centihz > 0) {
        result = result + offset_step_q32;
    }

    if(result < min_step) {
        return min_step;
    }
    if(result > max_step) {
        return max_step;
    }
    return result;
}

static uint32_t
    rds_carrier_step_q32_with_manual_offset(const RDSDsp* dsp, uint32_t pilot_step_q32) {
    if(!dsp) {
        return 0U;
    }

    uint32_t base_step = rds_carrier_step_from_pilot_q32(pilot_step_q32, dsp->sample_rate_hz);
    return rds_apply_manual_carrier_offset_step_q32(
        base_step, dsp->carrier_manual_offset_centihz, dsp->sample_rate_hz);
}

static uint32_t rds_carrier_step_from_pilot_q32(uint32_t pilot_step_q32, uint32_t sample_rate_hz) {
    /* Pilot is mixed at sample_rate/2 (decimated by 2); carrier mixer runs at full
     * sample_rate. Carrier is 3x pilot frequency, so:
     *   pilot_step_q32 = (f_p << 32) / (fs/2) = 2*(f_p<<32)/fs
     *   carrier_step_q32 = (3*f_p << 32) / fs = (3/2) * pilot_step_q32
     */
    uint64_t carrier_step = ((uint64_t)pilot_step_q32 * 3ULL) >> 1U;
    uint32_t min_step = rds_phase_step_q32(RDS_CARRIER_MIN_HZ, sample_rate_hz);
    uint32_t max_step = rds_phase_step_q32(RDS_CARRIER_MAX_HZ, sample_rate_hz);

    if(carrier_step > (uint64_t)max_step) {
        return max_step;
    }
    if(carrier_step < (uint64_t)min_step) {
        return min_step;
    }
    return (uint32_t)carrier_step;
}

static inline uint8_t rds_phase_index_q32(uint32_t phase_q32, uint8_t table_size) {
    return (uint8_t)((((phase_q32 >> 16U) * (uint32_t)table_size)) >> 16U);
}

static inline uint32_t rds_scale_filter_level_q8(uint32_t level) {
    if(level > (UINT32_MAX >> RDS_FILTER_INPUT_SHIFT)) {
        return UINT32_MAX;
    }
    return level << RDS_FILTER_INPUT_SHIFT;
}

#ifndef HOST_BUILD
static void rds_runtime_cycle_counter_init(RDSDsp* dsp) {
    if(!dsp) {
        return;
    }

    memset(&dsp->runtime_profile, 0, sizeof(dsp->runtime_profile));
#if RDS_RUNTIME_STAGE_CYCLE_COUNTERS_ENABLED
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    dsp->runtime_profile.cycle_counter_available =
        ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) != 0U) ? 1U : 0U;
#else
    dsp->runtime_profile.cycle_counter_available = 0U;
#endif
}

static inline uint32_t rds_runtime_cycles_now(const RDSDsp* dsp) {
#if RDS_RUNTIME_STAGE_CYCLE_COUNTERS_ENABLED
    if(dsp && dsp->runtime_profile.cycle_counter_available) {
        return DWT->CYCCNT;
    }
#else
    (void)dsp;
#endif
    return 0U;
}

static inline void rds_runtime_cycles_add(uint64_t* total, uint32_t start, uint32_t end) {
    if(total) {
        *total += (uint32_t)(end - start);
    }
}
#endif

static inline int32_t
    rds_phase_error_q15_fast(int32_t prev_i, int32_t prev_q, int32_t curr_i, int32_t curr_q) {
    int32_t pi = prev_i >> 8U;
    int32_t pq = prev_q >> 8U;
    int32_t ci = curr_i >> 8U;
    int32_t cq = curr_q >> 8U;
    int32_t dot = (pi * ci) + (pq * cq);
    int32_t cross = (pi * cq) - (pq * ci);
    uint32_t abs_dot = rds_abs_i32(dot);
    uint32_t abs_cross = rds_abs_i32(cross);
    uint32_t magnitude = (abs_dot > abs_cross) ? abs_dot : abs_cross;

    while(magnitude > 0x3FFFU) {
        abs_dot >>= 1U;
        abs_cross >>= 1U;
        cross >>= 1U;
        magnitude >>= 1U;
    }

    uint32_t denom = abs_dot + abs_cross + 1U;
    return (int32_t)((cross << 15U) / (int32_t)denom);
}

static inline int32_t
    rds_symbol_dot_fast_q31(int32_t symbol_i, int32_t symbol_q, int32_t prev_i, int32_t prev_q) {
    return ((symbol_i >> 8U) * (prev_i >> 8U)) + ((symbol_q >> 8U) * (prev_q >> 8U));
}

static inline void rds_hist_store_q15(int16_t* hist, uint8_t index, uint8_t len, int16_t sample) {
    hist[index] = sample;
    hist[index + len] = sample;
}

static inline uint8_t rds_hist_advance_head(uint8_t head, uint8_t len) {
    uint8_t index = head;
    index++;
    if(index >= len) {
        index = 0U;
    }
    return index;
}

static inline void rds_hist_write_q15(int16_t* hist, uint8_t* head, uint8_t len, int16_t sample) {
    uint8_t index = *head;
    hist[index] = sample;
    hist[index + len] = sample;
    *head = rds_hist_advance_head(index, len);
}

static inline const int16_t* rds_hist_window_q15(const int16_t* hist, uint8_t head) {
    return &hist[head];
}

static inline int16_t rds_halfband19_q15(const int16_t* window) {
    int32_t acc = (int32_t)rds_hb1_q15[9] * window[9];

    for(uint8_t offset = 1U; offset <= 9U; offset += 2U) {
        uint32_t packed_samples = rds_pack_i16(window[9U + offset], window[9U - offset]);
        acc = rds_smlad_q15(packed_samples, rds_hb1_pair_q15[(offset + 1U) >> 1U], acc);
    }
    acc += (acc >= 0) ? (1L << 14U) : -(1L << 14U);
    return rds_sat_i16(acc >> 15U);
}

static inline int16_t rds_halfband31_q15(const int16_t* window) {
    int32_t acc = (int32_t)rds_hb2_q15[15] * window[15];

    for(uint8_t offset = 1U; offset <= 15U; offset += 2U) {
        uint32_t packed_samples = rds_pack_i16(window[15U + offset], window[15U - offset]);
        acc = rds_smlad_q15(packed_samples, rds_hb2_pair_q15[(offset + 1U) >> 1U], acc);
    }
    acc += (acc >= 0) ? (1L << 14U) : -(1L << 14U);
    return rds_sat_i16(acc >> 15U);
}

static inline int16_t rds_halfband11_q15(const int16_t* window) {
    int32_t acc = (int32_t)rds_hb3_q15[5] * window[5];

    for(uint8_t offset = 1U; offset <= 5U; offset += 2U) {
        uint32_t packed_samples = rds_pack_i16(window[5U + offset], window[5U - offset]);
        acc = rds_smlad_q15(packed_samples, rds_hb3_pair_q15[(offset + 1U) >> 1U], acc);
    }
    acc += (acc >= 0) ? (1L << 14U) : -(1L << 14U);
    return rds_sat_i16(acc >> 15U);
}

static inline int16_t rds_fir41_apply_q15(const int16_t* window) {
    int32_t acc = (int32_t)rds_fir41_q15[20] * window[20];

    for(uint8_t offset = 1U; offset <= 20U; offset++) {
        uint32_t packed_samples = rds_pack_i16(window[20U + offset], window[20U - offset]);
        acc = rds_smlad_q15(packed_samples, rds_fir41_pair_q15[offset - 1U], acc);
    }
    acc += (acc >= 0) ? (1L << 14U) : -(1L << 14U);
    return rds_sat_i16(acc >> 15U);
}

static inline uint32_t rds_initial_symbol_phase_q16(uint32_t period_q16) {
    if(period_q16 == 0U) {
        return 0U;
    }

    return (uint32_t)((((uint64_t)RDS_TOTAL_GROUP_DELAY_INPUT_SAMPLES) << 16U) % period_q16);
}

#ifdef HOST_BUILD
#if RDS_STAGE_TIMING_ENABLED
static inline uint64_t rds_host_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ((uint64_t)ts.tv_sec * 1000000000ULL) + (uint64_t)ts.tv_nsec;
}
#endif

void rds_dsp_profile_reset(RDSDsp* dsp) {
    if(!dsp) {
        return;
    }

    memset(&dsp->profile, 0, sizeof(dsp->profile));
}

void rds_dsp_profile_get(const RDSDsp* dsp, RdsDspProfile* out_profile) {
    if(!dsp || !out_profile) {
        return;
    }

    *out_profile = dsp->profile;
}
#endif

uint32_t rds_dsp_get_carrier_hz_q8(const RDSDsp* dsp) {
    if(!dsp || dsp->sample_rate_hz == 0U) {
        return 0U;
    }

    return (uint32_t)((((uint64_t)dsp->carrier_step_q32 * (uint64_t)dsp->sample_rate_hz) << 8U) >>
                      32U);
}

void rds_dsp_set_carrier_hz(RDSDsp* dsp, uint32_t carrier_hz) {
    if(!dsp || dsp->sample_rate_hz == 0U) {
        return;
    }

    carrier_hz = (uint32_t)rds_clamp_i32(
        (int32_t)carrier_hz, (int32_t)RDS_CARRIER_MIN_HZ, (int32_t)RDS_CARRIER_MAX_HZ);
    uint32_t pilot_sample_rate_hz = rds_pilot_tracking_sample_rate_hz(dsp->sample_rate_hz);
    dsp->pilot_nominal_step_q32 = rds_phase_step_q32(RDS_PILOT_HZ, pilot_sample_rate_hz);
    dsp->pilot_min_step_q32 = rds_phase_step_q32(RDS_PILOT_TRACK_MIN_HZ, pilot_sample_rate_hz);
    dsp->pilot_max_step_q32 = rds_phase_step_q32(RDS_PILOT_TRACK_MAX_HZ, pilot_sample_rate_hz);
    dsp->pilot_step_q32 = rds_phase_step_q32(carrier_hz / 3U, pilot_sample_rate_hz);
    if(dsp->pilot_step_q32 < dsp->pilot_min_step_q32) {
        dsp->pilot_step_q32 = dsp->pilot_min_step_q32;
    } else if(dsp->pilot_step_q32 > dsp->pilot_max_step_q32) {
        dsp->pilot_step_q32 = dsp->pilot_max_step_q32;
    }
    dsp->carrier_manual_offset_centihz = 0;
    dsp->carrier_step_q32 = rds_carrier_step_q32_with_manual_offset(dsp, dsp->pilot_step_q32);
    dsp->carrier_phase_q32 = 0U;
    dsp->pilot_phase_q32 = 0U;
    dsp->pilot_error_avg_q15 = 0;
    dsp->pilot_i_lpf_state = 0;
    dsp->pilot_q_lpf_state = 0;
    dsp->pilot_prev_i_lpf_state = 0;
    dsp->pilot_prev_q_lpf_state = 0;
    dsp->pilot_update_div = 0U;
}

void rds_dsp_set_manual_carrier_offset_centihz(RDSDsp* dsp, int16_t offset_centihz) {
    if(!dsp) {
        return;
    }

    if(offset_centihz < RDS_CARRIER_MANUAL_OFFSET_MIN_CENTIHZ) {
        offset_centihz = RDS_CARRIER_MANUAL_OFFSET_MIN_CENTIHZ;
    } else if(offset_centihz > RDS_CARRIER_MANUAL_OFFSET_MAX_CENTIHZ) {
        offset_centihz = RDS_CARRIER_MANUAL_OFFSET_MAX_CENTIHZ;
    }

    dsp->carrier_manual_offset_centihz = offset_centihz;
    if(dsp->sample_rate_hz > 0U) {
        dsp->carrier_step_q32 = rds_carrier_step_q32_with_manual_offset(dsp, dsp->pilot_step_q32);
    }
}

int16_t rds_dsp_get_manual_carrier_offset_centihz(const RDSDsp* dsp) {
    if(!dsp) {
        return 0;
    }
    return dsp->carrier_manual_offset_centihz;
}

void rds_dsp_set_symbol_callback(RDSDsp* dsp, RdsDspSymbolCallback callback, void* context) {
    if(!dsp) {
        return;
    }
    dsp->symbol_callback = callback;
    dsp->symbol_callback_context = context;
}

void rds_dsp_init(RDSDsp* dsp, uint32_t sample_rate_hz) {
    if(!dsp) return;

    dsp->sample_rate_hz = sample_rate_hz;
    dsp->decim_factor = RDS_RUNTIME_DECIMATION_FACTOR;
    dsp->decim_phase = 0U;
    dsp->decim_step_q16 = (uint32_t)dsp->decim_factor << 16U;
    dsp->symbol_phase_q16 = 0U;
    dsp->carrier_phase_q32 = 0U;
    dsp->pilot_phase_q32 = 0U;
    dsp->pilot_step_q32 = 0U;
    dsp->pilot_nominal_step_q32 = 0U;
    dsp->pilot_min_step_q32 = 0U;
    dsp->pilot_max_step_q32 = 0U;
    dsp->pilot_error_avg_q15 = 0;
    dsp->pilot_i_lpf_state = 0;
    dsp->pilot_q_lpf_state = 0;
    dsp->pilot_prev_i_lpf_state = 0;
    dsp->pilot_prev_q_lpf_state = 0;
    dsp->pilot_update_div = 0U;
    dsp->dc_estimate_q8 = 0;
    memset(dsp->hb1_i_hist, 0, sizeof(dsp->hb1_i_hist));
    memset(dsp->hb1_q_hist, 0, sizeof(dsp->hb1_q_hist));
    dsp->hb1_head = 0U;
    dsp->hb1_phase = 0U;
    memset(dsp->hb2_i_hist, 0, sizeof(dsp->hb2_i_hist));
    memset(dsp->hb2_q_hist, 0, sizeof(dsp->hb2_q_hist));
    dsp->hb2_head = 0U;
    dsp->hb2_phase = 0U;
    memset(dsp->hb3_i_hist, 0, sizeof(dsp->hb3_i_hist));
    memset(dsp->hb3_q_hist, 0, sizeof(dsp->hb3_q_hist));
    dsp->hb3_head = 0U;
    dsp->hb3_phase = 0U;
    memset(dsp->en50067_i_hist, 0, sizeof(dsp->en50067_i_hist));
    memset(dsp->en50067_q_hist, 0, sizeof(dsp->en50067_q_hist));
    dsp->en50067_head = 0U;
    dsp->i_integrator = 0;
    dsp->q_integrator = 0;
    dsp->half_i_integrator = 0;
    dsp->half_q_integrator = 0;
    dsp->prev_i_symbol = 0;
    dsp->prev_q_symbol = 0;
    dsp->prev_symbol_valid = false;
    dsp->prev_half_i_symbol = 0;
    dsp->prev_half_q_symbol = 0;
    dsp->prev_half_symbol_valid = false;
    dsp->first_half_i_symbol = 0;
    dsp->first_half_q_symbol = 0;
    dsp->half_symbol_phase = 0U;
    dsp->symbol_count = 0U;
    dsp->symbol_confidence_avg_q16 = 0U;
    dsp->block_symbol_count_last = 0U;
    dsp->block_confidence_last_q16 = 0U;
    dsp->block_confidence_avg_q16 = 0U;
    dsp->corrected_confidence_avg_q16 = 0U;
    dsp->uncorrectable_confidence_avg_q16 = 0U;
    dsp->block_corrected_count_last = 0U;
    dsp->block_uncorrectable_count_last = 0U;
    dsp->block_corrected_confidence_last_q16 = 0U;
    dsp->block_uncorrectable_confidence_last_q16 = 0U;
    dsp->pilot_level_q8 = 0U;
    dsp->rds_band_level_q8 = 0U;
    dsp->avg_abs_hp_q8 = 0U;
    dsp->avg_vector_mag_q8 = 0U;
    dsp->avg_decision_mag_q8 = 0U;
    dsp->cached_symbol_period_q16 = 0U;
    dsp->carrier_manual_offset_centihz = 0;
    dsp->symbol_callback = NULL;
    dsp->symbol_callback_context = NULL;
#ifdef HOST_BUILD
    dsp->bit_log = NULL;
    dsp->bit_log_count = 0;
    dsp->bit_log_capacity = 0;
    memset(&dsp->profile, 0, sizeof(dsp->profile));
#else
    rds_runtime_cycle_counter_init(dsp);
#endif

    if(sample_rate_hz == 0U) {
        dsp->samples_per_symbol_q16 = 0U;
        dsp->carrier_step_q32 = 0U;
        dsp->pilot_step_q32 = 0U;
    } else {
        uint64_t numerator = ((uint64_t)sample_rate_hz) << 32U;
        dsp->samples_per_symbol_q16 = (uint32_t)(numerator / RDS_BITRATE_Q16);
        uint32_t pilot_sample_rate_hz = rds_pilot_tracking_sample_rate_hz(sample_rate_hz);
        dsp->pilot_nominal_step_q32 = rds_phase_step_q32(RDS_PILOT_HZ, pilot_sample_rate_hz);
        dsp->pilot_min_step_q32 = rds_phase_step_q32(RDS_PILOT_TRACK_MIN_HZ, pilot_sample_rate_hz);
        dsp->pilot_max_step_q32 = rds_phase_step_q32(RDS_PILOT_TRACK_MAX_HZ, pilot_sample_rate_hz);
        dsp->pilot_step_q32 = dsp->pilot_nominal_step_q32;
        dsp->carrier_step_q32 = rds_carrier_step_q32_with_manual_offset(dsp, dsp->pilot_step_q32);
        dsp->cached_symbol_period_q16 = dsp->samples_per_symbol_q16;
        dsp->symbol_phase_q16 = rds_initial_symbol_phase_q16(dsp->cached_symbol_period_q16);
    }
}

void rds_dsp_reset(RDSDsp* dsp) {
    if(!dsp) return;

    dsp->symbol_phase_q16 = rds_initial_symbol_phase_q16(dsp->cached_symbol_period_q16);
    dsp->decim_phase = 0U;
    dsp->carrier_phase_q32 = 0U;
    dsp->carrier_step_q32 =
        rds_carrier_step_q32_with_manual_offset(dsp, dsp->pilot_nominal_step_q32);
    dsp->pilot_phase_q32 = 0U;
    dsp->pilot_step_q32 = dsp->pilot_nominal_step_q32;
    dsp->pilot_error_avg_q15 = 0;
    dsp->pilot_i_lpf_state = 0;
    dsp->pilot_q_lpf_state = 0;
    dsp->pilot_prev_i_lpf_state = 0;
    dsp->pilot_prev_q_lpf_state = 0;
    dsp->pilot_update_div = 0U;
    dsp->dc_estimate_q8 = 0;
    memset(dsp->hb1_i_hist, 0, sizeof(dsp->hb1_i_hist));
    memset(dsp->hb1_q_hist, 0, sizeof(dsp->hb1_q_hist));
    dsp->hb1_head = 0U;
    dsp->hb1_phase = 0U;
    memset(dsp->hb2_i_hist, 0, sizeof(dsp->hb2_i_hist));
    memset(dsp->hb2_q_hist, 0, sizeof(dsp->hb2_q_hist));
    dsp->hb2_head = 0U;
    dsp->hb2_phase = 0U;
    memset(dsp->hb3_i_hist, 0, sizeof(dsp->hb3_i_hist));
    memset(dsp->hb3_q_hist, 0, sizeof(dsp->hb3_q_hist));
    dsp->hb3_head = 0U;
    dsp->hb3_phase = 0U;
    memset(dsp->en50067_i_hist, 0, sizeof(dsp->en50067_i_hist));
    memset(dsp->en50067_q_hist, 0, sizeof(dsp->en50067_q_hist));
    dsp->en50067_head = 0U;
    dsp->i_integrator = 0;
    dsp->q_integrator = 0;
    dsp->half_i_integrator = 0;
    dsp->half_q_integrator = 0;
    dsp->prev_i_symbol = 0;
    dsp->prev_q_symbol = 0;
    dsp->prev_symbol_valid = false;
    dsp->prev_half_i_symbol = 0;
    dsp->prev_half_q_symbol = 0;
    dsp->prev_half_symbol_valid = false;
    dsp->first_half_i_symbol = 0;
    dsp->first_half_q_symbol = 0;
    dsp->half_symbol_phase = 0U;
    dsp->symbol_count = 0U;
    dsp->symbol_confidence_avg_q16 = 0U;
    dsp->block_symbol_count_last = 0U;
    dsp->block_confidence_last_q16 = 0U;
    dsp->block_confidence_avg_q16 = 0U;
    dsp->corrected_confidence_avg_q16 = 0U;
    dsp->uncorrectable_confidence_avg_q16 = 0U;
    dsp->block_corrected_count_last = 0U;
    dsp->block_uncorrectable_count_last = 0U;
    dsp->block_corrected_confidence_last_q16 = 0U;
    dsp->block_uncorrectable_confidence_last_q16 = 0U;
    dsp->pilot_level_q8 = 0U;
    dsp->rds_band_level_q8 = 0U;
    dsp->avg_abs_hp_q8 = 0U;
    dsp->avg_vector_mag_q8 = 0U;
    dsp->avg_decision_mag_q8 = 0U;
    dsp->cached_symbol_period_q16 = dsp->samples_per_symbol_q16;
#ifndef HOST_BUILD
    rds_runtime_cycle_counter_init(dsp);
#endif
}

void rds_dsp_process_u16_samples(
    RDSDsp* dsp,
    RDSCore* core,
    const uint16_t* samples,
    size_t count,
    uint16_t adc_midpoint) {
    if(!dsp || !core || !samples || count == 0U || dsp->samples_per_symbol_q16 == 0U) {
        return;
    }

#if RDS_EXTRA_DIAGNOSTICS_ENABLED
    uint32_t block_symbol_count = 0U;
    uint64_t block_confidence_sum_q16 = 0U;
#endif
    uint64_t block_abs_hp_sum_q8 = 0U;
    uint64_t block_pilot_mag_sum_q8 = 0U;
    uint64_t block_rds_band_sum_q8 = 0U;
    uint32_t block_pilot_mag_count = 0U;
    uint32_t block_rds_band_count = 0U;
    const uint32_t corrected_before_block = core->corrected_blocks;
    const uint32_t uncorrectable_before_block = core->uncorrectable_blocks;

    for(size_t i = 0; i < count; i++) {
#if defined(HOST_BUILD) && RDS_STAGE_TIMING_ENABLED
        uint64_t stage_start_ns = rds_host_now_ns();
#elif !defined(HOST_BUILD) && RDS_RUNTIME_STAGE_CYCLE_COUNTERS_ENABLED
        uint32_t stage_start_cycles = rds_runtime_cycles_now(dsp);
#endif
        int32_t centered = (int32_t)samples[i] - (int32_t)adc_midpoint;
        int32_t centered_q8 = centered << 8;

        dsp->dc_estimate_q8 += (centered_q8 - dsp->dc_estimate_q8) >> 6;
        int32_t hp = centered_q8 - dsp->dc_estimate_q8;
        int16_t hp_q15 = rds_scale_hp_to_q15(hp);
        block_abs_hp_sum_q8 += rds_abs_i32(hp);

#if defined(HOST_BUILD) && RDS_STAGE_TIMING_ENABLED
        dsp->profile.dc_pilot_ns += rds_host_now_ns() - stage_start_ns;
        dsp->profile.input_samples++;
        stage_start_ns = rds_host_now_ns();
#elif !defined(HOST_BUILD) && RDS_RUNTIME_STAGE_CYCLE_COUNTERS_ENABLED
        rds_runtime_cycles_add(
            &dsp->runtime_profile.dc_pilot_cycles,
            stage_start_cycles,
            rds_runtime_cycles_now(dsp));
        dsp->runtime_profile.input_samples++;
        stage_start_cycles = rds_runtime_cycles_now(dsp);
#endif

        uint8_t carrier_index = (uint8_t)(dsp->carrier_phase_q32 >> (32U - RDS_NCO_LUT_BITS));
        uint32_t carrier_cos_sin = rds_cos_sin_q15[carrier_index];
        int16_t mixed_i = (int16_t)(((int32_t)hp_q15 * rds_packed_cos_q15(carrier_cos_sin)) >> 15);
        int16_t mixed_q =
            (int16_t)(-(((int32_t)hp_q15 * rds_packed_sin_q15(carrier_cos_sin)) >> 15));
        dsp->carrier_phase_q32 += dsp->carrier_step_q32;

#if defined(HOST_BUILD) && RDS_STAGE_TIMING_ENABLED
        dsp->profile.carrier_mix_ns += rds_host_now_ns() - stage_start_ns;
        stage_start_ns = rds_host_now_ns();
#elif !defined(HOST_BUILD) && RDS_RUNTIME_STAGE_CYCLE_COUNTERS_ENABLED
        rds_runtime_cycles_add(
            &dsp->runtime_profile.carrier_mix_cycles,
            stage_start_cycles,
            rds_runtime_cycles_now(dsp));
        stage_start_cycles = rds_runtime_cycles_now(dsp);
#endif

        /* Pilot tracking runs at sample_rate/2 (decimated). HB1 attenuates 19 kHz after
         * the carrier mix at 57 kHz, so the pilot is observed directly on hp_q15 before
         * any decimation, gated by hb1_phase parity to halve the per-block cost. */
#if defined(HOST_BUILD) && RDS_STAGE_TIMING_ENABLED
        uint64_t pilot_stage_start_ns = rds_host_now_ns();
#elif !defined(HOST_BUILD) && RDS_RUNTIME_STAGE_CYCLE_COUNTERS_ENABLED
        uint32_t pilot_stage_start_cycles = rds_runtime_cycles_now(dsp);
#endif
        if(dsp->hb1_phase == 0U) {
            uint8_t pilot_index = (uint8_t)(dsp->pilot_phase_q32 >> (32U - RDS_NCO_LUT_BITS));
            uint32_t pilot_cos_sin = rds_cos_sin_q15[pilot_index];
            int32_t pilot_i = ((int32_t)hp_q15 * rds_packed_cos_q15(pilot_cos_sin)) >> 15;
            int32_t pilot_q = -(((int32_t)hp_q15 * rds_packed_sin_q15(pilot_cos_sin)) >> 15);
            dsp->pilot_phase_q32 += dsp->pilot_step_q32;
            dsp->pilot_i_lpf_state += (pilot_i - dsp->pilot_i_lpf_state) >> 4;
            dsp->pilot_q_lpf_state += (pilot_q - dsp->pilot_q_lpf_state) >> 4;
            block_pilot_mag_sum_q8 +=
                rds_abs_i32(dsp->pilot_i_lpf_state) + rds_abs_i32(dsp->pilot_q_lpf_state);
            block_pilot_mag_count++;
            dsp->pilot_update_div++;
            if(dsp->pilot_update_div >= 32U) {
                int32_t phase_error_q15 = 0;

#if RDS_FAST_PILOT_ERROR
                phase_error_q15 = rds_phase_error_q15_fast(
                    dsp->pilot_prev_i_lpf_state,
                    dsp->pilot_prev_q_lpf_state,
                    dsp->pilot_i_lpf_state,
                    dsp->pilot_q_lpf_state);
#else
                int64_t dot =
                    ((int64_t)dsp->pilot_prev_i_lpf_state * (int64_t)dsp->pilot_i_lpf_state) +
                    ((int64_t)dsp->pilot_prev_q_lpf_state * (int64_t)dsp->pilot_q_lpf_state);
                int64_t cross =
                    ((int64_t)dsp->pilot_prev_i_lpf_state * (int64_t)dsp->pilot_q_lpf_state) -
                    ((int64_t)dsp->pilot_prev_q_lpf_state * (int64_t)dsp->pilot_i_lpf_state);
                uint64_t abs_dot = (uint64_t)((dot < 0) ? -dot : dot);
                uint64_t abs_cross = (uint64_t)((cross < 0) ? -cross : cross);

                if((abs_dot + abs_cross) > 0U) {
                    int64_t denom = (int64_t)(abs_dot + abs_cross + 1U);
                    phase_error_q15 = (int32_t)((cross << 15U) / denom);
                }
#endif

                dsp->pilot_error_avg_q15 += (phase_error_q15 - dsp->pilot_error_avg_q15) >> 6;

                int32_t step_nudge_q32 =
                    rds_clamp_i32(dsp->pilot_error_avg_q15 << 1U, -131072, 131072);
                int64_t pilot_step =
                    (int64_t)dsp->pilot_nominal_step_q32 + (int64_t)step_nudge_q32;
                if(pilot_step < (int64_t)dsp->pilot_min_step_q32) {
                    pilot_step = (int64_t)dsp->pilot_min_step_q32;
                } else if(pilot_step > (int64_t)dsp->pilot_max_step_q32) {
                    pilot_step = (int64_t)dsp->pilot_max_step_q32;
                }
                dsp->pilot_step_q32 = (uint32_t)pilot_step;
                dsp->carrier_step_q32 =
                    rds_carrier_step_q32_with_manual_offset(dsp, dsp->pilot_step_q32);

                dsp->pilot_prev_i_lpf_state = dsp->pilot_i_lpf_state;
                dsp->pilot_prev_q_lpf_state = dsp->pilot_q_lpf_state;
                dsp->pilot_update_div = 0U;
            }
        }
#if defined(HOST_BUILD) && RDS_STAGE_TIMING_ENABLED
        dsp->profile.dc_pilot_ns += rds_host_now_ns() - pilot_stage_start_ns;
        stage_start_ns = rds_host_now_ns();
#elif !defined(HOST_BUILD) && RDS_RUNTIME_STAGE_CYCLE_COUNTERS_ENABLED
        rds_runtime_cycles_add(
            &dsp->runtime_profile.dc_pilot_cycles,
            pilot_stage_start_cycles,
            rds_runtime_cycles_now(dsp));
        stage_start_cycles = rds_runtime_cycles_now(dsp);
#endif

        uint8_t hb1_write_index = dsp->hb1_head;
        rds_hist_store_q15(dsp->hb1_i_hist, hb1_write_index, RDS_HB1_TAPS, mixed_i);
        rds_hist_store_q15(dsp->hb1_q_hist, hb1_write_index, RDS_HB1_TAPS, mixed_q);
        dsp->hb1_head = rds_hist_advance_head(hb1_write_index, RDS_HB1_TAPS);
        dsp->hb1_phase ^= 1U;
        if(dsp->hb1_phase != 0U) {
#if defined(HOST_BUILD) && RDS_STAGE_TIMING_ENABLED
            dsp->profile.hb1_ns += rds_host_now_ns() - stage_start_ns;
#elif !defined(HOST_BUILD) && RDS_RUNTIME_STAGE_CYCLE_COUNTERS_ENABLED
            rds_runtime_cycles_add(
                &dsp->runtime_profile.hb1_cycles, stage_start_cycles, rds_runtime_cycles_now(dsp));
#endif
            continue;
        }

        const int16_t* hb1_i_window = rds_hist_window_q15(dsp->hb1_i_hist, dsp->hb1_head);
        const int16_t* hb1_q_window = rds_hist_window_q15(dsp->hb1_q_hist, dsp->hb1_head);
        int16_t hb1_i = rds_halfband19_q15(hb1_i_window);
        int16_t hb1_q = rds_halfband19_q15(hb1_q_window);

#if defined(HOST_BUILD) && RDS_STAGE_TIMING_ENABLED
        dsp->profile.hb1_ns += rds_host_now_ns() - stage_start_ns;
        dsp->profile.hb1_outputs++;
        stage_start_ns = rds_host_now_ns();
#elif !defined(HOST_BUILD) && RDS_RUNTIME_STAGE_CYCLE_COUNTERS_ENABLED
        rds_runtime_cycles_add(
            &dsp->runtime_profile.hb1_cycles, stage_start_cycles, rds_runtime_cycles_now(dsp));
        dsp->runtime_profile.hb1_outputs++;
        stage_start_cycles = rds_runtime_cycles_now(dsp);
#endif

        uint8_t hb2_write_index = dsp->hb2_head;
        rds_hist_store_q15(dsp->hb2_i_hist, hb2_write_index, RDS_HB2_TAPS, hb1_i);
        rds_hist_store_q15(dsp->hb2_q_hist, hb2_write_index, RDS_HB2_TAPS, hb1_q);
        dsp->hb2_head = rds_hist_advance_head(hb2_write_index, RDS_HB2_TAPS);
        dsp->hb2_phase ^= 1U;
        if(dsp->hb2_phase != 0U) {
#if defined(HOST_BUILD) && RDS_STAGE_TIMING_ENABLED
            dsp->profile.hb2_ns += rds_host_now_ns() - stage_start_ns;
#elif !defined(HOST_BUILD) && RDS_RUNTIME_STAGE_CYCLE_COUNTERS_ENABLED
            rds_runtime_cycles_add(
                &dsp->runtime_profile.hb2_cycles, stage_start_cycles, rds_runtime_cycles_now(dsp));
#endif
            continue;
        }

        const int16_t* hb2_i_window = rds_hist_window_q15(dsp->hb2_i_hist, dsp->hb2_head);
        const int16_t* hb2_q_window = rds_hist_window_q15(dsp->hb2_q_hist, dsp->hb2_head);
        int16_t hb2_i = rds_halfband31_q15(hb2_i_window);
        int16_t hb2_q = rds_halfband31_q15(hb2_q_window);

#if defined(HOST_BUILD) && RDS_STAGE_TIMING_ENABLED
        dsp->profile.hb2_ns += rds_host_now_ns() - stage_start_ns;
        dsp->profile.hb2_outputs++;
        stage_start_ns = rds_host_now_ns();
#elif !defined(HOST_BUILD) && RDS_RUNTIME_STAGE_CYCLE_COUNTERS_ENABLED
        rds_runtime_cycles_add(
            &dsp->runtime_profile.hb2_cycles, stage_start_cycles, rds_runtime_cycles_now(dsp));
        dsp->runtime_profile.hb2_outputs++;
        stage_start_cycles = rds_runtime_cycles_now(dsp);
#endif

        uint8_t hb3_write_index = dsp->hb3_head;
        rds_hist_store_q15(dsp->hb3_i_hist, hb3_write_index, RDS_HB3_TAPS, hb2_i);
        rds_hist_store_q15(dsp->hb3_q_hist, hb3_write_index, RDS_HB3_TAPS, hb2_q);
        dsp->hb3_head = rds_hist_advance_head(hb3_write_index, RDS_HB3_TAPS);
        dsp->hb3_phase ^= 1U;
        if(dsp->hb3_phase != 0U) {
#if defined(HOST_BUILD) && RDS_STAGE_TIMING_ENABLED
            dsp->profile.hb3_ns += rds_host_now_ns() - stage_start_ns;
#elif !defined(HOST_BUILD) && RDS_RUNTIME_STAGE_CYCLE_COUNTERS_ENABLED
            rds_runtime_cycles_add(
                &dsp->runtime_profile.hb3_cycles, stage_start_cycles, rds_runtime_cycles_now(dsp));
#endif
            continue;
        }

        const int16_t* hb3_i_window = rds_hist_window_q15(dsp->hb3_i_hist, dsp->hb3_head);
        const int16_t* hb3_q_window = rds_hist_window_q15(dsp->hb3_q_hist, dsp->hb3_head);
        int16_t hb3_i = rds_halfband11_q15(hb3_i_window);
        int16_t hb3_q = rds_halfband11_q15(hb3_q_window);

#if defined(HOST_BUILD) && RDS_STAGE_TIMING_ENABLED
        dsp->profile.hb3_ns += rds_host_now_ns() - stage_start_ns;
        dsp->profile.hb3_outputs++;
        stage_start_ns = rds_host_now_ns();
#elif !defined(HOST_BUILD) && RDS_RUNTIME_STAGE_CYCLE_COUNTERS_ENABLED
        rds_runtime_cycles_add(
            &dsp->runtime_profile.hb3_cycles, stage_start_cycles, rds_runtime_cycles_now(dsp));
        dsp->runtime_profile.hb3_outputs++;
        stage_start_cycles = rds_runtime_cycles_now(dsp);
#endif

        uint8_t en50067_write_index = dsp->en50067_head;
        rds_hist_store_q15(dsp->en50067_i_hist, en50067_write_index, RDS_EN50067_FIR_TAPS, hb3_i);
        rds_hist_store_q15(dsp->en50067_q_hist, en50067_write_index, RDS_EN50067_FIR_TAPS, hb3_q);
        dsp->en50067_head = rds_hist_advance_head(en50067_write_index, RDS_EN50067_FIR_TAPS);
        const int16_t* en50067_i_window =
            rds_hist_window_q15(dsp->en50067_i_hist, dsp->en50067_head);
        const int16_t* en50067_q_window =
            rds_hist_window_q15(dsp->en50067_q_hist, dsp->en50067_head);
        /* FIR41 returns Q15 already saturated to int16; RDS_FIR41_OUTPUT_SHIFT
         * documents that no extra post-shift is applied for the lowpass design. */
        int16_t filtered_i = rds_fir41_apply_q15(en50067_i_window);
        int16_t filtered_q = rds_fir41_apply_q15(en50067_q_window);

#if defined(HOST_BUILD) && RDS_STAGE_TIMING_ENABLED
        dsp->profile.en50067_ns += rds_host_now_ns() - stage_start_ns;
        stage_start_ns = rds_host_now_ns();
#elif !defined(HOST_BUILD) && RDS_RUNTIME_STAGE_CYCLE_COUNTERS_ENABLED
        rds_runtime_cycles_add(
            &dsp->runtime_profile.en50067_cycles, stage_start_cycles, rds_runtime_cycles_now(dsp));
        stage_start_cycles = rds_runtime_cycles_now(dsp);
#endif

        dsp->half_i_integrator += filtered_i;
        dsp->half_q_integrator += filtered_q;

        uint32_t vector_mag_sample =
            rds_scale_filter_level_q8(rds_abs_i32(filtered_i) + rds_abs_i32(filtered_q));
        block_rds_band_sum_q8 += vector_mag_sample;
        block_rds_band_count++;

        uint32_t period_q16 = dsp->cached_symbol_period_q16;
        if(period_q16 < 0x00020000U) {
            period_q16 = 0x00020000U;
        }
        uint32_t half_period_q16 = period_q16 >> 1U;

        dsp->symbol_phase_q16 += dsp->decim_step_q16;

        while(true) {
            uint32_t boundary_q16 = (dsp->half_symbol_phase == 0U) ? half_period_q16 : period_q16;
            if(dsp->symbol_phase_q16 < boundary_q16) {
                break;
            }

            int32_t half_i = dsp->half_i_integrator;
            int32_t half_q = dsp->half_q_integrator;
            dsp->half_i_integrator = 0;
            dsp->half_q_integrator = 0;

            if(dsp->half_symbol_phase == 0U) {
                dsp->first_half_i_symbol = half_i;
                dsp->first_half_q_symbol = half_q;
                dsp->prev_half_i_symbol = half_i;
                dsp->prev_half_q_symbol = half_q;
                dsp->prev_half_symbol_valid = true;
                dsp->half_symbol_phase = 1U;
                continue;
            }

            int32_t symbol_i = dsp->first_half_i_symbol - half_i;
            int32_t symbol_q = dsp->first_half_q_symbol - half_q;
#if RDS_EXTRA_DIAGNOSTICS_ENABLED
            uint32_t symbol_vector_mag = rds_abs_i32(dsp->first_half_i_symbol) +
                                         rds_abs_i32(dsp->first_half_q_symbol) +
                                         rds_abs_i32(half_i) + rds_abs_i32(half_q);
            dsp->avg_vector_mag_q8 = rds_ema_u32(dsp->avg_vector_mag_q8, symbol_vector_mag, 8U);
#endif

            if(!dsp->prev_symbol_valid) {
                dsp->prev_i_symbol = symbol_i;
                dsp->prev_q_symbol = symbol_q;
                dsp->prev_symbol_valid = true;
            } else {
                int32_t dot = rds_symbol_dot_fast_q31(
                    symbol_i, symbol_q, dsp->prev_i_symbol, dsp->prev_q_symbol);
                uint8_t bit = (dot < 0) ? 1U : 0U;
#if RDS_EXTRA_DIAGNOSTICS_ENABLED
#if RDS_FAST_CONFIDENCE
                uint32_t decision_mag =
                    (rds_abs_i32(symbol_i) + rds_abs_i32(symbol_q) +
                     rds_abs_i32(dsp->prev_i_symbol) + rds_abs_i32(dsp->prev_q_symbol)) >>
                    3U;
                if(decision_mag > 65535U) {
                    decision_mag = 65535U;
                }
                uint32_t confidence_q16 = decision_mag;
#else
                uint64_t abs_dot = (uint64_t)rds_abs_i32(dot);
                uint32_t decision_mag = (uint32_t)(abs_dot >> 16U);
                if((abs_dot >> 16U) > 0xFFFFFFFFULL) {
                    decision_mag = 0xFFFFFFFFU;
                }

                uint32_t denominator = symbol_vector_mag + rds_abs_i32(dsp->prev_i_symbol) +
                                       rds_abs_i32(dsp->prev_q_symbol) + 1U;
                uint32_t confidence_q16 =
                    (uint32_t)(((uint64_t)decision_mag << 16U) / (uint64_t)denominator);
                if(confidence_q16 > 65535U) confidence_q16 = 65535U;
#endif
#endif

#ifdef HOST_BUILD
                if(dsp->bit_log && dsp->bit_log_count < dsp->bit_log_capacity) {
                    dsp->bit_log[dsp->bit_log_count++] = bit;
                }
#endif

#if RDS_EXTRA_DIAGNOSTICS_ENABLED
                dsp->avg_decision_mag_q8 = rds_ema_u32(dsp->avg_decision_mag_q8, decision_mag, 8U);
                dsp->symbol_confidence_avg_q16 =
                    rds_ema_u32(dsp->symbol_confidence_avg_q16, confidence_q16, 7U);
                block_confidence_sum_q16 += confidence_q16;
                block_symbol_count++;
#endif

                core->pilot_level_q8 = dsp->pilot_level_q8;
                core->rds_band_level_q8 = dsp->rds_band_level_q8;
#if RDS_EXTRA_DIAGNOSTICS_ENABLED
                core->lock_quality_q16 = dsp->symbol_confidence_avg_q16;
#else
                core->lock_quality_q16 = 0U;
#endif

#if defined(HOST_BUILD) && RDS_STAGE_TIMING_ENABLED
                uint64_t core_decode_start_ns = rds_host_now_ns();
#elif !defined(HOST_BUILD) && RDS_RUNTIME_STAGE_CYCLE_COUNTERS_ENABLED
                uint32_t core_decode_start_cycles = rds_runtime_cycles_now(dsp);
#endif
                (void)rds_core_consume_demod_bit(core, bit, NULL);
#if defined(HOST_BUILD) && RDS_STAGE_TIMING_ENABLED
                dsp->profile.core_decode_ns += rds_host_now_ns() - core_decode_start_ns;
#elif !defined(HOST_BUILD) && RDS_RUNTIME_STAGE_CYCLE_COUNTERS_ENABLED
                rds_runtime_cycles_add(
                    &dsp->runtime_profile.core_decode_cycles,
                    core_decode_start_cycles,
                    rds_runtime_cycles_now(dsp));
#endif
                dsp->prev_i_symbol = symbol_i;
                dsp->prev_q_symbol = symbol_q;
                dsp->symbol_count++;
                if(dsp->symbol_callback) {
                    dsp->symbol_callback(
                        dsp->symbol_callback_context,
                        symbol_i,
                        symbol_q,
#if RDS_EXTRA_DIAGNOSTICS_ENABLED
                        confidence_q16);
#else
                        0U);
#endif
                }

#if defined(HOST_BUILD) && RDS_STAGE_TIMING_ENABLED
                dsp->profile.symbol_events++;
#elif !defined(HOST_BUILD) && RDS_RUNTIME_STAGE_CYCLE_COUNTERS_ENABLED
                dsp->runtime_profile.symbol_events++;
#endif
            }

            dsp->prev_half_i_symbol = half_i;
            dsp->prev_half_q_symbol = half_q;
            dsp->prev_half_symbol_valid = true;

            uint32_t phase_remainder_q16 = dsp->symbol_phase_q16 - period_q16;
            if(phase_remainder_q16 >= period_q16) {
                phase_remainder_q16 = period_q16 - 1U;
            }
            dsp->symbol_phase_q16 = phase_remainder_q16;
            dsp->half_symbol_phase = 0U;
        }

#if defined(HOST_BUILD) && RDS_STAGE_TIMING_ENABLED
        dsp->profile.symbol_core_ns += rds_host_now_ns() - stage_start_ns;
#elif !defined(HOST_BUILD) && RDS_RUNTIME_STAGE_CYCLE_COUNTERS_ENABLED
        rds_runtime_cycles_add(
            &dsp->runtime_profile.symbol_core_cycles,
            stage_start_cycles,
            rds_runtime_cycles_now(dsp));
#endif
    }

    uint32_t block_corrected_count = core->corrected_blocks - corrected_before_block;
    uint32_t block_uncorrectable_count = core->uncorrectable_blocks - uncorrectable_before_block;

    if(count > 0U) {
        dsp->avg_abs_hp_q8 =
            rds_ema_u32(dsp->avg_abs_hp_q8, (uint32_t)(block_abs_hp_sum_q8 / count), 4U);
    }
    if(block_pilot_mag_count > 0U) {
        dsp->pilot_level_q8 = rds_ema_u32(
            dsp->pilot_level_q8, (uint32_t)(block_pilot_mag_sum_q8 / block_pilot_mag_count), 4U);
    }
    if(block_rds_band_count > 0U) {
        dsp->rds_band_level_q8 = rds_ema_u32(
            dsp->rds_band_level_q8, (uint32_t)(block_rds_band_sum_q8 / block_rds_band_count), 4U);
    }

#if RDS_EXTRA_DIAGNOSTICS_ENABLED
    dsp->block_symbol_count_last = block_symbol_count;
    dsp->block_corrected_count_last = block_corrected_count;
    dsp->block_uncorrectable_count_last = block_uncorrectable_count;
    if(block_symbol_count > 0U) {
        uint32_t block_confidence_q16 = (uint32_t)(block_confidence_sum_q16 / block_symbol_count);
        dsp->block_confidence_last_q16 = block_confidence_q16;
        dsp->block_confidence_avg_q16 =
            rds_ema_u32(dsp->block_confidence_avg_q16, block_confidence_q16, 5U);
    }
    dsp->block_corrected_confidence_last_q16 = 0U;
    dsp->block_uncorrectable_confidence_last_q16 = 0U;

    core->pilot_level_q8 = dsp->pilot_level_q8;
    core->rds_band_level_q8 = dsp->rds_band_level_q8;
    core->lock_quality_q16 = dsp->symbol_confidence_avg_q16;
#else
    dsp->block_symbol_count_last = 0U;
    dsp->block_corrected_count_last = block_corrected_count;
    dsp->block_uncorrectable_count_last = block_uncorrectable_count;
    dsp->block_confidence_last_q16 = 0U;
    dsp->block_confidence_avg_q16 = 0U;
    dsp->block_corrected_confidence_last_q16 = 0U;
    dsp->block_uncorrectable_confidence_last_q16 = 0U;
    core->pilot_level_q8 = dsp->pilot_level_q8;
    core->rds_band_level_q8 = dsp->rds_band_level_q8;
    core->lock_quality_q16 = 0U;
#endif
}
