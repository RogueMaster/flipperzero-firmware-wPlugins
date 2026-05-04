#include "RDSDsp.h"

#include <limits.h>

#define RDS_BITRATE_Q16                       0x04A38000UL /* 1187.5 * 65536 */
#define RDS_PILOT_TRACK_TOLERANCE_HZ          2U
#define RDS_CARRIER_MIN_HZ                    (RDS_CARRIER_NOMINAL_HZ - (RDS_PILOT_TRACK_TOLERANCE_HZ * 3U))
#define RDS_CARRIER_MAX_HZ                    (RDS_CARRIER_NOMINAL_HZ + (RDS_PILOT_TRACK_TOLERANCE_HZ * 3U))
#define RDS_PILOT_TRACK_MIN_HZ                (RDS_PILOT_HZ - RDS_PILOT_TRACK_TOLERANCE_HZ)
#define RDS_PILOT_TRACK_MAX_HZ                (RDS_PILOT_HZ + RDS_PILOT_TRACK_TOLERANCE_HZ)
#define RDS_CARRIER_MANUAL_OFFSET_MIN_CENTIHZ (-600)
#define RDS_CARRIER_MANUAL_OFFSET_MAX_CENTIHZ (600)
static const int16_t rds_carrier_cos_q8[16] = {
    256,
    237,
    181,
    98,
    0,
    -98,
    -181,
    -237,
    -256,
    -237,
    -181,
    -98,
    0,
    98,
    181,
    237,
};

static const int16_t rds_carrier_sin_q8[16] = {
    0,
    98,
    181,
    237,
    256,
    237,
    181,
    98,
    0,
    -98,
    -181,
    -237,
    -256,
    -237,
    -181,
    -98,
};

static const int16_t rds_pilot_cos_q8[12] = {
    256,
    222,
    128,
    0,
    -128,
    -222,
    -256,
    -222,
    -128,
    0,
    128,
    222,
};

static const int16_t rds_pilot_sin_q8[12] = {
    0,
    128,
    222,
    256,
    222,
    128,
    0,
    -128,
    -222,
    -256,
    -222,
    -128,
};

static uint32_t rds_abs_i32(int32_t value) {
    uint32_t uvalue = (uint32_t)value;
    uint32_t sign_mask = (uint32_t) - (int32_t)(uvalue >> 31U);
    return (uvalue ^ sign_mask) - sign_mask;
}

static uint32_t rds_ema_u32(uint32_t avg, uint32_t sample, uint8_t shift) {
    return (uint32_t)((int32_t)avg + (((int32_t)sample - (int32_t)avg) >> shift));
}

static int32_t rds_clamp_i32(int32_t value, int32_t min_value, int32_t max_value) {
    if(value < min_value) return min_value;
    if(value > max_value) return max_value;
    return value;
}

