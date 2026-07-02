/**
 * @file rds.c
 * @brief RDS decode pipeline, DSP worker, and runtime toggles.
 */
#include "src/fred_fm/include/config.h"
#include "src/fred_fm/include/types.h"
#include "src/fred_fm/core/core.h"
#include "src/fred_fm/rds/rds.h"
#include "src/fred_fm/rds/capture.h"
#include "src/fred_fm/ui/ui.h"

#include <furi.h>
#include <string.h>
#include "src/drivers/rds/RDSCore.h"
#include "src/drivers/rds/RDSDsp.h"
#include "src/drivers/rds/RDSAcquisition.h"

#ifdef ENABLE_RDS

/** Reset acquisition stats after tune or pipeline restart. */
void fred_fm_rds_runtime_reset(void) {
    rds_acquisition_reset(&rds_acquisition);
}

/** Init decoder, ADC/DMA, and capture writer (not on hot ISR path). */
void fred_fm_rds_pipeline_start(void) {
    fred_fm_rds_runtime_reset();

    rds_core_set_tick_ms(&rds_core, furi_get_tick());
    rds_core_reset(&rds_core);
    rds_dsp_init(&rds_dsp, RDS_ACQ_TARGET_SAMPLE_RATE_HZ);
    rds_dsp_set_symbol_callback(&rds_dsp, fred_fm_rds_symbol_callback, NULL);
    rds_dsp_set_manual_carrier_offset_centihz(&rds_dsp, rds_carrier_offset_centihz);

    rds_acquisition_init(
        &rds_acquisition,
        rds_adc_pin,
        rds_adc_channel,
        RDS_ADC_FIXED_MIDPOINT,
        fred_fm_rds_acquisition_block_callback,
        NULL);
    rds_acquisition_set_realtime_block_callback(
        &rds_acquisition, fred_fm_rds_acquisition_realtime_block_callback, NULL);

    /* Start acquisition (ADC/DMA) – timer will provide ticks for worker */
    if(!rds_acquisition_start(&rds_acquisition)) {
        FURI_LOG_E(TAG, "Failed to start RDS acquisition");
    }

    if(!fred_fm_rds_capture_writer_start()) {
        FURI_LOG_E(TAG, "Failed to start RDS capture writer");
    }

    fred_fm_rds_clear_station_name();
    fred_fm_rds_constellation_clear_history();
}

/** True when RDS decode or RDS Debug is enabled. */
bool fred_fm_rds_pipeline_enabled(void) {
    return rds_enabled || rds_debug_enabled;
}

/** Start 2 ms timer that wakes the DSP worker. */
void fred_fm_rds_timer_start(void) {
    if(rds_adc_timer_handle && !rds_adc_timer_running) {
        furi_timer_start(rds_adc_timer_handle, furi_ms_to_ticks(RDS_ACQ_TIMER_MS));
        rds_adc_timer_running = true;
    }
}

/** Stop the DSP worker timer. */
void fred_fm_rds_timer_stop(void) {
    if(rds_adc_timer_handle && rds_adc_timer_running) {
        furi_timer_stop(rds_adc_timer_handle);
        rds_adc_timer_running = false;
    }
}

/** Start/stop RDS pipeline when user toggles RDS or RDS Debug. */
void fred_fm_rds_apply_runtime_state(bool reset_decoder) {
    if(fred_fm_app_exiting) return;
    RdsAcquisitionStats stats;
    rds_acquisition_get_stats(&rds_acquisition, &stats);

    if(reset_decoder) {
        fred_fm_rds_clear_station_name();
        if(fred_fm_rds_pipeline_enabled()) {
            rds_core_set_tick_ms(&rds_core, furi_get_tick());
            rds_core_reset(&rds_core);
            rds_dsp_reset(&rds_dsp);
        }
    }

    if(fred_fm_rds_pipeline_enabled() && tea_i2c_ready) {
        if(!stats.running) {
            fred_fm_rds_pipeline_start();
        }
        fred_fm_rds_timer_start();
    } else {
        /* Stop producers before freeing capture ring / ADC. */
        rds_pipeline_stopping = true;
        fred_fm_rds_timer_stop();
        furi_delay_ms(20);
        fred_fm_rds_capture_stop();
        fred_fm_rds_adc_stop();
        rds_pipeline_stopping = false;
    }
}

