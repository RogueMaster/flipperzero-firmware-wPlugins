#include "RDSDsp.h"

#include <limits.h>

#define RDS_BITRATE_Q16            0x04A38000UL /* 1187.5 * 65536 */
#define RDS_CARRIER_HZ             57000U
#define RDS_PILOT_HZ               19000U
#define RDS_FAST_PATH_228K_HZ      228000U
#define RDS_FAST_PATH_DECIM_FACTOR 2U

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

static const uint16_t rds_pilot_abs_sum_q8[12] = {
    256U,
    350U,
    350U,
    256U,
    350U,
    350U,
    256U,
    350U,
    350U,
    256U,
    350U,
    350U,
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
    dsp->use_fast_path_228k = (sample_rate_hz == RDS_FAST_PATH_228K_HZ);
    dsp->sample_mod4 = 0U;
    dsp->sample_mod12 = 0U;
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
    dsp->dc_estimate_q8 = 0;
    dsp->i_lpf_state = 0;
    dsp->q_lpf_state = 0;
    dsp->i_lpf_state2 = 0;
    dsp->q_lpf_state2 = 0;
    dsp->i_lpf_state3 = 0;
    dsp->q_lpf_state3 = 0;
    dsp->i_integrator = 0;
    dsp->q_integrator = 0;
    dsp->early_energy_acc = 0U;
    dsp->late_energy_acc = 0U;
    dsp->prev_i_symbol = 0;
    dsp->prev_q_symbol = 0;
    dsp->prev_symbol_valid = false;
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

    if(sample_rate_hz == 0U) {
        dsp->samples_per_symbol_q16 = 0U;
        dsp->carrier_step_q32 = 0U;
        dsp->pilot_step_q32 = 0U;
    } else {
        uint64_t numerator = ((uint64_t)sample_rate_hz) << 32U;
        dsp->samples_per_symbol_q16 = (uint32_t)(numerator / RDS_BITRATE_Q16);
        dsp->carrier_step_q32 = (uint32_t)((((uint64_t)RDS_CARRIER_HZ) << 32U) / sample_rate_hz);
        dsp->pilot_step_q32 = (uint32_t)((((uint64_t)RDS_PILOT_HZ) << 32U) / sample_rate_hz);
        dsp->timing_adjust_limit_q16 = (int32_t)(dsp->samples_per_symbol_q16 >> 4U);
        dsp->cached_symbol_period_q16 = dsp->samples_per_symbol_q16;
    }
}

