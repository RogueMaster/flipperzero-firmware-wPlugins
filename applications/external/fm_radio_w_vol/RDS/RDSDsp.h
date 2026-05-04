#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "RDSCore.h"

#define RDS_PILOT_HZ           19000U
#define RDS_CARRIER_NOMINAL_HZ 57000U

typedef void (*RdsDspSymbolCallback)(
    void* context,
    int32_t symbol_i,
    int32_t symbol_q,
    uint32_t confidence_q16);

typedef struct {
    uint32_t sample_rate_hz;
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
    uint32_t pilot_nominal_step_q32;
    uint32_t pilot_min_step_q32;
    uint32_t pilot_max_step_q32;
    int32_t pilot_error_avg_q15;
    int32_t pilot_i_lpf_state;
    int32_t pilot_q_lpf_state;
    int32_t pilot_prev_i_lpf_state;
    int32_t pilot_prev_q_lpf_state;
    uint8_t pilot_update_div;
    int32_t dc_estimate_q8;
    int32_t i_lpf_state;
    int32_t q_lpf_state;
    int32_t i_lpf_state2;
    int32_t q_lpf_state2;
    int32_t i_lpf_state3;
    int32_t q_lpf_state3;
    int32_t i_lpf_state4;
    int32_t q_lpf_state4;
    int32_t i_integrator;
    int32_t q_integrator;
    int32_t half_i_integrator;
    int32_t half_q_integrator;
    uint32_t early_energy_acc;
    uint32_t late_energy_acc;
    int32_t prev_i_symbol;
    int32_t prev_q_symbol;
    bool prev_symbol_valid;
    int32_t prev_half_i_symbol;
    int32_t prev_half_q_symbol;
    bool prev_half_symbol_valid;
    int32_t first_half_i_symbol;
    int32_t first_half_q_symbol;
    uint8_t half_symbol_phase;
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
    int16_t carrier_manual_offset_centihz;
    RdsDspSymbolCallback symbol_callback;
    void* symbol_callback_context;
#ifdef HOST_BUILD
    uint8_t* bit_log;
    size_t bit_log_count;
    size_t bit_log_capacity;
#endif
} RDSDsp;

void rds_dsp_init(RDSDsp* dsp, uint32_t sample_rate_hz);
void rds_dsp_reset(RDSDsp* dsp);
void rds_dsp_set_carrier_hz(RDSDsp* dsp, uint32_t carrier_hz);
void rds_dsp_set_manual_carrier_offset_centihz(RDSDsp* dsp, int16_t offset_centihz);
int16_t rds_dsp_get_manual_carrier_offset_centihz(const RDSDsp* dsp);
void rds_dsp_set_symbol_callback(RDSDsp* dsp, RdsDspSymbolCallback callback, void* context);
uint32_t rds_dsp_get_carrier_hz_q8(const RDSDsp* dsp);

void rds_dsp_process_u16_samples(
    RDSDsp* dsp,
    RDSCore* core,
    const uint16_t* samples,
    size_t count,
    uint16_t adc_midpoint);