/** Clear displayed PS name and reset sync indicator. */
void fred_fm_rds_clear_station_name(void) {
    fred_fm_state_lock();
    memset(rds_ps_display, 0, sizeof(rds_ps_display));
    rds_sync_display = RdsSyncStateSearch;
    fred_fm_state_unlock();
}

void fred_fm_rds_constellation_clear_history_locked(void) {
    rds_constellation_history_count = 0U;
    rds_constellation_history_write_index = 0U;
}

void fred_fm_rds_constellation_clear_history(void) {
    FURI_CRITICAL_ENTER();
    fred_fm_rds_constellation_clear_history_locked();
    FURI_CRITICAL_EXIT();
}

/** RDS symbol hook — records I/Q samples for constellation view. */
void fred_fm_rds_symbol_callback(
    void* context,
    int32_t symbol_i,
    int32_t symbol_q,
    uint32_t confidence_q16) {
    UNUSED(context);
    UNUSED(confidence_q16);

    if(!rds_debug_enabled || !rds_constellation_view_active) {
        return;
    }

    FURI_CRITICAL_ENTER();
    const uint8_t wr = rds_constellation_history_write_index;
    rds_constellation_i_history[wr] = symbol_i;
    rds_constellation_q_history[wr] = symbol_q;
    rds_constellation_history_write_index = (uint8_t)((wr + 1U) % RDS_CONSTELLATION_HISTORY_LEN);
    if(rds_constellation_history_count < RDS_CONSTELLATION_HISTORY_LEN) {
        rds_constellation_history_count++;
    }
    FURI_CRITICAL_EXIT();
}

/** Manual carrier offset in centihertz (constellation view). */
int16_t fred_fm_rds_get_manual_offset_centihz(void) {
    int16_t offset_centihz;
    fred_fm_state_lock();
    offset_centihz = rds_carrier_offset_centihz;
    fred_fm_state_unlock();
    return offset_centihz;
}

/** Set manual carrier offset and push it to the DSP. */
void fred_fm_rds_set_manual_offset_centihz(int16_t offset_centihz) {
    offset_centihz = fred_fm_clamp_manual_offset_centihz(offset_centihz);

    fred_fm_state_lock();
    rds_carrier_offset_centihz = offset_centihz;
    rds_dsp_set_manual_carrier_offset_centihz(&rds_dsp, offset_centihz);
    fred_fm_state_unlock();
}

/** Load offset from matching preset, or zero if not preset. */
void fred_fm_rds_sync_offset_from_current_frequency(void) {
    uint32_t freq_10khz = fred_fm_normalize_preset_freq_10khz(fred_fm_get_current_freq_10khz());
    int16_t offset_centihz = 0;
    bool preset_found = false;

    fred_fm_state_lock();
    for(uint8_t i = 0; i < preset_count; i++) {
        if(preset_freq_10khz[i] == freq_10khz) {
            preset_index = i;
            offset_centihz = preset_carrier_offset_centihz[i];
            preset_found = true;
            break;
        }
    }
    fred_fm_state_unlock();

    fred_fm_rds_set_manual_offset_centihz(preset_found ? offset_centihz : 0);
}

/** Copy decoder sync state into UI-safe snapshot. */
void fred_fm_rds_update_ui_snapshot(void) {
    fred_fm_state_lock();
    rds_sync_display = rds_core.sync_state;
    fred_fm_state_unlock();
}

