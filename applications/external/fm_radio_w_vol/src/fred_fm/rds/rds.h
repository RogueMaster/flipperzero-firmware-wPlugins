/**
 * @file rds.h
 * @brief RDS pipeline: acquisition worker, DSP, decode, and UI snapshots.
 */
#pragma once

#include <furi.h>
#include <furi_hal.h>
#include "src/drivers/rds/RDSCore.h"
#include "src/drivers/rds/RDSDsp.h"
#include "src/drivers/rds/RDSAcquisition.h"

extern bool rds_enabled;
extern bool rds_debug_enabled;
extern RDSCore rds_core;
extern RDSDsp rds_dsp;
extern RdsAcquisition rds_acquisition;
extern int16_t rds_carrier_offset_centihz;
extern int32_t rds_constellation_i_history[RDS_CONSTELLATION_HISTORY_LEN];
extern int32_t rds_constellation_q_history[RDS_CONSTELLATION_HISTORY_LEN];
extern int32_t rds_constellation_i_snapshot[RDS_CONSTELLATION_HISTORY_LEN];
extern int32_t rds_constellation_q_snapshot[RDS_CONSTELLATION_HISTORY_LEN];
extern uint8_t rds_constellation_history_count;
extern uint8_t rds_constellation_history_write_index;
extern uint32_t rds_constellation_saved_until_tick;
extern bool rds_constellation_view_active;
extern uint32_t rds_runtime_sample_rate_hz;
extern char rds_ps_display[RDS_PS_LEN + 1U];
extern RdsSyncState rds_sync_display;
extern const GpioPin* rds_adc_pin;
extern FuriHalAdcChannel rds_adc_channel;
extern FuriTimer* rds_adc_timer_handle;
extern bool rds_adc_timer_running;
extern FuriThread* rds_dsp_worker_thread;
extern volatile FuriThreadId rds_dsp_worker_thread_id;
extern uint8_t tea_i2c_failure_count;

/** Reset RDS globals and decoder/DSP handles. */
void fred_fm_rds_runtime_reset(void);

/** Start acquisition, DSP worker, and ADC timer when RDS enabled. */
void fred_fm_rds_pipeline_start(void);

/** True while RDS pipeline is active and not stopping. */
bool fred_fm_rds_pipeline_enabled(void);

/** Start 2 ms timer that wakes RdsDspWorker. */
void fred_fm_rds_timer_start(void);

/** Stop ADC timer. */
void fred_fm_rds_timer_stop(void);

/** Start/stop acquisition and optionally reset decoder. */
void fred_fm_rds_apply_runtime_state(bool reset_decoder);

/** Clear displayed station name and PS buffer. */
void fred_fm_rds_clear_station_name(void);

/** Clear constellation history under mutex. */
void fred_fm_rds_constellation_clear_history(void);

/** Manual RDS carrier offset for current frequency (centihertz). */
int16_t fred_fm_rds_get_manual_offset_centihz(void);

/** Set manual offset and push to DSP. */
void fred_fm_rds_set_manual_offset_centihz(int16_t offset_centihz);

/** Load offset from preset table for current frequency. */
void fred_fm_rds_sync_offset_from_current_frequency(void);

/** Copy constellation history and sync state for UI thread. */
void fred_fm_rds_update_ui_snapshot(void);

/** Re-tune hook: reset decoder and sync offset from preset. */
void fred_fm_rds_on_tuned_frequency_changed(void);

/** Drain RDSCore event queue into UI strings/state. */
void fred_fm_rds_process_events(void);

/** Run DSP + decoder on one deferred acquisition block. */
void fred_fm_rds_process_adc_block(const uint16_t* samples, size_t count, uint16_t adc_midpoint);

/** Deferred block callback registered with rds_acquisition. */
void fred_fm_rds_acquisition_block_callback(
    const uint16_t* samples,
    size_t count,
    uint16_t adc_midpoint,
    void* context);

/** Stop ADC acquisition (worker may still drain). */
void fred_fm_rds_adc_stop(void);

/** Spawn RdsDspWorker thread. */
void fred_fm_rds_dsp_worker_start(void);

/** 2 ms timer ISR callback; signals worker to drain pending blocks. */
void fred_fm_rds_adc_timer_callback(void* context);

/** Optional DSP symbol hook for constellation debug. */
void fred_fm_rds_symbol_callback(
    void* context,
    int32_t symbol_i,
    int32_t symbol_q,
    uint32_t confidence_q16);

/** Config menu: RDS on/off. */
void fred_fm_rds_change(VariableItem* item);

/** Config menu: RDS debug/capture mode. */
void fred_fm_rds_debug_change(VariableItem* item);

/** Short label for sync state (Search, Sync, etc.). */
const char* fred_fm_rds_sync_short_text(RdsSyncState state);