static uint32_t rds_phase_step_q32(uint32_t freq_hz, uint32_t sample_rate_hz) {
    if(sample_rate_hz == 0U) {
        return 0U;
    }

    return (uint32_t)((((uint64_t)freq_hz) << 32U) / sample_rate_hz);
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
    uint64_t carrier_step = (uint64_t)pilot_step_q32 * 3ULL;
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

static uint8_t rds_phase_index_q32(uint32_t phase_q32, uint8_t table_size) {
    return (uint8_t)(((uint64_t)phase_q32 * (uint64_t)table_size) >> 32U);
}

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
    dsp->pilot_nominal_step_q32 = rds_phase_step_q32(RDS_PILOT_HZ, dsp->sample_rate_hz);
    dsp->pilot_min_step_q32 = rds_phase_step_q32(RDS_PILOT_TRACK_MIN_HZ, dsp->sample_rate_hz);
    dsp->pilot_max_step_q32 = rds_phase_step_q32(RDS_PILOT_TRACK_MAX_HZ, dsp->sample_rate_hz);
    dsp->pilot_step_q32 = rds_phase_step_q32(carrier_hz / 3U, dsp->sample_rate_hz);
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

static uint32_t rds_symbol_period_q16(const RDSDsp* dsp) {
    int32_t period = (int32_t)dsp->samples_per_symbol_q16 + dsp->timing_adjust_q16;
    int32_t min_period = (int32_t)dsp->samples_per_symbol_q16 - dsp->timing_adjust_limit_q16;
    int32_t max_period = (int32_t)dsp->samples_per_symbol_q16 + dsp->timing_adjust_limit_q16;

    period = rds_clamp_i32(period, min_period, max_period);
    if(period < 0x00010000L) {
        period = 0x00010000L;
    }

    return (uint32_t)period;
}

void rds_dsp_init(RDSDsp* dsp, uint32_t sample_rate_hz) {
    if(!dsp) return;

    dsp->sample_rate_hz = sample_rate_hz;
    dsp->decim_factor = 1U;
    dsp->decim_phase = 0U;
    dsp->decim_step_q16 = (uint32_t)dsp->decim_factor << 16U;
    dsp->symbol_phase_q16 = 0U;
    dsp->timing_adjust_q16 = 0;
    dsp->timing_adjust_limit_q16 = 0;
    dsp->timing_error_avg_q8 = 0;
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
    dsp->i_lpf_state = 0;
    dsp->q_lpf_state = 0;
    dsp->i_lpf_state2 = 0;
    dsp->q_lpf_state2 = 0;
    dsp->i_lpf_state3 = 0;
    dsp->q_lpf_state3 = 0;
    dsp->i_lpf_state4 = 0;
    dsp->q_lpf_state4 = 0;
    dsp->i_integrator = 0;
    dsp->q_integrator = 0;
    dsp->half_i_integrator = 0;
    dsp->half_q_integrator = 0;
    dsp->early_energy_acc = 0U;
    dsp->late_energy_acc = 0U;
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
#endif

    if(sample_rate_hz == 0U) {
        dsp->samples_per_symbol_q16 = 0U;
        dsp->carrier_step_q32 = 0U;
        dsp->pilot_step_q32 = 0U;
    } else {
        uint64_t numerator = ((uint64_t)sample_rate_hz) << 32U;
        dsp->samples_per_symbol_q16 = (uint32_t)(numerator / RDS_BITRATE_Q16);
        dsp->pilot_nominal_step_q32 = rds_phase_step_q32(RDS_PILOT_HZ, sample_rate_hz);
        dsp->pilot_min_step_q32 = rds_phase_step_q32(RDS_PILOT_TRACK_MIN_HZ, sample_rate_hz);
        dsp->pilot_max_step_q32 = rds_phase_step_q32(RDS_PILOT_TRACK_MAX_HZ, sample_rate_hz);
        dsp->pilot_step_q32 = dsp->pilot_nominal_step_q32;
        dsp->carrier_step_q32 = rds_carrier_step_q32_with_manual_offset(dsp, dsp->pilot_step_q32);
        dsp->timing_adjust_limit_q16 = (int32_t)(dsp->samples_per_symbol_q16 >> 4U);
        dsp->cached_symbol_period_q16 = dsp->samples_per_symbol_q16;
    }
}

void rds_dsp_reset(RDSDsp* dsp) {
    if(!dsp) return;

    dsp->symbol_phase_q16 = 0U;
    dsp->timing_adjust_q16 = 0;
    dsp->timing_error_avg_q8 = 0;
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
    dsp->i_lpf_state = 0;
    dsp->q_lpf_state = 0;
    dsp->i_lpf_state2 = 0;
    dsp->q_lpf_state2 = 0;
    dsp->i_lpf_state3 = 0;
    dsp->q_lpf_state3 = 0;
    dsp->i_lpf_state4 = 0;
    dsp->q_lpf_state4 = 0;
    dsp->i_integrator = 0;
    dsp->q_integrator = 0;
    dsp->half_i_integrator = 0;
    dsp->half_q_integrator = 0;
    dsp->early_energy_acc = 0U;
    dsp->late_energy_acc = 0U;
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

    uint32_t block_symbol_count = 0U;
    uint64_t block_confidence_sum_q16 = 0U;
    const uint32_t corrected_before_block = core->corrected_blocks;
    const uint32_t uncorrectable_before_block = core->uncorrectable_blocks;

    for(size_t i = 0; i < count; i++) {
        int32_t centered = (int32_t)samples[i] - (int32_t)adc_midpoint;
        int32_t centered_q8 = centered << 8;

        dsp->dc_estimate_q8 += (centered_q8 - dsp->dc_estimate_q8) >> 6;
        int32_t hp = centered_q8 - dsp->dc_estimate_q8;
        dsp->avg_abs_hp_q8 = rds_ema_u32(dsp->avg_abs_hp_q8, rds_abs_i32(hp), 8U);

        uint8_t pilot_index = rds_phase_index_q32(dsp->pilot_phase_q32, 12U);
        int32_t pilot_i = (hp * rds_pilot_cos_q8[pilot_index]) >> 8;
        int32_t pilot_q = (-hp * rds_pilot_sin_q8[pilot_index]) >> 8;
        dsp->pilot_phase_q32 += dsp->pilot_step_q32;

        dsp->pilot_i_lpf_state += (pilot_i - dsp->pilot_i_lpf_state) >> 4;
        dsp->pilot_q_lpf_state += (pilot_q - dsp->pilot_q_lpf_state) >> 4;
        uint32_t pilot_mag_sample =
            rds_abs_i32(dsp->pilot_i_lpf_state) + rds_abs_i32(dsp->pilot_q_lpf_state);
        dsp->pilot_level_q8 = rds_ema_u32(dsp->pilot_level_q8, pilot_mag_sample, 8U);

        dsp->pilot_update_div++;
        if(dsp->pilot_update_div >= 16U) {
            int64_t dot =
                ((int64_t)dsp->pilot_prev_i_lpf_state * (int64_t)dsp->pilot_i_lpf_state) +
                ((int64_t)dsp->pilot_prev_q_lpf_state * (int64_t)dsp->pilot_q_lpf_state);
            int64_t cross =
                ((int64_t)dsp->pilot_prev_i_lpf_state * (int64_t)dsp->pilot_q_lpf_state) -
                ((int64_t)dsp->pilot_prev_q_lpf_state * (int64_t)dsp->pilot_i_lpf_state);
            uint64_t abs_dot = (uint64_t)((dot < 0) ? -dot : dot);
            uint64_t abs_cross = (uint64_t)((cross < 0) ? -cross : cross);
            int32_t phase_error_q15 = 0;

            if((abs_dot + abs_cross) > 0U) {
                int64_t denom = (int64_t)(abs_dot + abs_cross + 1U);
                phase_error_q15 = (int32_t)((cross << 15U) / denom);
            }

            dsp->pilot_error_avg_q15 += (phase_error_q15 - dsp->pilot_error_avg_q15) >> 6;

            int32_t step_nudge_q32 =
                rds_clamp_i32(dsp->pilot_error_avg_q15 << 1U, -131072, 131072);
            int64_t pilot_step = (int64_t)dsp->pilot_nominal_step_q32 + (int64_t)step_nudge_q32;
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

        uint8_t carrier_index = (uint8_t)(dsp->carrier_phase_q32 >> 28U);
        int32_t mixed_i = (hp * rds_carrier_cos_q8[carrier_index]) >> 8;
        int32_t mixed_q = (-hp * rds_carrier_sin_q8[carrier_index]) >> 8;
        dsp->carrier_phase_q32 += dsp->carrier_step_q32;

        // Four-stage cascade IIR LPF (alpha=1/8 each).
        // Extra rolloff helps reject stereo L-R leakage above the desired baseband.
        // Stage 1
        dsp->i_lpf_state += (mixed_i - dsp->i_lpf_state) >> 3;
        dsp->q_lpf_state += (mixed_q - dsp->q_lpf_state) >> 3;
        // Stage 2
        dsp->i_lpf_state2 += (dsp->i_lpf_state - dsp->i_lpf_state2) >> 3;
        dsp->q_lpf_state2 += (dsp->q_lpf_state - dsp->q_lpf_state2) >> 3;
        // Stage 3
        dsp->i_lpf_state3 += (dsp->i_lpf_state2 - dsp->i_lpf_state3) >> 3;
        dsp->q_lpf_state3 += (dsp->q_lpf_state2 - dsp->q_lpf_state3) >> 3;
        // Stage 4
        dsp->i_lpf_state4 += (dsp->i_lpf_state3 - dsp->i_lpf_state4) >> 3;
        dsp->q_lpf_state4 += (dsp->q_lpf_state3 - dsp->q_lpf_state4) >> 3;

        dsp->half_i_integrator += dsp->i_lpf_state4;
        dsp->half_q_integrator += dsp->q_lpf_state4;

        uint32_t vector_mag_sample =
            rds_abs_i32(dsp->i_lpf_state4) + rds_abs_i32(dsp->q_lpf_state4);
        dsp->rds_band_level_q8 = rds_ema_u32(dsp->rds_band_level_q8, vector_mag_sample, 8U);

        uint32_t period_q16 = dsp->cached_symbol_period_q16;
        if(period_q16 < 0x00020000U) {
            period_q16 = 0x00020000U;
        }
        uint32_t half_period_q16 = period_q16 >> 1U;
        uint32_t edge_window_q16 = period_q16 >> 3U;
        uint32_t phase_before = dsp->symbol_phase_q16;

        if(phase_before < edge_window_q16) {
            dsp->early_energy_acc += vector_mag_sample;
        }
        if(phase_before >= (period_q16 - edge_window_q16)) {
            dsp->late_energy_acc += vector_mag_sample;
        }

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
            uint32_t symbol_vector_mag = rds_abs_i32(dsp->first_half_i_symbol) +
                                         rds_abs_i32(dsp->first_half_q_symbol) +
                                         rds_abs_i32(half_i) + rds_abs_i32(half_q);
            dsp->avg_vector_mag_q8 = rds_ema_u32(dsp->avg_vector_mag_q8, symbol_vector_mag, 8U);

            if(!dsp->prev_symbol_valid) {
                dsp->prev_i_symbol = symbol_i;
                dsp->prev_q_symbol = symbol_q;
                dsp->prev_symbol_valid = true;
            } else {
                int64_t dot = ((int64_t)symbol_i * (int64_t)dsp->prev_i_symbol) +
                              ((int64_t)symbol_q * (int64_t)dsp->prev_q_symbol);
                uint8_t bit = (dot < 0) ? 1U : 0U;
                uint64_t abs_dot = (uint64_t)((dot < 0) ? -dot : dot);
                uint32_t decision_mag = (uint32_t)(abs_dot >> 16U);
                if((abs_dot >> 16U) > 0xFFFFFFFFULL) {
                    decision_mag = 0xFFFFFFFFU;
                }

                uint32_t denominator = symbol_vector_mag + rds_abs_i32(dsp->prev_i_symbol) +
                                       rds_abs_i32(dsp->prev_q_symbol) + 1U;
                uint32_t confidence_q16 =
                    (uint32_t)(((uint64_t)decision_mag << 16U) / (uint64_t)denominator);
                if(confidence_q16 > 65535U) confidence_q16 = 65535U;

#ifdef HOST_BUILD
                if(dsp->bit_log && dsp->bit_log_count < dsp->bit_log_capacity) {
                    dsp->bit_log[dsp->bit_log_count++] = bit;
                }
#endif

                dsp->avg_decision_mag_q8 = rds_ema_u32(dsp->avg_decision_mag_q8, decision_mag, 8U);
                dsp->symbol_confidence_avg_q16 =
                    rds_ema_u32(dsp->symbol_confidence_avg_q16, confidence_q16, 7U);
                block_confidence_sum_q16 += confidence_q16;
                block_symbol_count++;

                core->pilot_level_q8 = dsp->pilot_level_q8;
                core->rds_band_level_q8 = dsp->rds_band_level_q8;
                core->lock_quality_q16 = dsp->symbol_confidence_avg_q16;

                (void)rds_core_consume_demod_bit(core, bit, NULL);
                dsp->prev_i_symbol = symbol_i;
                dsp->prev_q_symbol = symbol_q;
                dsp->symbol_count++;
                if(dsp->symbol_callback) {
                    dsp->symbol_callback(
                        dsp->symbol_callback_context, symbol_i, symbol_q, confidence_q16);
                }
            }

            dsp->prev_half_i_symbol = half_i;
            dsp->prev_half_q_symbol = half_q;
            dsp->prev_half_symbol_valid = true;

            int64_t timing_error64 =
                (int64_t)dsp->late_energy_acc - (int64_t)dsp->early_energy_acc;
            int32_t timing_error;
            if(timing_error64 > INT32_MAX) {
                timing_error = INT32_MAX;
            } else if(timing_error64 < INT32_MIN) {
                timing_error = INT32_MIN;
            } else {
                timing_error = (int32_t)timing_error64;
            }

            int32_t phase_step = timing_error >> 10;
            phase_step = rds_clamp_i32(phase_step, -1024, 1024);

            if(rds_abs_i32(timing_error) < (dsp->avg_vector_mag_q8 >> 4U)) {
                phase_step = 0;
            }

            dsp->symbol_phase_q16 += (uint32_t)phase_step;

            dsp->timing_error_avg_q8 += (timing_error - dsp->timing_error_avg_q8) >> 6;
            dsp->cached_symbol_period_q16 = rds_symbol_period_q16(dsp);
            period_q16 = dsp->cached_symbol_period_q16;
            if(period_q16 < 0x00020000U) {
                period_q16 = 0x00020000U;
            }
            half_period_q16 = period_q16 >> 1U;

            dsp->symbol_phase_q16 -= period_q16;
            dsp->early_energy_acc = 0U;
            dsp->late_energy_acc = 0U;
            dsp->half_symbol_phase = 0U;
        }
    }

    uint32_t block_corrected_count = core->corrected_blocks - corrected_before_block;
    uint32_t block_uncorrectable_count = core->uncorrectable_blocks - uncorrectable_before_block;

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
}