/** Pick DSP sample rate from acquisition stats or default. */
uint32_t fred_fm_rds_select_runtime_sample_rate(const RdsAcquisitionStats* stats) {
    if(!stats) {
        return 0U;
    }

    if(stats->configured_sample_rate_hz != 0U) {
        return stats->configured_sample_rate_hz;
    }

    return RDS_DECODE_SAMPLE_RATE_HZ;
}

/** Re-init DSP when observed ADC rate changes. */
void fred_fm_rds_refresh_runtime_sample_rate(bool force_reset) {
    RdsAcquisitionStats stats;
    rds_acquisition_get_stats(&rds_acquisition, &stats);

    uint32_t next_rate_hz = fred_fm_rds_select_runtime_sample_rate(&stats);
    if(next_rate_hz == 0U) {
        return;
    }

    if(!force_reset && rds_runtime_sample_rate_hz == next_rate_hz) {
        return;
    }

    rds_runtime_sample_rate_hz = next_rate_hz;
    rds_dsp_init(&rds_dsp, next_rate_hz);
    rds_dsp_set_symbol_callback(&rds_dsp, fred_fm_rds_symbol_callback, NULL);
    rds_dsp_set_manual_carrier_offset_centihz(&rds_dsp, rds_carrier_offset_centihz);
}

/** Short sync-state label for the Listen screen. */
const char* fred_fm_rds_sync_short_text(RdsSyncState state) {
    switch(state) {
    case RdsSyncStateSearch:
        return "srch";
    case RdsSyncStatePreSync:
        return "pre";
    case RdsSyncStateSync:
        return "syn";
    case RdsSyncStateLost:
        return "lost";
    default:
        return "?";
    }
}

/** After tune/seek: reset RDS state and reload preset offset. */
void fred_fm_rds_on_tuned_frequency_changed(void) {
    fred_fm_rds_clear_station_name();
    fred_fm_rds_constellation_clear_history();
    if(fred_fm_rds_pipeline_enabled()) {
        fred_fm_rds_sync_offset_from_current_frequency();
        rds_core_set_tick_ms(&rds_core, furi_get_tick());
        rds_core_restart_sync(&rds_core);
        fred_fm_rds_refresh_runtime_sample_rate(true);
        fred_fm_rds_update_ui_snapshot();
        fred_fm_rds_runtime_reset();
    }
    fred_fm_settings_mark_dirty();
}

/** Drain decoder events (PS updates) on the main tick. */
void fred_fm_rds_process_events(void) {
    RdsEvent event;

    rds_core_set_tick_ms(&rds_core, furi_get_tick());

    while(rds_core_pop_event(&rds_core, &event)) {
        if(event.type == RdsEventTypePsUpdated) {
            fred_fm_state_lock();
            memcpy(rds_ps_display, event.ps, RDS_PS_LEN);
            rds_ps_display[RDS_PS_LEN] = '\0';
            fred_fm_state_unlock();
        }
    }

    fred_fm_rds_update_ui_snapshot();
}

/** Per-block ADC handler: optional capture, else RDS DSP. */
void fred_fm_rds_process_adc_block(const uint16_t* samples, size_t count, uint16_t adc_midpoint) {
    if(fred_fm_app_exiting || rds_pipeline_stopping) return;
    static uint8_t ui_snapshot_div = 0U;

#if ENABLE_ADC_CAPTURE
    if(rds_capture_requested && !rds_capture_active) {
        rds_capture_requested = false;
        fred_fm_rds_capture_start();
    }

    if(rds_capture_active) {
        fred_fm_rds_capture_write_block(samples, count);
        return;
    }
#endif

    if(!fred_fm_rds_pipeline_enabled()) return;

    rds_core_set_tick_ms(&rds_core, furi_get_tick());
    rds_dsp_process_u16_samples(&rds_dsp, &rds_core, samples, count, adc_midpoint);
    (void)ui_snapshot_div;
}

