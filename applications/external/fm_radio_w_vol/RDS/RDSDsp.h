#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "RDSCore.h"

typedef struct {
    uint32_t sample_rate_hz;
    bool use_fast_path_228k;
    uint8_t sample_mod4;
    uint8_t sample_mod12;
    uint8_t decim_factor;
    uint8_t decim_phase;
    uint32_t decim_step_q16;
    uint32_t samples_per_symbol_q16;
    uint32_t symbol_phase_q16;
    int32_t timing_adjust_q16;
    int32_t timing_adjust_limit_q16;
    int32_t timing_error_avg_q8;
    uint32_t carrier_phase_q32;
    uint32_t carrier_step_q32;
    uint32_t pilot_phase_q32;
    uint32_t pilot_step_q32;
    int32_t dc_estimate_q8;
    int32_t i_lpf_state;
    int32_t q_lpf_state;
    int32_t i_lpf_state2;
    int32_t q_lpf_state2;
    int32_t i_lpf_state3;
    int32_t q_lpf_state3;
    int32_t i_integrator;
    int32_t q_integrator;
    uint32_t early_energy_acc;
    uint32_t late_energy_acc;
    int32_t prev_i_symbol;
    int32_t prev_q_symbol;
    bool prev_symbol_valid;
    uint32_t symbol_count;
    uint32_t symbol_confidence_avg_q16;
    uint32_t block_symbol_count_last;
    uint32_t block_confidence_last_q16;
    uint32_t block_confidence_avg_q16;
    uint32_t corrected_confidence_avg_q16;
    uint32_t uncorrectable_confidence_avg_q16;
    uint32_t block_corrected_count_last;
    uint32_t block_uncorrectable_count_last;
    uint32_t block_corrected_confidence_last_q16;
    uint32_t block_uncorrectable_confidence_last_q16;
    uint32_t pilot_level_q8;
    uint32_t rds_band_level_q8;
    uint32_t avg_abs_hp_q8;
    uint32_t avg_vector_mag_q8;
    uint32_t avg_decision_mag_q8;
    uint32_t cached_symbol_period_q16;
} RDSDsp;

void rds_dsp_init(RDSDsp* dsp, uint32_t sample_rate_hz);
void rds_dsp_reset(RDSDsp* dsp);

void rds_dsp_process_u16_samples(
    RDSDsp* dsp,
    RDSCore* core,
    const uint16_t* samples,
    size_t count,
    uint16_t adc_midpoint);