void rds_dsp_reset(RDSDsp* dsp) {
    if(!dsp) return;

    dsp->symbol_phase_q16 = 0U;
    dsp->timing_adjust_q16 = 0;
    dsp->timing_error_avg_q8 = 0;
    dsp->sample_mod4 = 0U;
    dsp->sample_mod12 = 0U;
    dsp->decim_phase = 0U;
    dsp->carrier_phase_q32 = 0U;
    dsp->pilot_phase_q32 = 0U;
    dsp->dc_estimate_q8 = 0;
    dsp->i_lpf_state = 0;
    dsp->q_lpf_state = 0;
    dsp->i_lpf_state2 = 0;
    dsp->q_lpf_state2 = 0;
    dsp->i_lpf_state3 = 0;
    dsp->q_lpf_state3 = 0;
    dsp->i_integrator = 0;
    dsp->q_integrator = 0;
    dsp->early_energy_acc = 0U;
    dsp->late_energy_acc = 0U;
    dsp->prev_i_symbol = 0;
    dsp->prev_q_symbol = 0;
    dsp->prev_symbol_valid = false;
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

        uint8_t pilot_index;
        if(dsp->use_fast_path_228k) {
            pilot_index = dsp->sample_mod12;
            dsp->sample_mod12++;
            if(dsp->sample_mod12 >= 12U) {
                dsp->sample_mod12 = 0U;
            }
        } else {
            pilot_index = (uint8_t)((dsp->pilot_phase_q32 >> 28U) % 12U);
            dsp->pilot_phase_q32 += dsp->pilot_step_q32;
        }
        uint32_t pilot_mag_sample = (rds_abs_i32(hp) * rds_pilot_abs_sum_q8[pilot_index]) >> 8U;
        dsp->pilot_level_q8 = rds_ema_u32(dsp->pilot_level_q8, pilot_mag_sample, 8U);

        int32_t mixed_i;
        int32_t mixed_q;
        if(dsp->use_fast_path_228k) {
            switch(dsp->sample_mod4) {
            case 0U:
                mixed_i = hp;
                mixed_q = 0;
                break;
            case 1U:
                mixed_i = 0;
                mixed_q = -hp;
                break;
            case 2U:
                mixed_i = -hp;
                mixed_q = 0;
                break;
            default:
                mixed_i = 0;
                mixed_q = hp;
                break;
            }

            dsp->sample_mod4++;
            if(dsp->sample_mod4 >= 4U) {
                dsp->sample_mod4 = 0U;
            }
        } else {
            uint8_t carrier_index = (uint8_t)(dsp->carrier_phase_q32 >> 28U);
            mixed_i = (hp * rds_carrier_cos_q8[carrier_index]) >> 8;
            mixed_q = (-hp * rds_carrier_sin_q8[carrier_index]) >> 8;
            dsp->carrier_phase_q32 += dsp->carrier_step_q32;
        }

        // Three-stage cascade IIR LPF (alpha=1/8 each).
        // Effective -3dB at ~2.5 kHz, 18 dB/octave rolloff.
        // Rejects stereo L-R subcarrier leaking at 4+ kHz baseband.
        // Stage 1
        dsp->i_lpf_state += (mixed_i - dsp->i_lpf_state) >> 3;
        dsp->q_lpf_state += (mixed_q - dsp->q_lpf_state) >> 3;
        // Stage 2
        dsp->i_lpf_state2 += (dsp->i_lpf_state - dsp->i_lpf_state2) >> 3;
        dsp->q_lpf_state2 += (dsp->q_lpf_state - dsp->q_lpf_state2) >> 3;
        // Stage 3
        dsp->i_lpf_state3 += (dsp->i_lpf_state2 - dsp->i_lpf_state3) >> 3;
        dsp->q_lpf_state3 += (dsp->q_lpf_state2 - dsp->q_lpf_state3) >> 3;

        dsp->i_integrator += dsp->i_lpf_state3;
        dsp->q_integrator += dsp->q_lpf_state3;

        uint32_t vector_mag_sample =
            rds_abs_i32(dsp->i_lpf_state3) + rds_abs_i32(dsp->q_lpf_state3);
        dsp->rds_band_level_q8 = rds_ema_u32(dsp->rds_band_level_q8, vector_mag_sample, 8U);
        uint32_t period_q16 = dsp->cached_symbol_period_q16;
        uint32_t edge_window_q16 = period_q16 >> 3U;
        uint32_t phase_before = dsp->symbol_phase_q16;
        if(phase_before < edge_window_q16) {
            dsp->early_energy_acc += vector_mag_sample;
        }
        if(phase_before >= (period_q16 - edge_window_q16)) {
            dsp->late_energy_acc += vector_mag_sample;
        }

        dsp->symbol_phase_q16 += dsp->decim_step_q16;

        if(dsp->symbol_phase_q16 >= period_q16) {
            uint32_t vector_mag = rds_abs_i32(dsp->i_integrator) + rds_abs_i32(dsp->q_integrator);
            dsp->avg_vector_mag_q8 = rds_ema_u32(dsp->avg_vector_mag_q8, vector_mag, 8U);

            if(!dsp->prev_symbol_valid) {
                dsp->prev_i_symbol = dsp->i_integrator;
                dsp->prev_q_symbol = dsp->q_integrator;
                dsp->prev_symbol_valid = true;
                dsp->symbol_phase_q16 -= period_q16;
                dsp->i_integrator = 0;
                dsp->q_integrator = 0;
                dsp->early_energy_acc = 0U;
                dsp->late_energy_acc = 0U;
                continue;
            }

            int64_t dot = ((int64_t)dsp->i_integrator * (int64_t)dsp->prev_i_symbol) +
                          ((int64_t)dsp->q_integrator * (int64_t)dsp->prev_q_symbol);
            uint8_t bit = (dot < 0) ? 1U : 0U;
            uint32_t decision_mag = (uint32_t)((dot < 0) ? -dot : dot) >> 16U;

            uint32_t denominator = vector_mag + 1U;
            uint32_t confidence_q16 =
                (uint32_t)(((uint64_t)decision_mag << 16U) / (uint64_t)denominator);
            if(confidence_q16 > 65535U) confidence_q16 = 65535U;

            dsp->avg_decision_mag_q8 = rds_ema_u32(dsp->avg_decision_mag_q8, decision_mag, 8U);
            dsp->symbol_confidence_avg_q16 =
                rds_ema_u32(dsp->symbol_confidence_avg_q16, confidence_q16, 7U);
            block_confidence_sum_q16 += confidence_q16;
            block_symbol_count++;

            (void)rds_core_consume_demod_bit(core, bit, NULL);

            dsp->prev_i_symbol = dsp->i_integrator;
            dsp->prev_q_symbol = dsp->q_integrator;
            dsp->symbol_count++;

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

            int32_t timing_step = timing_error >> 10;
            timing_step = rds_clamp_i32(timing_step, -1024, 1024);
            if(rds_abs_i32(timing_error) < (dsp->avg_vector_mag_q8 >> 4U)) {
                timing_step = 0;
            }
            dsp->timing_adjust_q16 = rds_clamp_i32(
                dsp->timing_adjust_q16 + timing_step,
                -dsp->timing_adjust_limit_q16,
                dsp->timing_adjust_limit_q16);
            dsp->timing_error_avg_q8 += (timing_error - dsp->timing_error_avg_q8) >> 6;
            dsp->cached_symbol_period_q16 = rds_symbol_period_q16(dsp);

            dsp->symbol_phase_q16 -= period_q16;
            dsp->i_integrator = 0;
            dsp->q_integrator = 0;
            dsp->early_energy_acc = 0U;
            dsp->late_energy_acc = 0U;
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