/** Deferred acquisition callback (same path as realtime ISR). */
void fred_fm_rds_acquisition_block_callback(
    const uint16_t* samples,
    size_t count,
    uint16_t adc_midpoint,
    void* context) {
    UNUSED(context);
    if(fred_fm_app_exiting || rds_pipeline_stopping) return;
    fred_fm_rds_process_adc_block(samples, count, adc_midpoint);
}

/** Stop ADC/DMA acquisition. */
void fred_fm_rds_adc_stop(void) {
    rds_acquisition_stop(&rds_acquisition);
}

/** Background thread: SD capture drain and acquisition timer ticks. */
int32_t fred_fm_rds_dsp_worker(void* context) {
    UNUSED(context);
    for(;;) {
        uint32_t flags = furi_thread_flags_wait(
            RDS_DSP_WORKER_FLAG_TICK | RDS_DSP_WORKER_FLAG_STOP, FuriFlagWaitAny, FuriWaitForever);
        if(flags & FuriFlagError) continue;
        if(flags & RDS_DSP_WORKER_FLAG_STOP) {
            break;
        }
        if(!(flags & RDS_DSP_WORKER_FLAG_TICK)) continue;

        if(fred_fm_app_exiting || rds_pipeline_stopping) continue;

#if ENABLE_ADC_CAPTURE
        if(fred_fm_app_exiting || rds_pipeline_stopping) continue;
        fred_fm_rds_capture_flush_to_sd();
        if(rds_capture_active) continue;
        if(!fred_fm_rds_pipeline_enabled() && !rds_capture_active && !rds_capture_requested)
            continue;
#else
        if(!fred_fm_rds_pipeline_enabled()) continue;
#endif

        /* Coalesce backlog ticks so RDS keeps up off the hot path. */
        if(fred_fm_app_exiting || rds_pipeline_stopping) {
            continue;
        }
        rds_acquisition_on_timer_tick(&rds_acquisition, true);
    }
    return 0;
}

/** Spawn the RDS DSP / capture worker thread. */
void fred_fm_rds_dsp_worker_start(void) {
    if(rds_dsp_worker_thread) return;
    rds_dsp_worker_thread = furi_thread_alloc_ex(
        "RdsDspWorker", RDS_DSP_WORKER_STACK_SIZE, fred_fm_rds_dsp_worker, NULL);
    if(!rds_dsp_worker_thread) return;
    furi_thread_set_priority(rds_dsp_worker_thread, FuriThreadPriorityLow);
    furi_thread_start(rds_dsp_worker_thread);
    rds_dsp_worker_thread_id = furi_thread_get_id(rds_dsp_worker_thread);
}

/** 2 ms timer ISR — flag worker for one tick. */
void fred_fm_rds_adc_timer_callback(void* context) {
    UNUSED(context);
    if(fred_fm_app_exiting) return;
    FuriThreadId id = rds_dsp_worker_thread_id;
    if(id) {
        furi_thread_flags_set(id, RDS_DSP_WORKER_FLAG_TICK);
    }
}

/** Config: RDS decode on/off. */
void fred_fm_rds_change(VariableItem* item) {
    UNUSED(variable_item_get_context(item));
    uint8_t index = variable_item_get_current_value_index(item);
    rds_enabled = (index != 0);
    variable_item_set_current_value_text(item, rds_enabled ? "On" : "Off");

    fred_fm_rds_apply_runtime_state(true);
    fred_fm_settings_mark_dirty();
    fred_fm_settings_save();
}

/** Config: RDS Debug (constellation + capture menu). */
void fred_fm_rds_debug_change(VariableItem* item) {
    FredFm* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    rds_debug_enabled = (index != 0);
    variable_item_set_current_value_text(item, rds_debug_enabled ? "On" : "Off");

    fred_fm_rds_apply_runtime_state(false);
    if(app && !fred_fm_submenu_rebuild(app)) {
        FURI_LOG_W(TAG, "Failed to rebuild submenu after RDS Debug change");
    }

    fred_fm_settings_mark_dirty();
    fred_fm_settings_save();
}

#endif
