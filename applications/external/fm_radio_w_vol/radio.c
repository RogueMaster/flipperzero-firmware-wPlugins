/**
 * @file radio.c
 * @brief FReD FM app for the Flipper Zero FM + RDS Radio Board.
 * @version 0.11
 */

#include <furi.h>
#include <furi_hal.h>
#include <stdint.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/elements.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/modules/variable_item_list.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include <storage/storage.h>
#include <flipper_format/flipper_format.h>

#include "TEA5767/TEA5767.h"
#include "PT/PT22xx.h"
#include "PAM/PAM8406.h"

// Set to 1 to enable RDS decoder (uses ADC on PA4, extra ~3 kB flash).
// Set to 0 to disable RDS completely (UI items hidden, no ADC).
#define ENABLE_RDS 1

// Set to 1 to enable raw ADC capture to SD card (long-press OK).
// Useful for offline spectrum analysis; disable for normal builds.
#define ENABLE_ADC_CAPTURE 1

#ifdef ENABLE_RDS
#include "RDS/RDSCore.h"
#include "RDS/RDSDsp.h"
#include "RDS/RDSAcquisition.h"
#endif

#define TAG                "fred_fm"
#define FRED_FM_UI_VERSION FAP_VERSION
// Volume config options (used by Config menu)
static const uint8_t volume_values[] = {0, 1};
static const char* volume_names[] = {"Un-Muted", "Muted"};
static bool current_volume = false;

static const PT22xxChip pt_chip_values[] = {PT22xxChipPT2257, PT22xxChipPT2259};
static const char* pt_chip_names[] = {"PT2257", "PT2259-S"};
static PT22xxChip pt_chip = PT22xxChipPT2257;

static const char* amp_power_names[] = {"Off", "On"};
static const char* amp_mode_names[] = {"AB", "D"};
static bool amp_power_enabled = true;
static bool amp_mode_class_d = false;
static FuriMutex* state_mutex = NULL;

typedef struct FredFm FredFm;

// Dedicated boards use a fixed 8-bit PT I2C address byte.
static const uint8_t pt_i2c_addr8 = 0x88;

// PT attenuation in dB: 0..79 (0 => max volume, 79 => min volume)
static uint8_t pt_atten_db = 20;
static bool pt_ready_cached = false;
static bool pt_initialized_cached = false;

/* Application exit flag — set true by the Back-button exit callback to make
 * timer/worker/ISR callbacks early-return during shutdown. Defined here
 * (before any helpers) so all guard checks can see it. */
static volatile bool fred_fm_app_exiting = false;

/* RDS pipeline stop barrier — set true by main thread BEFORE tearing down the
 * ADC/DMA producer (either on config change RDS ON->OFF or on app exit), and
 * paired with a furi_delay_ms() drain delay. Worker DSP thread, DMA ISR and
 * deferred block callbacks observe this flag and early-return, so by the time
 * the main thread proceeds to stop the DMA / release ADC handle / free the
 * capture ring, no other context is touching shared RDS state. Fixes the race
 * condition where rds_capture_ring could be freed while the DSP worker or
 * realtime ISR was still writing into it (use-after-free / HardFault). */
static volatile bool rds_pipeline_stopping = false;

static void fred_fm_state_lock(void);
static void fred_fm_state_unlock(void);
static uint32_t fred_fm_get_current_freq_10khz(void);
static void fred_fm_tune_nominal_freq_10khz(uint32_t freq_10khz);
static void fred_fm_sync_nominal_frequency_from_hardware(void);
static void fred_fm_apply_audio_output_state(void);
static void fred_fm_audio_shutdown(void);
static uint32_t fred_fm_navigation_exit_callback(void* context);
static uint32_t fred_fm_navigation_submenu_callback(void* context);
static void fred_fm_submenu_callback(void* context, uint32_t index);
static bool fred_fm_submenu_rebuild(FredFm* app);
#ifdef ENABLE_RDS
static void fred_fm_rds_on_tuned_frequency_changed(void);
static void fred_fm_rds_sync_offset_from_current_frequency(void);
void fred_fm_rds_process_adc_block(const uint16_t* samples, size_t count, uint16_t adc_midpoint);
static void fred_fm_rds_acquisition_block_callback(
    const uint16_t* samples,
    size_t count,
    uint16_t adc_midpoint,
    void* context);
static bool fred_fm_rds_acquisition_realtime_block_callback(
    const uint16_t* samples,
    size_t count,
    uint16_t adc_midpoint,
    void* context);
static void fred_fm_rds_adc_stop(void);
static void fred_fm_rds_adc_timer_callback(void* context);
static bool fred_fm_rds_pipeline_enabled(void);
static void fred_fm_rds_pipeline_start(void);
static void fred_fm_rds_constellation_clear_history(void);
static void fred_fm_rds_symbol_callback(
    void* context,
    int32_t symbol_i,
    int32_t symbol_q,
    uint32_t confidence_q16);
static void fred_fm_rds_timer_stop(void);
static void fred_fm_rds_update_ui_snapshot(void);
static const char* fred_fm_rds_sync_short_text(RdsSyncState state);
static void fred_fm_rds_clear_station_name(void);
static void fred_fm_rds_runtime_reset(void);
static void fred_fm_rds_runtime_meta_save(void);
static int16_t fred_fm_rds_get_manual_offset_centihz(void);
static void fred_fm_rds_set_manual_offset_centihz(int16_t offset_centihz);
#else
static inline void fred_fm_rds_on_tuned_frequency_changed(void) {
}
#endif

static void fred_fm_pt_apply_config(void) {
    pt22xx_set_chip(pt_chip);
    pt22xx_set_i2c_addr(pt_i2c_addr8);
}

static bool fred_fm_pt_refresh_state(bool force_init) {
    bool local_initialized;

    fred_fm_state_lock();
    local_initialized = pt_initialized_cached;
    fred_fm_state_unlock();

    fred_fm_pt_apply_config();
    bool ready = pt22xx_is_device_ready();

    if(!ready) {
        local_initialized = false;
    } else if(force_init || !local_initialized) {
        local_initialized = pt22xx_init();
        if(!local_initialized) ready = false;
    }

    fred_fm_state_lock();
    pt_ready_cached = ready;
    pt_initialized_cached = local_initialized;
    fred_fm_state_unlock();

    return ready;
}

static void fred_fm_apply_pt_state(void) {
    fred_fm_state_lock();
    bool local_ready = pt_ready_cached;
    bool local_muted = current_volume;
    uint8_t local_atten_db = pt_atten_db;
    fred_fm_state_unlock();

    if(!local_ready) return;

    PT22xxState state = {
        .attenuation_db = local_atten_db,
        .muted = local_muted,
    };

    (void)pt22xx_apply_state(&state);
}

static void fred_fm_apply_audio_output_state(void) {
    if(fred_fm_app_exiting) return;
    fred_fm_state_lock();
    bool local_muted = current_volume;
    bool local_amp_power = amp_power_enabled;
    bool local_amp_mode_class_d = amp_mode_class_d;
    fred_fm_state_unlock();

    fred_fm_apply_pt_state();

    PAM8406State amp_state = {
        .powered = local_amp_power,
        .muted = local_muted,
        .class_d_mode = local_amp_mode_class_d,
    };
    pam8406_apply_state(&amp_state);
}

static void fred_fm_audio_shutdown(void) {
    bool local_ready = false;
    uint8_t local_atten_db = pt_atten_db;

    if(state_mutex) {
        fred_fm_state_lock();
        local_ready = pt_ready_cached;
        local_atten_db = pt_atten_db;
        fred_fm_state_unlock();
    }

    if(local_ready) {
        PT22xxState state = {
            .attenuation_db = local_atten_db,
            .muted = true,
        };
        (void)pt22xx_apply_state(&state);
    }

    pam8406_shutdown();
}

#define SETTINGS_FILE     APP_DATA_PATH("settings.fff")
#define SETTINGS_FILETYPE "FReD FM Settings"
#define SETTINGS_VERSION  (6U)
#ifdef ENABLE_RDS
#define RDS_RUNTIME_META_FILE    APP_DATA_PATH("rds_runtime_meta.txt")
/* Master switch for runtime_meta collection and save path. Set to 1U to re-enable. */
#define RDS_RUNTIME_META_ENABLED 0U
#endif

#define PRESETS_FILE                      APP_DATA_PATH("presets.fff")
#define PRESETS_FILETYPE                  "FReD FM Presets"
#define PRESETS_VERSION                   (1U)
#define PRESETS_MAX                       (32U)
#define PRESET_OFFSET_CENTIHZ_BIAS_CENTER (600U)
#define PRESET_OFFSET_BIAS_MIN            (0U)
#define PRESET_OFFSET_CENTIHZ_BIAS_MAX    (1200U)

#define RDS_MANUAL_OFFSET_MIN_CENTIHZ (-600)
#define RDS_MANUAL_OFFSET_MAX_CENTIHZ (600)

#define RDS_CONSTELLATION_HISTORY_LEN (192U)
#define TEA_I2C_RDS_FAILURE_LIMIT     (8U)
#define RDS_CONSTELLATION_CENTER_X    (64)
#define RDS_CONSTELLATION_CENTER_Y    (36)
#define RDS_CONSTELLATION_PLOT_LEFT   (2)
#define RDS_CONSTELLATION_PLOT_RIGHT  (125)
#define RDS_CONSTELLATION_PLOT_TOP    (10)
#define RDS_CONSTELLATION_PLOT_BOTTOM (62)

static bool settings_dirty = false;

static bool tea_snc_enabled = false;
static bool tea_deemph_75us = false;
static bool tea_highcut_enabled = false;
static bool tea_force_mono_enabled = false;
#ifdef ENABLE_RDS
static bool rds_enabled = true;
static bool rds_debug_enabled = false;
#endif

static bool backlight_keep_on = false;

static uint32_t preset_freq_10khz[PRESETS_MAX];
static int16_t preset_carrier_offset_centihz[PRESETS_MAX];
static uint8_t preset_count = 0;
static uint8_t preset_index = 0;
static bool presets_dirty = false;

static uint32_t seek_last_step_tick = 0;
static uint32_t tea_nominal_freq_10khz = 8750U;
static uint32_t tea_last_tune_tick = 0U;

#define TEA_IF_COUNT_TARGET 0x37U

/* Cached TEA5767 radio info — refreshed every tick, consumed by draw callback */
static struct RADIO_INFO tea_info_cached;
static bool tea_info_valid = false;
static bool tea_i2c_ready = false;
/* fred_fm_app_exiting: defined earlier (line ~74) so helpers can early-return. */
static uint32_t tea_info_read_count = 0;

#ifdef ENABLE_RDS
static RDSCore rds_core;
static RDSDsp rds_dsp;
static RdsAcquisition rds_acquisition;
static int16_t rds_carrier_offset_centihz = 0;
static int32_t rds_constellation_i_history[RDS_CONSTELLATION_HISTORY_LEN];
static int32_t rds_constellation_q_history[RDS_CONSTELLATION_HISTORY_LEN];
static int32_t rds_constellation_i_snapshot[RDS_CONSTELLATION_HISTORY_LEN];
static int32_t rds_constellation_q_snapshot[RDS_CONSTELLATION_HISTORY_LEN];
static uint8_t rds_constellation_history_count = 0U;
static uint8_t rds_constellation_history_write_index = 0U;
static uint32_t rds_constellation_saved_until_tick = 0U;
static bool rds_constellation_view_active = false;
static uint32_t rds_runtime_sample_rate_hz = 0U;
static char rds_ps_display[RDS_PS_LEN + 1U];
static RdsSyncState rds_sync_display = RdsSyncStateSearch;
#define RDS_ADC_FIXED_MIDPOINT 2048U
static const GpioPin* rds_adc_pin = &gpio_ext_pa4;
static FuriHalAdcChannel rds_adc_channel = FuriHalAdcChannel9;
static FuriTimer* rds_adc_timer_handle = NULL;
static bool rds_adc_timer_running = false;
#define RDS_DSP_WORKER_STACK_SIZE 4096U
#define RDS_DSP_WORKER_FLAG_TICK  0x01U
#define RDS_DSP_WORKER_FLAG_STOP  0x02U
static FuriThread* rds_dsp_worker_thread = NULL;
static volatile FuriThreadId rds_dsp_worker_thread_id = NULL;
static uint8_t tea_i2c_failure_count = 0U;
#if RDS_RUNTIME_META_ENABLED
static uint32_t rds_dsp_block_count = 0U;
static uint32_t rds_dsp_block_total_ms = 0U;
static uint32_t rds_dsp_block_max_ms = 0U;
static uint32_t rds_dsp_last_block_ms = 0U;
#endif

#endif

// SEEK pacing / settling for TEA5767
#define SEEK_MIN_INTERVAL_MS  2500
#define SEEK_SETTLE_DELAY_MS  250
#define SEEK_READY_TIMEOUT_MS 1800
#define SEEK_READY_POLL_MS    50

static uint32_t clamp_u32(uint32_t value, uint32_t min, uint32_t max) {
    if(value < min) return min;
    if(value > max) return max;
    return value;
}

static void fred_fm_state_lock(void) {
    if(state_mutex) {
        (void)furi_mutex_acquire(state_mutex, FuriWaitForever);
    }
}

static void fred_fm_state_unlock(void) {
    if(state_mutex) {
        (void)furi_mutex_release(state_mutex);
    }
}

static uint32_t fred_fm_get_current_freq_10khz(void) {
    uint32_t freq_10khz = tea_nominal_freq_10khz;

    if(freq_10khz < 7600U || freq_10khz > 10800U) {
        freq_10khz = 8750U;
    }

    return clamp_u32(freq_10khz, 7600U, 10800U);
}

static void fred_fm_tune_nominal_freq_10khz(uint32_t freq_10khz) {
    uint32_t now = furi_get_tick();

    freq_10khz = clamp_u32(freq_10khz, 7600U, 10800U);

    fred_fm_state_lock();
    tea_nominal_freq_10khz = freq_10khz;
    tea_last_tune_tick = now;
    fred_fm_state_unlock();

    tea5767_SetFreqMHz(((float)freq_10khz) / 100.0f);
}

static void fred_fm_sync_nominal_frequency_from_hardware(void) {
    float freq = tea5767_GetFreq();
    if(freq <= 0.0f) {
        return;
    }

    fred_fm_state_lock();
    tea_nominal_freq_10khz = clamp_u32((uint32_t)(freq * 100.0f), 7600U, 10800U);
    tea_last_tune_tick = furi_get_tick();
    fred_fm_state_unlock();
}

static void fred_fm_settings_mark_dirty(void) {
    settings_dirty = true;
}

static void fred_fm_presets_mark_dirty(void) {
    presets_dirty = true;
}

static uint32_t fred_fm_normalize_preset_freq_10khz(uint32_t freq_10khz) {
    freq_10khz = clamp_u32(freq_10khz, 7600U, 10800U);
    return clamp_u32(((freq_10khz + 5U) / 10U) * 10U, 7600U, 10800U);
}

static int16_t fred_fm_clamp_manual_offset_centihz(int16_t offset_centihz) {
    if(offset_centihz < RDS_MANUAL_OFFSET_MIN_CENTIHZ) {
        return RDS_MANUAL_OFFSET_MIN_CENTIHZ;
    }
    if(offset_centihz > RDS_MANUAL_OFFSET_MAX_CENTIHZ) {
        return RDS_MANUAL_OFFSET_MAX_CENTIHZ;
    }
    return offset_centihz;
}

static uint32_t fred_fm_preset_offset_to_bias600(int16_t offset_centihz) {
    int16_t clamped = fred_fm_clamp_manual_offset_centihz(offset_centihz);
    return (uint32_t)(clamped + (int16_t)PRESET_OFFSET_CENTIHZ_BIAS_CENTER);
}

static int16_t fred_fm_preset_offset_from_bias600(uint32_t bias600) {
    uint32_t clamped = clamp_u32(bias600, PRESET_OFFSET_BIAS_MIN, PRESET_OFFSET_CENTIHZ_BIAS_MAX);
    return (int16_t)((int32_t)clamped - (int32_t)PRESET_OFFSET_CENTIHZ_BIAS_CENTER);
}

static bool fred_fm_preset_find(uint32_t freq_10khz, uint8_t* found_index) {
    freq_10khz = fred_fm_normalize_preset_freq_10khz(freq_10khz);

    for(uint8_t i = 0; i < preset_count; i++) {
        if(preset_freq_10khz[i] == freq_10khz) {
            if(found_index) *found_index = i;
            return true;
        }
    }
    return false;
}

static void fred_fm_ensure_app_data_dir(Storage* storage) {
    if(!storage) return;
    FuriString* path = furi_string_alloc_set(APP_DATA_PATH("."));
    storage_common_resolve_path_and_ensure_app_directory(storage, path);
    furi_string_free(path);
}

static void fred_fm_presets_load(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    FuriString* filetype = furi_string_alloc();
    uint32_t version = 0;
    bool normalized_or_deduped = false;

    memset(preset_freq_10khz, 0, sizeof(preset_freq_10khz));
    memset(preset_carrier_offset_centihz, 0, sizeof(preset_carrier_offset_centihz));
    preset_count = 0;
    preset_index = 0;

    do {
        if(!flipper_format_file_open_existing(ff, PRESETS_FILE)) break;
        if(!flipper_format_read_header(ff, filetype, &version)) break;
        if(version != PRESETS_VERSION) break;

        uint32_t count = 0;
        if(!flipper_format_read_uint32(ff, "Count", &count, 1)) break;
        if(count > PRESETS_MAX) count = PRESETS_MAX;

        uint32_t idx = 0;
        uint32_t raw_freqs[PRESETS_MAX] = {0};
        uint32_t raw_offsets_bias600[PRESETS_MAX] = {0};

        if(count > 0) {
            if(!flipper_format_read_uint32(ff, "Freq10kHz", raw_freqs, (uint16_t)count)) break;
            if(!flipper_format_read_uint32(
                   ff, "CarrierOffsetCentiHzBias600", raw_offsets_bias600, (uint16_t)count))
                break;
            if(!flipper_format_read_uint32(ff, "Index", &idx, 1)) break;
            idx = clamp_u32(idx, 0, (count > 0U) ? (count - 1U) : 0U);
        }

        for(uint32_t i = 0; i < count; i++) {
            uint32_t normalized = fred_fm_normalize_preset_freq_10khz(raw_freqs[i]);
            int16_t offset_centihz = fred_fm_preset_offset_from_bias600(raw_offsets_bias600[i]);

            if(normalized != raw_freqs[i]) {
                normalized_or_deduped = true;
            }

            uint8_t existing_index = 0;
            if(fred_fm_preset_find(normalized, &existing_index)) {
                normalized_or_deduped = true;
                if(i == idx) {
                    preset_index = existing_index;
                    preset_carrier_offset_centihz[existing_index] = offset_centihz;
                }
                continue;
            }

            if(preset_count < PRESETS_MAX) {
                preset_freq_10khz[preset_count] = normalized;
                preset_carrier_offset_centihz[preset_count] = offset_centihz;
                if(i == idx) {
                    preset_index = preset_count;
                }
                preset_count++;
            }
        }

        if(preset_count > 0) {
            preset_index = (uint8_t)clamp_u32(preset_index, 0, preset_count - 1);
        } else {
            preset_index = 0;
        }

        if(normalized_or_deduped) {
            presets_dirty = true;
        }
    } while(false);

    flipper_format_file_close(ff);
    flipper_format_free(ff);
    furi_string_free(filetype);
    furi_record_close(RECORD_STORAGE);
}

static void fred_fm_presets_save(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    fred_fm_ensure_app_data_dir(storage);
    FlipperFormat* ff = flipper_format_file_alloc(storage);

    bool ok = false;
    do {
        (void)storage_simply_remove(storage, PRESETS_FILE);
        if(!flipper_format_file_open_new(ff, PRESETS_FILE)) break;
        if(!flipper_format_write_header_cstr(ff, PRESETS_FILETYPE, PRESETS_VERSION)) break;

        uint32_t count = preset_count;
        if(!flipper_format_write_uint32(ff, "Count", &count, 1)) break;
        if(preset_count > 0) {
            uint32_t offsets_bias600[PRESETS_MAX] = {0};
            for(uint8_t i = 0; i < preset_count; i++) {
                offsets_bias600[i] =
                    fred_fm_preset_offset_to_bias600(preset_carrier_offset_centihz[i]);
            }

            if(!flipper_format_write_uint32(ff, "Freq10kHz", preset_freq_10khz, preset_count))
                break;
            if(!flipper_format_write_uint32(
                   ff, "CarrierOffsetCentiHzBias600", offsets_bias600, preset_count))
                break;
            uint32_t idx = preset_index;
            if(!flipper_format_write_uint32(ff, "Index", &idx, 1)) break;
        }

        ok = true;
    } while(false);

    flipper_format_file_close(ff);
    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);

    if(ok) presets_dirty = false;
}

static void fred_fm_feedback_success(void) {
    NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);
    notification_message(notifications, &sequence_success);
    furi_record_close(RECORD_NOTIFICATION);
}

static void fred_fm_presets_add_or_select(uint32_t freq_10khz, int16_t offset_centihz) {
    freq_10khz = fred_fm_normalize_preset_freq_10khz(freq_10khz);
    offset_centihz = fred_fm_clamp_manual_offset_centihz(offset_centihz);

    uint8_t found = 0;
    if(fred_fm_preset_find(freq_10khz, &found)) {
        preset_index = found;
        if(preset_carrier_offset_centihz[found] != offset_centihz) {
            preset_carrier_offset_centihz[found] = offset_centihz;
            fred_fm_presets_mark_dirty();
        }
        return;
    }

    if(preset_count < PRESETS_MAX) {
        preset_index = preset_count;
        preset_freq_10khz[preset_count] = freq_10khz;
        preset_carrier_offset_centihz[preset_count] = offset_centihz;
        preset_count++;
    } else {
        if(preset_index >= PRESETS_MAX) preset_index = 0;
        preset_freq_10khz[preset_index] = freq_10khz;
        preset_carrier_offset_centihz[preset_index] = offset_centihz;
    }

    fred_fm_presets_mark_dirty();
}

static bool fred_fm_presets_step_and_apply(bool forward) {
    uint32_t preset_freq_10khz_local = 0U;
    int16_t preset_offset_centihz_local = 0;

    fred_fm_state_lock();
    if(preset_count == 0U) {
        fred_fm_state_unlock();
        return false;
    }

    if(forward) {
        preset_index = (preset_index + 1U) % preset_count;
    } else if(preset_index == 0U) {
        preset_index = preset_count - 1U;
    } else {
        preset_index--;
    }

    preset_freq_10khz_local = preset_freq_10khz[preset_index];
    preset_offset_centihz_local = preset_carrier_offset_centihz[preset_index];
    fred_fm_presets_mark_dirty();
    fred_fm_state_unlock();

#ifdef ENABLE_RDS
    fred_fm_rds_set_manual_offset_centihz(preset_offset_centihz_local);
#else
    UNUSED(preset_offset_centihz_local);
#endif
    fred_fm_tune_nominal_freq_10khz(preset_freq_10khz_local);
    fred_fm_rds_on_tuned_frequency_changed();
    return true;
}

static void fred_fm_seek_step(bool direction_up) {
    // SEEK-only behavior (scan logic disabled by request).
    // Debounce repeated long-hold events so seek does not run too fast.
    uint32_t now = furi_get_tick();
    if((now - seek_last_step_tick) < furi_ms_to_ticks(SEEK_MIN_INTERVAL_MS)) {
        return;
    }

    uint32_t cur = fred_fm_get_current_freq_10khz();
    uint32_t next = direction_up ? clamp_u32(cur + 10U, 7600U, 10800U) :
                                   ((cur > 7610U) ? (cur - 10U) : 7600U);

    fred_fm_tune_nominal_freq_10khz(next);
    tea5767_seekFrom10kHz(next, direction_up);
    seek_last_step_tick = now;

    // Let PLL/status settle, then poll READY (byte0 bit7) briefly.
    // This improves reliability on some TEA5767 modules.
    furi_delay_ms(SEEK_SETTLE_DELAY_MS);
    uint8_t tea_buffer[5];
    uint32_t wait_start = furi_get_tick();
    while((furi_get_tick() - wait_start) < furi_ms_to_ticks(SEEK_READY_TIMEOUT_MS)) {
        if(tea5767_read_registers(tea_buffer)) {
            bool ready = (tea_buffer[0] & 0x80) != 0;
            if(ready) {
                break;
            }
        }
        furi_delay_ms(SEEK_READY_POLL_MS);
    }

    fred_fm_sync_nominal_frequency_from_hardware();
    fred_fm_rds_on_tuned_frequency_changed();
}

static void fred_fm_settings_load(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    FuriString* filetype = furi_string_alloc();
    uint32_t version = 0;

    do {
        if(!flipper_format_file_open_existing(ff, SETTINGS_FILE)) break;
        if(!flipper_format_read_header(ff, filetype, &version)) break;
        if(version != SETTINGS_VERSION) {
            FURI_LOG_W(
                TAG,
                "Ignoring settings version=%lu expected=%lu",
                (unsigned long)version,
                (unsigned long)SETTINGS_VERSION);
            break;
        }

        if(!flipper_format_read_bool(ff, "TeaSNC", &tea_snc_enabled, 1)) break;
        tea5767_set_snc_enabled(tea_snc_enabled);

        if(!flipper_format_read_bool(ff, "TeaDeemph75us", &tea_deemph_75us, 1)) break;
        tea5767_set_deemphasis_75us_enabled(tea_deemph_75us);

        if(!flipper_format_read_bool(ff, "TeaHighCut", &tea_highcut_enabled, 1)) break;
        tea5767_set_high_cut_enabled(tea_highcut_enabled);

        if(!flipper_format_read_bool(ff, "TeaForceMono", &tea_force_mono_enabled, 1)) break;
        tea5767_set_force_mono_enabled(tea_force_mono_enabled);

        if(!flipper_format_read_bool(ff, "BacklightKeepOn", &backlight_keep_on, 1)) break;

#ifdef ENABLE_RDS
        if(!flipper_format_read_bool(ff, "RdsEnabled", &rds_enabled, 1)) break;
        if(!flipper_format_read_bool(ff, "RdsDebugEnabled", &rds_debug_enabled, 1)) break;
#endif

        uint32_t freq_10khz = 0;
        if(!flipper_format_read_uint32(ff, "Freq10kHz", &freq_10khz, 1)) break;
        freq_10khz = clamp_u32(freq_10khz, 7600U, 10800U);
        fred_fm_tune_nominal_freq_10khz(freq_10khz);

        uint32_t atten = 0;
        if(!flipper_format_read_uint32(ff, "PtAttenDb", &atten, 1)) break;
        if(atten > 79) atten = 79;
        pt_atten_db = (uint8_t)atten;

        if(!flipper_format_read_bool(ff, "PtMuted", &current_volume, 1)) break;

        uint32_t chip_type = 0;
        if(!flipper_format_read_uint32(ff, "PtChipType", &chip_type, 1)) break;
        if(chip_type <= (uint32_t)PT22xxChipPT2259) {
            pt_chip = (PT22xxChip)chip_type;
        }

        if(!flipper_format_read_bool(ff, "AmpPower", &amp_power_enabled, 1)) break;
        if(!flipper_format_read_bool(ff, "AmpModeClassD", &amp_mode_class_d, 1)) break;

        FURI_LOG_I(
            TAG,
            "Settings loaded: version=%lu rds=%u rds_debug=%u",
            (unsigned long)version,
            (unsigned)rds_enabled,
            (unsigned)rds_debug_enabled);

    } while(false);

    flipper_format_file_close(ff);
    flipper_format_free(ff);
    furi_string_free(filetype);
    furi_record_close(RECORD_STORAGE);

#ifdef ENABLE_RDS
    fred_fm_rds_sync_offset_from_current_frequency();
#endif
}

static void fred_fm_settings_save(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    fred_fm_ensure_app_data_dir(storage);
    FlipperFormat* ff = flipper_format_file_alloc(storage);

    bool ok = false;
    do {
        (void)storage_simply_remove(storage, SETTINGS_FILE);
        if(!flipper_format_file_open_new(ff, SETTINGS_FILE)) break;
        if(!flipper_format_write_header_cstr(ff, SETTINGS_FILETYPE, SETTINGS_VERSION)) break;

        bool snc = tea_snc_enabled;
        if(!flipper_format_write_bool(ff, "TeaSNC", &snc, 1)) break;

        bool deemph_75 = tea_deemph_75us;
        if(!flipper_format_write_bool(ff, "TeaDeemph75us", &deemph_75, 1)) break;

        bool highcut = tea_highcut_enabled;
        if(!flipper_format_write_bool(ff, "TeaHighCut", &highcut, 1)) break;

        bool mono = tea_force_mono_enabled;
        if(!flipper_format_write_bool(ff, "TeaForceMono", &mono, 1)) break;

        bool bl = backlight_keep_on;
        if(!flipper_format_write_bool(ff, "BacklightKeepOn", &bl, 1)) break;

#ifdef ENABLE_RDS
        bool rds = rds_enabled;
        if(!flipper_format_write_bool(ff, "RdsEnabled", &rds, 1)) break;

        bool rds_debug = rds_debug_enabled;
        if(!flipper_format_write_bool(ff, "RdsDebugEnabled", &rds_debug, 1)) break;
#endif

        uint32_t freq_10khz = fred_fm_get_current_freq_10khz();
        if(!flipper_format_write_uint32(ff, "Freq10kHz", &freq_10khz, 1)) break;

        uint32_t atten = pt_atten_db;
        if(!flipper_format_write_uint32(ff, "PtAttenDb", &atten, 1)) break;

        bool muted = (current_volume != 0);
        if(!flipper_format_write_bool(ff, "PtMuted", &muted, 1)) break;

        uint32_t chip_type = (uint32_t)pt_chip;
        if(!flipper_format_write_uint32(ff, "PtChipType", &chip_type, 1)) break;

        bool amp_power = amp_power_enabled;
        if(!flipper_format_write_bool(ff, "AmpPower", &amp_power, 1)) break;

        bool amp_mode_d = amp_mode_class_d;
        if(!flipper_format_write_bool(ff, "AmpModeClassD", &amp_mode_d, 1)) break;

        ok = true;
    } while(false);

    flipper_format_file_close(ff);
    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);

    if(ok) {
        settings_dirty = false;
        FURI_LOG_I(
            TAG,
            "Settings saved: rds=%u rds_debug=%u",
            (unsigned)rds_enabled,
            (unsigned)rds_debug_enabled);
    } else {
        FURI_LOG_E(TAG, "Settings save failed");
    }
}

static void fred_fm_apply_backlight(NotificationApp* notifications) {
    if(!notifications) return;
    if(backlight_keep_on) {
        notification_message(notifications, &sequence_display_backlight_enforce_on);
    } else {
        notification_message(notifications, &sequence_display_backlight_enforce_auto);
    }
}

#ifdef ENABLE_RDS

#if ENABLE_ADC_CAPTURE
/* ── ADC raw capture to SD card ─────────────────────────────────────────
 * Records raw ADC samples to SD for offline spectrum analysis.
 * Activated by long-press OK (works with RDS on or off).
 * Uses a small ring buffer in RAM and streams to SD during capture. */
#define RDS_CAPTURE_FILE_TEMPLATE      APP_DATA_PATH("rds_capture_u16le_%04lu.raw")
#define RDS_CAPTURE_META_FILE_TEMPLATE APP_DATA_PATH("rds_capture_meta_%04lu.txt")
#define RDS_CAPTURE_PATH_MAX           128U
#define RDS_CAPTURE_MAX_INDEX          9999U
#define RDS_CAPTURE_SAMPLE_BYTES       sizeof(uint16_t)
#define RDS_CAPTURE_SAMPLE_BITS        16U
#define RDS_CAPTURE_SAMPLE_FORMAT      "u16le"
#define RDS_CAPTURE_TARGET_BLOCKS      512U
#define RDS_CAPTURE_TARGET_SAMPLES     (RDS_CAPTURE_TARGET_BLOCKS * RDS_ACQ_BLOCK_SAMPLES)
#define RDS_CAPTURE_TARGET_BYTES       (RDS_CAPTURE_TARGET_SAMPLES * RDS_CAPTURE_SAMPLE_BYTES)
#define RDS_CAPTURE_RING_MIN_BLOCKS    8U
#define RDS_CAPTURE_RING_MAX_BLOCKS    128U
#define RDS_CAPTURE_HEAP_RESERVE_BYTES (16U * 1024U)
#define RDS_CAPTURE_WRITER_STACK_SIZE  2048U
#define RDS_CAPTURE_WRITER_PRIORITY    FuriThreadPriorityNormal
#define RDS_CAPTURE_WRITER_FLAG_WORK   (1U << 0)
#define RDS_CAPTURE_WRITER_FLAG_STOP   (1U << 1)

static volatile bool rds_capture_active = false;
static volatile bool rds_capture_requested = false;
static volatile bool rds_capture_finalize_pending = false;
static volatile bool rds_capture_abort_pending = false;
static volatile bool rds_capture_error = false;
static volatile bool rds_capture_complete = false;
static uint16_t* rds_capture_ring = NULL;
static uint32_t rds_capture_ring_capacity_blocks = 0U;
static uint32_t rds_capture_ring_head_block = 0U;
static uint32_t rds_capture_ring_tail_block = 0U;
static uint32_t rds_capture_ring_count_blocks = 0U;
static uint32_t rds_capture_ring_peak_blocks = 0U;
static uint32_t rds_capture_target_blocks = RDS_CAPTURE_TARGET_BLOCKS;
static uint32_t rds_capture_captured_blocks = 0U;
static uint32_t rds_capture_written_blocks = 0U;
static uint32_t rds_capture_capture_samples = 0U;
static uint32_t rds_capture_ring_overflow_blocks = 0U;
static uint32_t rds_capture_storage_write_errors = 0U;
static uint32_t rds_capture_free_heap_bytes = 0U;
static uint32_t rds_capture_write_call_count = 0U;
static uint32_t rds_capture_write_total_ms = 0U;
static uint32_t rds_capture_write_max_ms = 0U;
static uint32_t rds_capture_write_max_bytes = 0U;
static uint32_t rds_capture_write_total_bytes = 0U;
static uint16_t rds_capture_min = 0U;
static uint16_t rds_capture_max = 0U;
static uint32_t rds_capture_clip_4095 = 0U;
static uint64_t rds_capture_sum = 0U;
static bool rds_capture_stats_valid = false;
static RdsAcquisitionStats rds_capture_acq_start_stats;
static uint32_t rds_capture_start_tick = 0U;
static uint32_t rds_capture_stop_tick = 0U;
static uint16_t rds_capture_pending_peak_blocks = 0U;
static File* rds_capture_file = NULL;
static Storage* rds_capture_storage = NULL;
static FuriThread* rds_capture_writer_thread = NULL;
static char rds_capture_file_path[RDS_CAPTURE_PATH_MAX] = {0};
static char rds_capture_meta_file_path[RDS_CAPTURE_PATH_MAX] = {0};
static uint32_t rds_capture_file_index = 0U;

static void fred_fm_rds_capture_signal_writer(uint32_t flags) {
    if(!rds_capture_writer_thread) return;
    FuriThreadId thread_id = furi_thread_get_id(rds_capture_writer_thread);
    if(thread_id) {
        furi_thread_flags_set(thread_id, flags);
    }
}

static void fred_fm_rds_capture_close_file(void) {
    if(rds_capture_file) {
        storage_file_close(rds_capture_file);
        storage_file_free(rds_capture_file);
        rds_capture_file = NULL;
    }
    if(rds_capture_storage) {
        furi_record_close(RECORD_STORAGE);
        rds_capture_storage = NULL;
    }
}

static bool fred_fm_rds_capture_path_exists(Storage* storage, const char* path) {
    bool exists = false;

    if(!storage || !path || path[0] == '\0') {
        return false;
    }

    File* file = storage_file_alloc(storage);
    if(!file) {
        return false;
    }

    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        exists = true;
        storage_file_close(file);
    }

    storage_file_free(file);
    return exists;
}

static bool fred_fm_rds_capture_prepare_paths(Storage* storage) {
    if(!storage) {
        return false;
    }

    for(uint32_t index = 1U; index <= RDS_CAPTURE_MAX_INDEX; index++) {
        int raw_len = snprintf(
            rds_capture_file_path,
            sizeof(rds_capture_file_path),
            RDS_CAPTURE_FILE_TEMPLATE,
            (unsigned long)index);
        int meta_len = snprintf(
            rds_capture_meta_file_path,
            sizeof(rds_capture_meta_file_path),
            RDS_CAPTURE_META_FILE_TEMPLATE,
            (unsigned long)index);

        if(raw_len <= 0 || meta_len <= 0 || raw_len >= (int)sizeof(rds_capture_file_path) ||
           meta_len >= (int)sizeof(rds_capture_meta_file_path)) {
            rds_capture_file_path[0] = '\0';
            rds_capture_meta_file_path[0] = '\0';
            return false;
        }

        if(!fred_fm_rds_capture_path_exists(storage, rds_capture_file_path) &&
           !fred_fm_rds_capture_path_exists(storage, rds_capture_meta_file_path)) {
            rds_capture_file_index = index;
            return true;
        }
    }

    rds_capture_file_path[0] = '\0';
    rds_capture_meta_file_path[0] = '\0';
    return false;
}

static bool fred_fm_rds_capture_preallocate_file(void) {
    if(!rds_capture_file) return false;

    const uint8_t zero = 0U;
    const uint32_t last_byte_offset = (uint32_t)RDS_CAPTURE_TARGET_BYTES - 1U;

    if(!storage_file_seek(rds_capture_file, last_byte_offset, true)) {
        return false;
    }
    if(storage_file_write(rds_capture_file, &zero, sizeof(zero)) != sizeof(zero)) {
        return false;
    }
    if(!storage_file_seek(rds_capture_file, 0U, true)) {
        return false;
    }

    return true;
}

static bool fred_fm_rds_capture_trim_file_to_written_size(void) {
    if(!rds_capture_file) return false;

    const uint32_t bytes_written =
        rds_capture_written_blocks * RDS_ACQ_BLOCK_SAMPLES * RDS_CAPTURE_SAMPLE_BYTES;

    /* The RAW file is preallocated to the full target size up front.
       When capture reaches the full target, trimming is a no-op and some
       cards/filesystem states intermittently fail the extra truncate step. */
    if(bytes_written >= RDS_CAPTURE_TARGET_BYTES) {
        return true;
    }

    if(!storage_file_seek(rds_capture_file, bytes_written, true)) {
        return false;
    }
    if(!storage_file_truncate(rds_capture_file)) {
        return false;
    }

    return true;
}

static uint32_t fred_fm_rds_capture_select_ring_blocks(void) {
    const size_t block_bytes = RDS_ACQ_BLOCK_SAMPLES * RDS_CAPTURE_SAMPLE_BYTES;
    size_t free_heap = memmgr_get_free_heap();
    size_t usable_heap = 0U;
    uint32_t blocks = RDS_CAPTURE_RING_MIN_BLOCKS;

    rds_capture_free_heap_bytes = (uint32_t)free_heap;

    if(free_heap > RDS_CAPTURE_HEAP_RESERVE_BYTES) {
        usable_heap = free_heap - RDS_CAPTURE_HEAP_RESERVE_BYTES;
    }

    if(block_bytes > 0U) {
        size_t max_blocks = usable_heap / block_bytes;
        if(max_blocks > RDS_CAPTURE_RING_MAX_BLOCKS) {
            max_blocks = RDS_CAPTURE_RING_MAX_BLOCKS;
        }
        if(max_blocks > RDS_CAPTURE_TARGET_BLOCKS) {
            max_blocks = RDS_CAPTURE_TARGET_BLOCKS;
        }
        if(max_blocks >= RDS_CAPTURE_RING_MIN_BLOCKS) {
            blocks = (uint32_t)max_blocks;
        }
    }

    return blocks;
}

static void fred_fm_rds_capture_release_ring(void) {
    if(rds_capture_ring) {
        free(rds_capture_ring);
        rds_capture_ring = NULL;
    }
    rds_capture_ring_capacity_blocks = 0U;
    rds_capture_ring_head_block = 0U;
    rds_capture_ring_tail_block = 0U;
    rds_capture_ring_count_blocks = 0U;
    rds_capture_ring_peak_blocks = 0U;
}

static void fred_fm_rds_capture_clear_state_fields(void) {
    rds_capture_active = false;
    rds_capture_requested = false;
    rds_capture_finalize_pending = false;
    rds_capture_abort_pending = false;
    rds_capture_error = false;
    rds_capture_complete = false;
    rds_capture_target_blocks = RDS_CAPTURE_TARGET_BLOCKS;
    rds_capture_captured_blocks = 0U;
    rds_capture_written_blocks = 0U;
    rds_capture_capture_samples = 0U;
    rds_capture_ring_overflow_blocks = 0U;
    rds_capture_storage_write_errors = 0U;
    rds_capture_free_heap_bytes = 0U;
    rds_capture_write_call_count = 0U;
    rds_capture_write_total_ms = 0U;
    rds_capture_write_max_ms = 0U;
    rds_capture_write_max_bytes = 0U;
    rds_capture_write_total_bytes = 0U;
    rds_capture_min = 0U;
    rds_capture_max = 0U;
    rds_capture_clip_4095 = 0U;
    rds_capture_sum = 0U;
    rds_capture_stats_valid = false;
    memset(&rds_capture_acq_start_stats, 0, sizeof(rds_capture_acq_start_stats));
    rds_capture_start_tick = 0U;
    rds_capture_stop_tick = 0U;
    rds_capture_pending_peak_blocks = 0U;
    rds_capture_file_path[0] = '\0';
    rds_capture_meta_file_path[0] = '\0';
    rds_capture_file_index = 0U;
}

static void fred_fm_rds_capture_reset_state(void) {
    fred_fm_rds_capture_release_ring();
    fred_fm_rds_capture_close_file();
    fred_fm_rds_capture_clear_state_fields();
}

static void fred_fm_rds_capture_update_acq_observed_stats(void) {
    RdsAcquisitionStats stats;
    rds_acquisition_get_stats(&rds_acquisition, &stats);
    if(stats.pending_blocks > rds_capture_pending_peak_blocks) {
        rds_capture_pending_peak_blocks = stats.pending_blocks;
    }
}

static bool fred_fm_rds_capture_ring_write_block(const uint16_t* samples, size_t count) {
    if(!rds_capture_ring || count != RDS_ACQ_BLOCK_SAMPLES) return false;

    uint32_t offset_bytes = 0U;
    {
        FURI_CRITICAL_ENTER();
        if(rds_capture_ring_count_blocks >= rds_capture_ring_capacity_blocks) {
            FURI_CRITICAL_EXIT();
            rds_capture_ring_overflow_blocks++;
            return false;
        }
        offset_bytes =
            rds_capture_ring_head_block * RDS_ACQ_BLOCK_SAMPLES * RDS_CAPTURE_SAMPLE_BYTES;
        FURI_CRITICAL_EXIT();
    }

    memcpy((uint8_t*)rds_capture_ring + offset_bytes, samples, count * sizeof(uint16_t));

    {
        FURI_CRITICAL_ENTER();
        rds_capture_ring_head_block++;
        if(rds_capture_ring_head_block >= rds_capture_ring_capacity_blocks) {
            rds_capture_ring_head_block = 0U;
        }

        rds_capture_ring_count_blocks++;
        if(rds_capture_ring_count_blocks > rds_capture_ring_peak_blocks) {
            rds_capture_ring_peak_blocks = rds_capture_ring_count_blocks;
        }
        FURI_CRITICAL_EXIT();
    }

    return true;
}

static bool fred_fm_rds_capture_ring_write_block_realtime(const uint16_t* samples, size_t count) {
    if(!rds_capture_ring || count != RDS_ACQ_BLOCK_SAMPLES) return false;
    if(rds_capture_ring_count_blocks >= rds_capture_ring_capacity_blocks) {
        rds_capture_ring_overflow_blocks++;
        return false;
    }

    uint32_t offset_bytes =
        rds_capture_ring_head_block * RDS_ACQ_BLOCK_SAMPLES * RDS_CAPTURE_SAMPLE_BYTES;
    memcpy((uint8_t*)rds_capture_ring + offset_bytes, samples, count * sizeof(uint16_t));

    rds_capture_ring_head_block++;
    if(rds_capture_ring_head_block >= rds_capture_ring_capacity_blocks) {
        rds_capture_ring_head_block = 0U;
    }

    rds_capture_ring_count_blocks++;
    if(rds_capture_ring_count_blocks > rds_capture_ring_peak_blocks) {
        rds_capture_ring_peak_blocks = rds_capture_ring_count_blocks;
    }

    return true;
}

static bool fred_fm_rds_capture_ring_peek_blocks(uint32_t* offset_bytes, uint32_t* blocks) {
    if(!offset_bytes || !blocks) return false;

    bool has_blocks = false;
    FURI_CRITICAL_ENTER();
    if(rds_capture_ring && (rds_capture_ring_count_blocks > 0U)) {
        uint32_t contiguous_blocks =
            rds_capture_ring_capacity_blocks - rds_capture_ring_tail_block;
        if(contiguous_blocks > rds_capture_ring_count_blocks) {
            contiguous_blocks = rds_capture_ring_count_blocks;
        }

        *offset_bytes =
            rds_capture_ring_tail_block * RDS_ACQ_BLOCK_SAMPLES * RDS_CAPTURE_SAMPLE_BYTES;
        *blocks = contiguous_blocks;
        has_blocks = true;
    }
    FURI_CRITICAL_EXIT();

    return has_blocks;
}

static void fred_fm_rds_capture_ring_consume_blocks(uint32_t blocks) {
    if(blocks == 0U) return;

    FURI_CRITICAL_ENTER();
    if(blocks > rds_capture_ring_count_blocks) {
        blocks = rds_capture_ring_count_blocks;
    }
    if(blocks > 0U) {
        rds_capture_ring_tail_block += blocks;
        if(rds_capture_ring_tail_block >= rds_capture_ring_capacity_blocks) {
            rds_capture_ring_tail_block %= rds_capture_ring_capacity_blocks;
        }
        rds_capture_ring_count_blocks -= blocks;
        rds_capture_written_blocks += blocks;
    }
    FURI_CRITICAL_EXIT();
}

static void fred_fm_rds_capture_update_stats_block(const uint16_t* samples, size_t count) {
    if(!samples || count == 0U) return;

    for(size_t i = 0; i < count; i++) {
        uint16_t sample = samples[i];

        if(!rds_capture_stats_valid) {
            rds_capture_min = sample;
            rds_capture_max = sample;
            rds_capture_stats_valid = true;
        } else {
            if(sample < rds_capture_min) rds_capture_min = sample;
            if(sample > rds_capture_max) rds_capture_max = sample;
        }

        if(sample >= 4095U) {
            rds_capture_clip_4095++;
        }

        rds_capture_sum += sample;
    }

    rds_capture_capture_samples += (uint32_t)count;
}

static void fred_fm_rds_capture_mark_done(void) {
    rds_capture_active = false;
    rds_capture_finalize_pending = true;
    rds_capture_stop_tick = furi_get_tick();
    fred_fm_rds_capture_signal_writer(RDS_CAPTURE_WRITER_FLAG_WORK);
}

static void fred_fm_rds_capture_mark_done_deferred(void) {
    rds_capture_active = false;
    rds_capture_finalize_pending = true;
}

static void fred_fm_rds_capture_start(void) {
    if(rds_capture_active || rds_capture_finalize_pending) return;
    if(!rds_capture_writer_thread) {
        FURI_LOG_W(TAG, "ADC capture: writer thread unavailable");
        return;
    }

    fred_fm_rds_capture_reset_state();

    uint32_t initial_blocks = fred_fm_rds_capture_select_ring_blocks();
    for(uint32_t blocks = initial_blocks; blocks >= 2U;) {
        rds_capture_ring = malloc(blocks * RDS_ACQ_BLOCK_SAMPLES * RDS_CAPTURE_SAMPLE_BYTES);
        if(rds_capture_ring) {
            rds_capture_ring_capacity_blocks = blocks;
            break;
        }

        if(blocks > 64U) {
            blocks -= 8U;
        } else if(blocks > 32U) {
            blocks -= 4U;
        } else if(blocks > 16U) {
            blocks -= 2U;
        } else if(blocks > RDS_CAPTURE_RING_MIN_BLOCKS) {
            blocks -= 1U;
        } else if(blocks == RDS_CAPTURE_RING_MIN_BLOCKS) {
            blocks = 4U;
        } else if(blocks == 4U) {
            blocks = 2U;
        } else {
            break;
        }
    }
    if(!rds_capture_ring) {
        FURI_LOG_W(TAG, "ADC capture: ring malloc failed");
        fred_fm_rds_capture_reset_state();
        return;
    }

    rds_capture_storage = furi_record_open(RECORD_STORAGE);
    if(!rds_capture_storage) {
        FURI_LOG_W(TAG, "ADC capture: storage unavailable");
        fred_fm_rds_capture_reset_state();
        return;
    }

    fred_fm_ensure_app_data_dir(rds_capture_storage);
    if(!fred_fm_rds_capture_prepare_paths(rds_capture_storage)) {
        FURI_LOG_W(TAG, "ADC capture: file path allocation failed");
        fred_fm_rds_capture_reset_state();
        return;
    }
    rds_capture_file = storage_file_alloc(rds_capture_storage);
    if(!rds_capture_file ||
       !storage_file_open(
           rds_capture_file, rds_capture_file_path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FURI_LOG_W(TAG, "ADC capture: raw open failed");
        fred_fm_rds_capture_reset_state();
        return;
    }
    if(!fred_fm_rds_capture_preallocate_file()) {
        FURI_LOG_W(TAG, "ADC capture: raw prealloc failed");
        fred_fm_rds_capture_reset_state();
        return;
    }

    rds_acquisition_get_stats(&rds_acquisition, &rds_capture_acq_start_stats);
    rds_capture_start_tick = furi_get_tick();
    rds_capture_stop_tick = rds_capture_start_tick;
    fred_fm_rds_capture_update_acq_observed_stats();

    rds_capture_active = true;
    rds_capture_abort_pending = false;
    FURI_LOG_I(
        TAG,
        "ADC capture started #%lu (%lu blocks ring, %lu KiB free heap, %lu blocks target)",
        (unsigned long)rds_capture_file_index,
        (unsigned long)rds_capture_ring_capacity_blocks,
        (unsigned long)(rds_capture_free_heap_bytes / 1024U),
        (unsigned long)rds_capture_target_blocks);
}

/* Called from cleanup paths — free resources without SD write */
static void fred_fm_rds_capture_stop(void) {
    rds_capture_requested = false;

    if(rds_capture_active || rds_capture_finalize_pending || rds_capture_file ||
       rds_capture_ring) {
        rds_capture_active = false;
        rds_capture_finalize_pending = false;
        rds_capture_abort_pending = true;
        fred_fm_rds_capture_signal_writer(RDS_CAPTURE_WRITER_FLAG_WORK);
        return;
    }

    fred_fm_rds_capture_reset_state();
}

/* Called from block callback — MUST be fast (memcpy only) */
static void fred_fm_rds_capture_write_block(const uint16_t* samples, size_t count) {
    if(!rds_capture_active || !rds_capture_ring) return;
    if(count != RDS_ACQ_BLOCK_SAMPLES) return;

    if(rds_capture_captured_blocks >= rds_capture_target_blocks) {
        fred_fm_rds_capture_mark_done();
        return;
    }

    if(!fred_fm_rds_capture_ring_write_block(samples, count)) {
        /* Capture ring full — writer is draining to SD; drop this deferred block. */
        return;
    }

    rds_capture_captured_blocks++;
    fred_fm_rds_capture_signal_writer(RDS_CAPTURE_WRITER_FLAG_WORK);

    if(rds_capture_captured_blocks >= rds_capture_target_blocks) {
        fred_fm_rds_capture_mark_done();
    }
}

static bool fred_fm_rds_acquisition_realtime_block_callback(
    const uint16_t* samples,
    size_t count,
    uint16_t adc_midpoint,
    void* context) {
    UNUSED(adc_midpoint);
    UNUSED(context);

    if(fred_fm_app_exiting || rds_pipeline_stopping)
        return false; // Skip during exit / pipeline stop

    if(!rds_capture_active || !rds_capture_ring) return false;
    if(count != RDS_ACQ_BLOCK_SAMPLES) return false;

    if(rds_capture_captured_blocks >= rds_capture_target_blocks) {
        fred_fm_rds_capture_mark_done_deferred();
        return true;
    }

    if(!fred_fm_rds_capture_ring_write_block_realtime(samples, count)) {
        /* Ring full: backpressure through acquisition pending queue. */
        return false;
    }

    rds_capture_captured_blocks++;
    fred_fm_rds_capture_signal_writer(RDS_CAPTURE_WRITER_FLAG_WORK);
    if(rds_capture_captured_blocks >= rds_capture_target_blocks) {
        fred_fm_rds_capture_mark_done_deferred();
        fred_fm_rds_capture_signal_writer(RDS_CAPTURE_WRITER_FLAG_WORK);
    }

    return true;
}

static void fred_fm_rds_capture_write_meta(void) {
    if(!rds_capture_storage) return;
    if(rds_capture_meta_file_path[0] == '\0') return;

    File* meta = storage_file_alloc(rds_capture_storage);
    if(meta &&
       storage_file_open(meta, rds_capture_meta_file_path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        char line[128];
        const uint32_t ticks_per_second = furi_ms_to_ticks(1000U);
        RdsAcquisitionStats stats;
        rds_acquisition_get_stats(&rds_acquisition, &stats);
        uint32_t capture_elapsed_ticks = rds_capture_stop_tick - rds_capture_start_tick;
        uint32_t capture_elapsed_ms = 0U;
        uint32_t measured_sample_rate_hz = 0U;
        uint32_t dma_measured_sample_rate_hz = 0U;
        uint32_t write_avg_bytes = 0U;
        uint32_t write_effective_rate_bps = 0U;
        uint32_t dma_half_events =
            stats.dma_half_events - rds_capture_acq_start_stats.dma_half_events;
        uint32_t dma_full_events =
            stats.dma_full_events - rds_capture_acq_start_stats.dma_full_events;
        uint32_t total_dma_blocks =
            stats.total_dma_blocks - rds_capture_acq_start_stats.total_dma_blocks;
        uint32_t delivered_blocks =
            stats.delivered_blocks - rds_capture_acq_start_stats.delivered_blocks;
        uint32_t dropped_blocks =
            stats.dropped_blocks - rds_capture_acq_start_stats.dropped_blocks;
        uint32_t adc_overrun_count =
            stats.adc_overrun_count - rds_capture_acq_start_stats.adc_overrun_count;
        uint32_t capture_mean_x100 =
            (rds_capture_capture_samples > 0U) ?
                (uint32_t)((rds_capture_sum * 100ULL) / rds_capture_capture_samples) :
                0U;
        int n;

        if((capture_elapsed_ticks > 0U) && (ticks_per_second > 0U)) {
            capture_elapsed_ms =
                (uint32_t)(((uint64_t)capture_elapsed_ticks * 1000ULL) / ticks_per_second);
            measured_sample_rate_hz =
                (uint32_t)(((uint64_t)rds_capture_capture_samples * (uint64_t)ticks_per_second) /
                           (uint64_t)capture_elapsed_ticks);
            dma_measured_sample_rate_hz =
                (uint32_t)(((uint64_t)total_dma_blocks * (uint64_t)RDS_ACQ_BLOCK_SAMPLES *
                            (uint64_t)ticks_per_second) /
                           (uint64_t)capture_elapsed_ticks);
        }
        if(rds_capture_write_call_count > 0U) {
            write_avg_bytes = rds_capture_write_total_bytes / rds_capture_write_call_count;
        }
        if(rds_capture_write_total_ms > 0U) {
            write_effective_rate_bps =
                (uint32_t)(((uint64_t)rds_capture_write_total_bytes * 1000ULL) /
                           (uint64_t)rds_capture_write_total_ms);
        }

#define CAP_META(fmt, ...)                                \
    n = snprintf(line, sizeof(line), fmt, ##__VA_ARGS__); \
    if(n > 0) storage_file_write(meta, line, (size_t)n)

        CAP_META("capture_index=%lu\n", (unsigned long)rds_capture_file_index);
        CAP_META("capture_file=%s\n", rds_capture_file_path);
        CAP_META("capture_meta_file=%s\n", rds_capture_meta_file_path);
        CAP_META("capture_samples=%lu\n", (unsigned long)rds_capture_capture_samples);
        CAP_META("capture_target_samples=%lu\n", (unsigned long)RDS_CAPTURE_TARGET_SAMPLES);
        CAP_META("capture_target_blocks=%lu\n", (unsigned long)rds_capture_target_blocks);
        CAP_META("capture_blocks=%lu\n", (unsigned long)rds_capture_captured_blocks);
        CAP_META("written_blocks=%lu\n", (unsigned long)rds_capture_written_blocks);
        CAP_META("capture_elapsed_ms=%lu\n", (unsigned long)capture_elapsed_ms);
        CAP_META(
            "configured_sample_rate_hz=%lu\n", (unsigned long)stats.configured_sample_rate_hz);
        CAP_META("measured_sample_rate_hz=%lu\n", (unsigned long)measured_sample_rate_hz);
        CAP_META("raw_effective_sample_rate_hz=%lu\n", (unsigned long)measured_sample_rate_hz);
        CAP_META("sample_format=%s\n", RDS_CAPTURE_SAMPLE_FORMAT);
        CAP_META("sample_bits=%u\n", (unsigned)RDS_CAPTURE_SAMPLE_BITS);
        CAP_META("sample_bytes=%u\n", (unsigned)RDS_CAPTURE_SAMPLE_BYTES);
        CAP_META("dma_measured_sample_rate_hz=%lu\n", (unsigned long)dma_measured_sample_rate_hz);
        CAP_META("adc_midpoint=%u\n", (unsigned)stats.adc_midpoint);
        CAP_META("adc_min=%u\n", (unsigned)rds_capture_min);
        CAP_META("adc_max=%u\n", (unsigned)rds_capture_max);
        CAP_META(
            "adc_mean=%lu.%02lu\n",
            (unsigned long)(capture_mean_x100 / 100U),
            (unsigned long)(capture_mean_x100 % 100U));
        CAP_META("adc_clip_4095=%lu\n", (unsigned long)rds_capture_clip_4095);
        CAP_META("tuned_freq_10khz=%lu\n", (unsigned long)fred_fm_get_current_freq_10khz());
        CAP_META(
            "manual_carrier_offset_centihz=%ld\n", (long)fred_fm_rds_get_manual_offset_centihz());
        CAP_META("ring_capacity_blocks=%lu\n", (unsigned long)rds_capture_ring_capacity_blocks);
        CAP_META(
            "ring_capacity_bytes=%lu\n",
            (unsigned long)(rds_capture_ring_capacity_blocks * RDS_ACQ_BLOCK_SAMPLES *
                            RDS_CAPTURE_SAMPLE_BYTES));
        CAP_META("free_heap_bytes=%lu\n", (unsigned long)rds_capture_free_heap_bytes);
        CAP_META("ring_peak_blocks=%lu\n", (unsigned long)rds_capture_ring_peak_blocks);
        CAP_META("ring_overflow_blocks=%lu\n", (unsigned long)rds_capture_ring_overflow_blocks);
        CAP_META("write_call_count=%lu\n", (unsigned long)rds_capture_write_call_count);
        CAP_META("write_total_ms=%lu\n", (unsigned long)rds_capture_write_total_ms);
        CAP_META("write_max_ms=%lu\n", (unsigned long)rds_capture_write_max_ms);
        CAP_META("write_total_bytes=%lu\n", (unsigned long)rds_capture_write_total_bytes);
        CAP_META("write_max_bytes=%lu\n", (unsigned long)rds_capture_write_max_bytes);
        CAP_META("write_avg_bytes=%lu\n", (unsigned long)write_avg_bytes);
        CAP_META("write_effective_rate_bps=%lu\n", (unsigned long)write_effective_rate_bps);
        CAP_META("storage_write_errors=%lu\n", (unsigned long)rds_capture_storage_write_errors);
        CAP_META("capture_complete=%u\n", rds_capture_complete ? 1U : 0U);
        CAP_META("capture_error=%u\n", rds_capture_error ? 1U : 0U);
        CAP_META("dma_buffer_samples=%u\n", (unsigned)stats.dma_buffer_samples);
        CAP_META("dma_block_samples=%u\n", (unsigned)stats.block_samples);
        CAP_META("dma_half_events=%lu\n", (unsigned long)dma_half_events);
        CAP_META("dma_full_events=%lu\n", (unsigned long)dma_full_events);
        CAP_META("total_dma_blocks=%lu\n", (unsigned long)total_dma_blocks);
        CAP_META("delivered_blocks=%lu\n", (unsigned long)delivered_blocks);
        CAP_META("pending_blocks=%u\n", (unsigned)stats.pending_blocks);
        CAP_META("pending_peak_blocks=%u\n", (unsigned)rds_capture_pending_peak_blocks);
        CAP_META("dropped_blocks=%lu\n", (unsigned long)dropped_blocks);
        CAP_META("adc_overrun_count=%lu\n", (unsigned long)adc_overrun_count);
#undef CAP_META
        storage_file_close(meta);
    }
    if(meta) storage_file_free(meta);
}

static void fred_fm_rds_capture_finish(void) {
    const bool target_reached = (rds_capture_captured_blocks >= rds_capture_target_blocks) &&
                                (rds_capture_written_blocks >= rds_capture_target_blocks);

    rds_capture_complete = !rds_capture_error && target_reached;

    if(!fred_fm_rds_capture_trim_file_to_written_size()) {
        rds_capture_storage_write_errors++;
        rds_capture_error = true;
        rds_capture_complete = false;
    }

    fred_fm_rds_capture_write_meta();
    fred_fm_rds_capture_close_file();
    fred_fm_rds_capture_release_ring();
    rds_capture_finalize_pending = false;
    rds_capture_abort_pending = false;

    FURI_LOG_I(
        TAG,
        "ADC capture finished (%lu/%lu blocks, err=%u)",
        (unsigned long)rds_capture_written_blocks,
        (unsigned long)rds_capture_target_blocks,
        rds_capture_error ? 1U : 0U);

    /* Double-stop guard: if the main thread is concurrently tearing down the
     * RDS pipeline (rds_pipeline_stopping=true on config OFF or app exit) or
     * already in app-exit, we must NOT also call fred_fm_rds_adc_stop() from
     * the writer thread. Otherwise two threads race inside
     * rds_acquisition_stop() and double-release the ADC handle (HardFault).
     * The main thread will perform the stop synchronously. */
    if(!fred_fm_rds_pipeline_enabled() && !rds_pipeline_stopping && !fred_fm_app_exiting) {
        fred_fm_rds_timer_stop();
        fred_fm_rds_adc_stop();
    }
}

static void fred_fm_rds_capture_abort_cleanup(void) {
    fred_fm_rds_capture_close_file();
    fred_fm_rds_capture_release_ring();
    fred_fm_rds_capture_clear_state_fields();
}

static int32_t fred_fm_rds_capture_writer_thread_callback(void* context) {
    UNUSED(context);

    while(true) {
        uint32_t flags = furi_thread_flags_wait(
            RDS_CAPTURE_WRITER_FLAG_WORK | RDS_CAPTURE_WRITER_FLAG_STOP,
            FuriFlagWaitAny,
            FuriWaitForever);
        if(flags & FuriFlagError) {
            continue;
        }

        if(flags & RDS_CAPTURE_WRITER_FLAG_STOP) {
            break;
        }

        while(true) {
            if(fred_fm_app_exiting) {
                /* App is exiting: stop draining the ring even if blocks are
                 * pending. The outer loop will then observe STOP flag and
                 * break out of the writer thread cleanly. */
                break;
            }
            if(rds_capture_abort_pending) {
                fred_fm_rds_capture_abort_cleanup();
                break;
            }

            uint32_t offset_bytes = 0U;
            uint32_t blocks = 0U;
            if(fred_fm_rds_capture_ring_peek_blocks(&offset_bytes, &blocks)) {
                if(!rds_capture_file || !rds_capture_ring) {
                    rds_capture_error = true;
                    fred_fm_rds_capture_abort_cleanup();
                    break;
                }

                size_t sample_count = (size_t)blocks * RDS_ACQ_BLOCK_SAMPLES;
                size_t bytes = sample_count * RDS_CAPTURE_SAMPLE_BYTES;
                uint32_t write_start_tick = furi_get_tick();
                size_t written = storage_file_write(
                    rds_capture_file, (const uint8_t*)rds_capture_ring + offset_bytes, bytes);
                uint32_t write_elapsed_ms = furi_get_tick() - write_start_tick;
                if(written != bytes) {
                    rds_capture_storage_write_errors++;
                    rds_capture_error = true;
                    rds_capture_complete = false;
                    fred_fm_rds_capture_abort_cleanup();
                    break;
                }

                rds_capture_write_call_count++;
                rds_capture_write_total_ms += write_elapsed_ms;
                rds_capture_write_total_bytes += (uint32_t)bytes;
                if(write_elapsed_ms > rds_capture_write_max_ms) {
                    rds_capture_write_max_ms = write_elapsed_ms;
                }
                if(bytes > rds_capture_write_max_bytes) {
                    rds_capture_write_max_bytes = (uint32_t)bytes;
                }

                fred_fm_rds_capture_update_stats_block(
                    (const uint16_t*)((const uint8_t*)rds_capture_ring + offset_bytes),
                    sample_count);
                fred_fm_rds_capture_ring_consume_blocks(blocks);
                continue;
            }

            if(rds_capture_finalize_pending) {
                fred_fm_rds_capture_finish();
                break;
            }

            break;
        }
    }

    if(rds_capture_file || rds_capture_ring || rds_capture_abort_pending || rds_capture_active ||
       rds_capture_finalize_pending) {
        fred_fm_rds_capture_abort_cleanup();
    }

    return 0;
}

static bool fred_fm_rds_capture_writer_start(void) {
    if(rds_capture_writer_thread) return true;

    rds_capture_writer_thread = furi_thread_alloc_ex(
        "RdsCaptureWrite",
        RDS_CAPTURE_WRITER_STACK_SIZE,
        fred_fm_rds_capture_writer_thread_callback,
        NULL);
    if(!rds_capture_writer_thread) {
        FURI_LOG_W(TAG, "ADC capture: writer thread alloc failed");
        return false;
    }

    furi_thread_set_priority(rds_capture_writer_thread, RDS_CAPTURE_WRITER_PRIORITY);
    furi_thread_start(rds_capture_writer_thread);
    return true;
}

static void fred_fm_rds_capture_writer_stop(void) {
    if(!rds_capture_writer_thread) return;

    fred_fm_rds_capture_signal_writer(RDS_CAPTURE_WRITER_FLAG_WORK | RDS_CAPTURE_WRITER_FLAG_STOP);
    furi_thread_join(rds_capture_writer_thread);
    furi_thread_free(rds_capture_writer_thread);
    rds_capture_writer_thread = NULL;
}

/* Called from timer callback — keep it cheap and never touch SD I/O here */
static void fred_fm_rds_capture_flush_to_sd(void) {
    if(rds_capture_active || rds_capture_finalize_pending) {
        fred_fm_rds_capture_update_acq_observed_stats();
    }

    if(rds_capture_finalize_pending && !rds_capture_active &&
       (rds_capture_stop_tick == rds_capture_start_tick)) {
        rds_capture_stop_tick = furi_get_tick();
    }

    if(rds_capture_ring_count_blocks > 0U || rds_capture_finalize_pending ||
       rds_capture_abort_pending) {
        fred_fm_rds_capture_signal_writer(RDS_CAPTURE_WRITER_FLAG_WORK);
    }
}

#else /* !ENABLE_ADC_CAPTURE */
static inline void fred_fm_rds_capture_stop(void) {
}
static inline void fred_fm_rds_capture_flush_to_sd(void) {
}
#endif /* ENABLE_ADC_CAPTURE */

static void fred_fm_rds_runtime_reset(void) {
    rds_acquisition_reset(&rds_acquisition);
#if RDS_RUNTIME_META_ENABLED
    rds_dsp_block_count = 0U;
    rds_dsp_block_total_ms = 0U;
    rds_dsp_block_max_ms = 0U;
    rds_dsp_last_block_ms = 0U;
#endif
}

/*
 * NOTE: fred_fm_rds_pipeline_start is called only during init or TEA startup.
 * It does NOT execute on the hot path (ISR → decoder).
 * The hot path remains: ADC ISR → fred_fm_rds_acquisition_block_callback →
 * rds_dsp_process_block → fred_fm_rds_symbol_callback, staying under 8.192ms.
 */
static void fred_fm_rds_pipeline_start(void) {
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

static bool fred_fm_rds_pipeline_enabled(void) {
    return rds_enabled || rds_debug_enabled;
}

static void fred_fm_rds_timer_start(void) {
    if(rds_adc_timer_handle && !rds_adc_timer_running) {
        furi_timer_start(rds_adc_timer_handle, furi_ms_to_ticks(RDS_ACQ_TIMER_MS));
        rds_adc_timer_running = true;
    }
}

static void fred_fm_rds_timer_stop(void) {
    if(rds_adc_timer_handle && rds_adc_timer_running) {
        furi_timer_stop(rds_adc_timer_handle);
        rds_adc_timer_running = false;
    }
}

static void fred_fm_rds_apply_runtime_state(bool reset_decoder) {
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
            fred_fm_rds_pipeline_start(); // includes runtime reset, decoder init, acquisition init, writer start
        }
        fred_fm_rds_timer_start();
    } else {
        /* Stop pipeline barrier sequence — order matters:
         *   1. Set rds_pipeline_stopping=true so worker DSP, DMA ISR realtime
         *      callback and deferred block callback short-circuit.
         *   2. Stop the 2 ms timer so no new TICK wake-ups arrive at the
         *      worker.
         *   3. Drain delay: 20 ms is comfortably longer than one DMA block
         *      period (8.192 ms) plus DSP processing of a 1024-sample block,
         *      so any worker iteration that started before step 1 finishes
         *      cleanly before we tear down the producer.
         *   4. Save meta (no-op when RDS_RUNTIME_META_ENABLED=0).
         *   5. capture_stop() signals the writer thread to abort_cleanup
         *      (which frees rds_capture_ring) — safe now, no producer.
         *   6. adc_stop() unregisters DMA1 CH1 ISR, stops trigger timer +
         *      ADC + DMA, releases ADC handle.
         *   7. Clear barrier so a later ON transition can restart.
         */
        rds_pipeline_stopping = true;
        fred_fm_rds_timer_stop();
        furi_delay_ms(20);
        fred_fm_rds_runtime_meta_save();
        fred_fm_rds_capture_stop();
        fred_fm_rds_adc_stop();
        rds_pipeline_stopping = false;
    }
}

static void fred_fm_rds_runtime_meta_save(void) {
#if !RDS_RUNTIME_META_ENABLED
    return;
#else
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return;

    File* meta_file = storage_file_alloc(storage);
    if(!meta_file) {
        furi_record_close(RECORD_STORAGE);
        return;
    }

    RdsAcquisitionStats stats;
    rds_acquisition_get_stats(&rds_acquisition, &stats);
    uint32_t drop_rate_pct_x100 = 0U;
    if(stats.total_dma_blocks > 0U) {
        drop_rate_pct_x100 =
            (uint32_t)(((uint64_t)stats.dropped_blocks * 10000ULL) / stats.total_dma_blocks);
    }

    fred_fm_ensure_app_data_dir(storage);
    if(!storage_file_open(meta_file, RDS_RUNTIME_META_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_free(meta_file);
        furi_record_close(RECORD_STORAGE);
        return;
    }

    /* Write meta in small chunks to avoid stack overflow (4 KB app stack).
       Each snprintf+write uses only 128 bytes of stack buffer. */
    char line[128];
    int n;

#define META_WRITE(fmt, ...)                                      \
    do {                                                          \
        n = snprintf(line, sizeof(line), fmt, __VA_ARGS__);       \
        if(n > 0) storage_file_write(meta_file, line, (size_t)n); \
    } while(0)

    META_WRITE("configured_sample_rate_hz=%lu\n", (unsigned long)stats.configured_sample_rate_hz);
    META_WRITE("measured_sample_rate_hz=%lu\n", (unsigned long)stats.measured_sample_rate_hz);
    META_WRITE("adc_midpoint=%u\n", (unsigned)stats.adc_midpoint);
    META_WRITE("dma_block_samples=%u\n", (unsigned)stats.block_samples);
    META_WRITE("total_dma_blocks=%lu\n", (unsigned long)stats.total_dma_blocks);
    META_WRITE("delivered_blocks=%lu\n", (unsigned long)stats.delivered_blocks);
    META_WRITE("dropped_blocks=%lu\n", (unsigned long)stats.dropped_blocks);
    META_WRITE(
        "drop_rate_pct=%lu.%02lu\n",
        (unsigned long)(drop_rate_pct_x100 / 100U),
        (unsigned long)(drop_rate_pct_x100 % 100U));
    META_WRITE("pending_blocks=%u\n", (unsigned)stats.pending_blocks);
    META_WRITE("pending_peak_blocks=%u\n", (unsigned)stats.pending_peak_blocks);
    META_WRITE("ring_capacity_blocks=%u\n", (unsigned)stats.ring_capacity_blocks);
    META_WRITE("ring_overrun_count=%lu\n", (unsigned long)stats.ring_overrun_count);
    META_WRITE("rds_dsp_block_count=%lu\n", (unsigned long)rds_dsp_block_count);
    META_WRITE("rds_dsp_block_total_ms=%lu\n", (unsigned long)rds_dsp_block_total_ms);
    META_WRITE("rds_dsp_block_max_ms=%lu\n", (unsigned long)rds_dsp_block_max_ms);
    META_WRITE("rds_dsp_last_block_ms=%lu\n", (unsigned long)rds_dsp_last_block_ms);
    META_WRITE("adc_overrun_count=%lu\n", (unsigned long)stats.adc_overrun_count);
    META_WRITE(
        "dsp_symbol_confidence_avg_q16=%lu\n", (unsigned long)rds_dsp.symbol_confidence_avg_q16);
    META_WRITE(
        "dsp_block_confidence_avg_q16=%lu\n", (unsigned long)rds_dsp.block_confidence_avg_q16);
    META_WRITE("dsp_pilot_level_q8=%lu\n", (unsigned long)rds_dsp.pilot_level_q8);
    META_WRITE("dsp_rds_band_level_q8=%lu\n", (unsigned long)rds_dsp.rds_band_level_q8);
    META_WRITE("dsp_avg_abs_hp_q8=%lu\n", (unsigned long)rds_dsp.avg_abs_hp_q8);
    META_WRITE("dsp_avg_vector_mag_q8=%lu\n", (unsigned long)rds_dsp.avg_vector_mag_q8);
    META_WRITE("dsp_avg_decision_mag_q8=%lu\n", (unsigned long)rds_dsp.avg_decision_mag_q8);
    META_WRITE("sync_state=%lu\n", (unsigned long)rds_sync_display);
    META_WRITE("valid_blocks=%lu\n", (unsigned long)rds_core.valid_blocks);
    META_WRITE("corrected_blocks=%lu\n", (unsigned long)rds_core.corrected_blocks);
    META_WRITE("uncorrectable_blocks=%lu\n", (unsigned long)rds_core.uncorrectable_blocks);
    META_WRITE("sync_losses=%lu\n", (unsigned long)rds_core.sync_losses);
    META_WRITE("bit_slip_repairs=%lu\n", (unsigned long)rds_core.bit_slip_repairs);
    META_WRITE("tuned_freq_10khz=%lu\n", (unsigned long)fred_fm_get_current_freq_10khz());
    META_WRITE(
        "manual_carrier_offset_centihz=%ld\n", (long)fred_fm_rds_get_manual_offset_centihz());
    META_WRITE("dsp_dc_estimate_q8=%ld\n", (long)rds_dsp.dc_estimate_q8);
    META_WRITE("core_groups_complete=%lu\n", (unsigned long)rds_core.groups_complete);
    META_WRITE("core_groups_type0=%lu\n", (unsigned long)rds_core.groups_type0);
    META_WRITE("core_groups_type2=%lu\n", (unsigned long)rds_core.groups_type2);
    META_WRITE("core_groups_other=%lu\n", (unsigned long)rds_core.groups_other);
    META_WRITE("core_ps_updates=%lu\n", (unsigned long)rds_core.ps_updates);
    META_WRITE("core_last_pi=0x%04X\n", (unsigned)rds_core.last_pi);
    META_WRITE("core_ps_segment_mask=0x%02X\n", (unsigned)rds_core.ps_segment_mask);
    META_WRITE("core_ps_candidate=%.8s\n", rds_core.program.ps_candidate);
    META_WRITE("core_ps_ready=%u\n", (unsigned)rds_core.program.ps_ready);
    META_WRITE("core_presync_attempts=%lu\n", (unsigned long)rds_core.presync_attempts);
    META_WRITE(
        "core_presync_max_consecutive=%lu\n", (unsigned long)rds_core.presync_max_consecutive);
    META_WRITE("core_presync_consecutive_now=%u\n", (unsigned)rds_core.presync_consecutive);
    META_WRITE(
        "core_quality_gate_pilot_fail=%lu\n", (unsigned long)rds_core.quality_gate_pilot_fail);
    META_WRITE("core_quality_gate_rds_fail=%lu\n", (unsigned long)rds_core.quality_gate_rds_fail);
    META_WRITE("core_pilot_detected=%u\n", (unsigned)rds_core.pilot_detected);
    META_WRITE("core_rds_carrier_detected=%u\n", (unsigned)rds_core.rds_carrier_detected);

#undef META_WRITE

    storage_file_close(meta_file);
    storage_file_free(meta_file);
    furi_record_close(RECORD_STORAGE);
#endif
}

static void fred_fm_rds_clear_station_name(void) {
    fred_fm_state_lock();
    memset(rds_ps_display, 0, sizeof(rds_ps_display));
    rds_sync_display = RdsSyncStateSearch;
    fred_fm_state_unlock();
}

static void fred_fm_rds_constellation_clear_history_locked(void) {
    rds_constellation_history_count = 0U;
    rds_constellation_history_write_index = 0U;
}

static void fred_fm_rds_constellation_clear_history(void) {
    FURI_CRITICAL_ENTER();
    fred_fm_rds_constellation_clear_history_locked();
    FURI_CRITICAL_EXIT();
}

static void fred_fm_rds_symbol_callback(
    void* context,
    int32_t symbol_i,
    int32_t symbol_q,
    uint32_t confidence_q16) {
    UNUSED(context);
    UNUSED(confidence_q16);

    /* Record constellation history only when debug view is active.
     * Guarded by rds_debug_enabled to avoid overhead in normal operation. */
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

static int16_t fred_fm_rds_get_manual_offset_centihz(void) {
    int16_t offset_centihz;
    fred_fm_state_lock();
    offset_centihz = rds_carrier_offset_centihz;
    fred_fm_state_unlock();
    return offset_centihz;
}

static void fred_fm_rds_set_manual_offset_centihz(int16_t offset_centihz) {
    offset_centihz = fred_fm_clamp_manual_offset_centihz(offset_centihz);

    fred_fm_state_lock();
    rds_carrier_offset_centihz = offset_centihz;
    rds_dsp_set_manual_carrier_offset_centihz(&rds_dsp, offset_centihz);
    fred_fm_state_unlock();
}

static void fred_fm_rds_sync_offset_from_current_frequency(void) {
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

static void fred_fm_rds_update_ui_snapshot(void) {
    fred_fm_state_lock();
    rds_sync_display = rds_core.sync_state;
    fred_fm_state_unlock();
}

static uint32_t fred_fm_rds_select_runtime_sample_rate(const RdsAcquisitionStats* stats) {
    if(!stats) {
        return 0U;
    }

    if(stats->configured_sample_rate_hz != 0U) {
        return stats->configured_sample_rate_hz;
    }

    return RDS_DECODE_SAMPLE_RATE_HZ;
}

static void fred_fm_rds_refresh_runtime_sample_rate(bool force_reset) {
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

static const char* fred_fm_rds_sync_short_text(RdsSyncState state) {
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

static void fred_fm_rds_on_tuned_frequency_changed(void) {
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

static void fred_fm_rds_process_events(void) {
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

void fred_fm_rds_process_adc_block(const uint16_t* samples, size_t count, uint16_t adc_midpoint) {
    if(fred_fm_app_exiting || rds_pipeline_stopping) return;
    static uint8_t ui_snapshot_div = 0U;

#if ENABLE_ADC_CAPTURE
    /* Start capture on request (deferred from input callback to ISR-safe context) */
    if(rds_capture_requested && !rds_capture_active) {
        rds_capture_requested = false;
        fred_fm_rds_capture_start();
    }

    /* During capture: ONLY write raw samples, skip DSP to save CPU */
    if(rds_capture_active) {
        fred_fm_rds_capture_write_block(samples, count);
        return;
    }
#endif

    if(!fred_fm_rds_pipeline_enabled()) return;

    rds_core_set_tick_ms(&rds_core, furi_get_tick());
#if RDS_RUNTIME_META_ENABLED
    uint32_t dsp_start_tick = furi_get_tick();
#endif
    rds_dsp_process_u16_samples(&rds_dsp, &rds_core, samples, count, adc_midpoint);
#if RDS_RUNTIME_META_ENABLED
    uint32_t dsp_elapsed_ms = furi_get_tick() - dsp_start_tick;
    rds_dsp_block_count++;
    rds_dsp_block_total_ms += dsp_elapsed_ms;
    rds_dsp_last_block_ms = dsp_elapsed_ms;
    if(dsp_elapsed_ms > rds_dsp_block_max_ms) {
        rds_dsp_block_max_ms = dsp_elapsed_ms;
    }
#endif
    (void)ui_snapshot_div;
}

static void fred_fm_rds_acquisition_block_callback(
    const uint16_t* samples,
    size_t count,
    uint16_t adc_midpoint,
    void* context) {
    UNUSED(context);
    if(fred_fm_app_exiting || rds_pipeline_stopping) return;
    fred_fm_rds_process_adc_block(samples, count, adc_midpoint);
}

static void fred_fm_rds_adc_stop(void) {
    rds_acquisition_stop(&rds_acquisition);
}

static int32_t fred_fm_rds_dsp_worker(void* context) {
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

        /* Timer flags are level-triggered, so a delayed worker can coalesce
       multiple 2 ms ticks into one wake-up. Drain the backlog per wake-up
       so RDS processing stays independent of the active UI view. */
        if(fred_fm_app_exiting || rds_pipeline_stopping) {
            continue; // Skip processing when exiting / pipeline stopping
        }
        rds_acquisition_on_timer_tick(&rds_acquisition, true);
    }
    return 0;
}

static void fred_fm_rds_dsp_worker_start(void) {
    if(rds_dsp_worker_thread) return;
    rds_dsp_worker_thread = furi_thread_alloc_ex(
        "RdsDspWorker", RDS_DSP_WORKER_STACK_SIZE, fred_fm_rds_dsp_worker, NULL);
    if(!rds_dsp_worker_thread) return;
    furi_thread_set_priority(rds_dsp_worker_thread, FuriThreadPriorityLow);
    furi_thread_start(rds_dsp_worker_thread);
    rds_dsp_worker_thread_id = furi_thread_get_id(rds_dsp_worker_thread);
}

static void fred_fm_rds_adc_timer_callback(void* context) {
    UNUSED(context);
    if(fred_fm_app_exiting) return;
    FuriThreadId id = rds_dsp_worker_thread_id;
    if(id) {
        furi_thread_flags_set(id, RDS_DSP_WORKER_FLAG_TICK);
    }
}

static void fred_fm_rds_change(VariableItem* item) {
    UNUSED(variable_item_get_context(item));
    uint8_t index = variable_item_get_current_value_index(item);
    rds_enabled = (index != 0);
    variable_item_set_current_value_text(item, rds_enabled ? "On" : "Off");

    fred_fm_rds_apply_runtime_state(true);
    fred_fm_settings_mark_dirty();
    fred_fm_settings_save();
}

static void fred_fm_rds_debug_change(VariableItem* item) {
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
#endif /* ENABLE_RDS */

static void fred_fm_snc_change(VariableItem* item) {
    UNUSED(variable_item_get_context(item));

    uint8_t index = variable_item_get_current_value_index(item);
    tea_snc_enabled = (index != 0);
    variable_item_set_current_value_text(item, tea_snc_enabled ? "On" : "Off");

    tea5767_set_snc_enabled(tea_snc_enabled);
    (void)tea5767_set_snc(tea_snc_enabled);
    fred_fm_settings_mark_dirty();
}

static void fred_fm_deemph_change(VariableItem* item) {
    UNUSED(variable_item_get_context(item));
    uint8_t index = variable_item_get_current_value_index(item);

    // index 0 => 50us, index 1 => 75us
    tea_deemph_75us = (index != 0);
    variable_item_set_current_value_text(item, tea_deemph_75us ? "75us" : "50us");

    tea5767_set_deemphasis_75us_enabled(tea_deemph_75us);
    (void)tea5767_set_deemphasis_75us(tea_deemph_75us);
    fred_fm_settings_mark_dirty();
}

static void fred_fm_highcut_change(VariableItem* item) {
    UNUSED(variable_item_get_context(item));
    uint8_t index = variable_item_get_current_value_index(item);
    tea_highcut_enabled = (index != 0);
    variable_item_set_current_value_text(item, tea_highcut_enabled ? "On" : "Off");
    tea5767_set_high_cut_enabled(tea_highcut_enabled);
    (void)tea5767_set_high_cut(tea_highcut_enabled);
    fred_fm_settings_mark_dirty();
}

static void fred_fm_mono_change(VariableItem* item) {
    UNUSED(variable_item_get_context(item));
    uint8_t index = variable_item_get_current_value_index(item);
    tea_force_mono_enabled = (index != 0);
    variable_item_set_current_value_text(item, tea_force_mono_enabled ? "On" : "Off");
    tea5767_set_force_mono_enabled(tea_force_mono_enabled);
    (void)tea5767_set_force_mono(tea_force_mono_enabled);
    fred_fm_settings_mark_dirty();
}

static void fred_fm_backlight_change(VariableItem* item) {
    UNUSED(variable_item_get_context(item));
    uint8_t index = variable_item_get_current_value_index(item);
    backlight_keep_on = (index != 0);
    variable_item_set_current_value_text(item, backlight_keep_on ? "On" : "Off");

    NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);
    fred_fm_apply_backlight(notifications);
    furi_record_close(RECORD_NOTIFICATION);

    fred_fm_settings_mark_dirty();
}

static void fred_fm_amp_power_change(VariableItem* item) {
    UNUSED(variable_item_get_context(item));
    uint8_t index = variable_item_get_current_value_index(item);
    if(index >= COUNT_OF(amp_power_names)) {
        index = 0;
        variable_item_set_current_value_index(item, index);
    }

    amp_power_enabled = (index != 0);
    variable_item_set_current_value_text(item, amp_power_names[index]);
    fred_fm_apply_audio_output_state();
    fred_fm_settings_mark_dirty();
}

static void fred_fm_amp_mode_change(VariableItem* item) {
    UNUSED(variable_item_get_context(item));
    uint8_t index = variable_item_get_current_value_index(item);
    if(index >= COUNT_OF(amp_mode_names)) {
        index = 0;
        variable_item_set_current_value_index(item, index);
    }

    amp_mode_class_d = (index != 0);
    variable_item_set_current_value_text(item, amp_mode_names[index]);
    fred_fm_apply_audio_output_state();
    fred_fm_settings_mark_dirty();
}

//lib can only do bottom left/right
static void elements_button_top_left(Canvas* canvas, const char* str) {
    const uint8_t button_height = 12;
    const uint8_t vertical_offset = 3;
    const uint8_t horizontal_offset = 3;
    const uint8_t string_width = canvas_string_width(canvas, str);
    const uint8_t button_width = string_width + horizontal_offset * 2 + 3;

    const uint8_t x = 0;
    const uint8_t y = 0 + button_height;

    canvas_draw_box(canvas, x, y - button_height, button_width, button_height);
    canvas_draw_line(canvas, x + button_width + 0, y - button_height, x + button_width + 0, y - 1);
    canvas_draw_line(canvas, x + button_width + 1, y - button_height, x + button_width + 1, y - 2);
    canvas_draw_line(canvas, x + button_width + 2, y - button_height, x + button_width + 2, y - 3);

    canvas_invert_color(canvas);
    canvas_draw_str(canvas, x + horizontal_offset + 3, y - vertical_offset, str);
    canvas_invert_color(canvas);
}

static void elements_button_top_right(Canvas* canvas, const char* str) {
    const uint8_t button_height = 12;
    const uint8_t vertical_offset = 3;
    const uint8_t horizontal_offset = 3;
    const uint8_t string_width = canvas_string_width(canvas, str);
    const uint8_t button_width = string_width + horizontal_offset * 2 + 3;

    const uint8_t x = canvas_width(canvas);
    const uint8_t y = 0 + button_height;

    canvas_draw_box(canvas, x - button_width, y - button_height, button_width, button_height);
    canvas_draw_line(canvas, x - button_width - 1, y - button_height, x - button_width - 1, y - 1);
    canvas_draw_line(canvas, x - button_width - 2, y - button_height, x - button_width - 2, y - 2);
    canvas_draw_line(canvas, x - button_width - 3, y - button_height, x - button_width - 3, y - 3);

    canvas_invert_color(canvas);
    canvas_draw_str(canvas, x - button_width + horizontal_offset, y - vertical_offset, str);
    canvas_invert_color(canvas);
}

// Enumerations for submenu and view indices
typedef enum {
    FredFmSubmenuIndexListen,
    FredFmSubmenuIndexConstellation,
    FredFmSubmenuIndexConfigure,
    FredFmSubmenuIndexAbout,
} FredFmSubmenuIndex;

typedef enum {
    FredFmViewSubmenu,
    FredFmViewConfigure,
    FredFmViewListen,
    FredFmViewConstellation,
    FredFmViewAbout,
} FredFmView;

// Define a struct to hold the application's components
struct FredFm {
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;
    Submenu* submenu;
    VariableItemList* variable_item_list_config;
    VariableItem* item_freq;
    VariableItem* item_volume;
    VariableItem* item_pt_chip;
    VariableItem* item_amp_power;
    VariableItem* item_amp_mode;
    VariableItem* item_snc;
    VariableItem* item_deemph;
    VariableItem* item_highcut;
    VariableItem* item_mono;
    VariableItem* item_backlight;
#ifdef ENABLE_RDS
    VariableItem* item_rds;
    VariableItem* item_rds_debug;
#endif
    View* listen_view;
    View* constellation_view;
    Widget* widget_about;
    FuriTimer* tick_timer;
#ifdef ENABLE_RDS
    FuriTimer* rds_adc_timer;
#endif
};

// Model struct for the Listen view (state lives in globals; kept for view_commit_model redraws)
typedef struct {
    uint8_t _dummy; // Flipper view system requires a non-zero model
} MyModel;

// Callback for navigation events

/* Back button on the top-level submenu.
 *
 * All shutdown work lives in fred_fm_free() — the single sequential
 * exit path. Here we only:
 *   1. flip fred_fm_app_exiting so timer/worker callbacks early-return,
 *   2. return VIEW_NONE so view_dispatcher_run() exits its event loop and
 *      hands control back to fred_fm_app(), which then invokes
 *      fred_fm_free() on the same (app main) thread.
 *
 * Doing teardown here AND in free() previously caused duplicated state
 * mutations and a use-after-free race between the still-armed rds_adc_timer
 * and the freed DSP worker thread. Centralising teardown in free() removes
 * that race entirely. */
static uint32_t fred_fm_navigation_exit_callback(void* context) {
    UNUSED(context);
    fred_fm_app_exiting = true;
    return VIEW_NONE;
}

// Callback for navigating to the submenu
static uint32_t fred_fm_navigation_submenu_callback(void* context) {
    UNUSED(context);
#ifdef ENABLE_RDS
    rds_constellation_view_active = false;
#endif
    return FredFmViewSubmenu;
}

static void fred_fm_redraw_listen_view(View* view) {
    if(view) {
        view_get_model(view);
        view_commit_model(view, true);
    }
}

static void fred_fm_redraw_constellation_view(View* view) {
    if(view) {
        view_get_model(view);
        view_commit_model(view, true);
    }
}

static bool fred_fm_submenu_rebuild(FredFm* app) {
    if(!app || !app->submenu) {
        return false;
    }

    submenu_reset(app->submenu);

    submenu_add_item(
        app->submenu, "Listen Now", FredFmSubmenuIndexListen, fred_fm_submenu_callback, app);
#ifdef ENABLE_RDS
    if(rds_debug_enabled) {
        submenu_add_item(
            app->submenu,
            "Constellation Visualizer",
            FredFmSubmenuIndexConstellation,
            fred_fm_submenu_callback,
            app);
    }
#endif
    submenu_add_item(
        app->submenu, "Config", FredFmSubmenuIndexConfigure, fred_fm_submenu_callback, app);
    submenu_add_item(
        app->submenu, "About", FredFmSubmenuIndexAbout, fred_fm_submenu_callback, app);

    return true;
}

// Callback for handling submenu selections
static void fred_fm_submenu_callback(void* context, uint32_t index) {
    FredFm* app = (FredFm*)context;
    switch(index) {
    case FredFmSubmenuIndexListen:
#ifdef ENABLE_RDS
        rds_constellation_view_active = false;
#endif
        view_dispatcher_switch_to_view(app->view_dispatcher, FredFmViewListen);
        break;
#ifdef ENABLE_RDS
    case FredFmSubmenuIndexConstellation:
        rds_constellation_view_active = true;
        fred_fm_rds_constellation_clear_history();
        view_dispatcher_switch_to_view(app->view_dispatcher, FredFmViewConstellation);
        break;
#endif
    case FredFmSubmenuIndexConfigure:
#ifdef ENABLE_RDS
        rds_constellation_view_active = false;
#endif
        view_dispatcher_switch_to_view(app->view_dispatcher, FredFmViewConfigure);
        break;
    case FredFmSubmenuIndexAbout:
#ifdef ENABLE_RDS
        rds_constellation_view_active = false;
#endif
        view_dispatcher_switch_to_view(app->view_dispatcher, FredFmViewAbout);
        break;
    default:
        break;
    }
}

bool fred_fm_view_input_callback(InputEvent* event, void* context) {
    FredFm* app = (FredFm*)context;
    if(event->type == InputTypeLong && event->key == InputKeyLeft) {
        fred_fm_seek_step(false);
        fred_fm_redraw_listen_view(app ? app->listen_view : NULL);
        return true;
    } else if(event->type == InputTypeLong && event->key == InputKeyRight) {
        fred_fm_seek_step(true);
        fred_fm_redraw_listen_view(app ? app->listen_view : NULL);
        return true;
    } else if(event->type == InputTypeShort && event->key == InputKeyLeft) {
        // Use integer 10kHz math to avoid PLL quantization drift
        uint32_t fq = fred_fm_get_current_freq_10khz();
        // Snap to nearest 100 kHz (10 units) grid before stepping
        fq = ((fq + 5) / 10) * 10;
        if(fq > 10)
            fq -= 10;
        else
            fq = 7600;
        fq = clamp_u32(fq, 7600U, 10800U);
        fred_fm_tune_nominal_freq_10khz(fq);
        fred_fm_rds_on_tuned_frequency_changed();
        fred_fm_redraw_listen_view(app ? app->listen_view : NULL);
        return true;
    } else if(event->type == InputTypeShort && event->key == InputKeyRight) {
        // Use integer 10kHz math to avoid PLL quantization drift
        uint32_t fq = fred_fm_get_current_freq_10khz();
        // Snap to nearest 100 kHz (10 units) grid before stepping
        fq = ((fq + 5) / 10) * 10;
        fq += 10;
        fq = clamp_u32(fq, 7600U, 10800U);
        fred_fm_tune_nominal_freq_10khz(fq);
        fred_fm_rds_on_tuned_frequency_changed();
        fred_fm_redraw_listen_view(app ? app->listen_view : NULL);
        return true;
    } else if(event->type == InputTypeLong && event->key == InputKeyOk) {
        // Save current frequency to presets: select if already present, otherwise append
        uint32_t freq_10khz = fred_fm_get_current_freq_10khz();
        int16_t offset_centihz = 0;
#ifdef ENABLE_RDS
        offset_centihz = fred_fm_rds_get_manual_offset_centihz();
#endif
        fred_fm_presets_add_or_select(freq_10khz, offset_centihz);
        fred_fm_presets_save();
        fred_fm_feedback_success();
        fred_fm_redraw_listen_view(app ? app->listen_view : NULL);
        return true;
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        fred_fm_state_lock();
        current_volume = !current_volume;
        fred_fm_state_unlock();
        fred_fm_apply_audio_output_state();
        fred_fm_settings_mark_dirty();
        fred_fm_redraw_listen_view(app ? app->listen_view : NULL);
        return true; // Event was handled
    } else if(event->type == InputTypeShort && event->key == InputKeyUp) {
        (void)fred_fm_presets_step_and_apply(true);
        fred_fm_redraw_listen_view(app ? app->listen_view : NULL);
        return true; // Event was handled
    } else if(event->type == InputTypeShort && event->key == InputKeyDown) {
        (void)fred_fm_presets_step_and_apply(false);
        fred_fm_redraw_listen_view(app ? app->listen_view : NULL);
        return true; // Event was handled
    } else if(
        (event->type == InputTypeLong || event->type == InputTypeRepeat) &&
        event->key == InputKeyUp) {
        // Volume up => reduce attenuation
        fred_fm_state_lock();
        if(pt_atten_db > 0) {
            pt_atten_db--;
            fred_fm_state_unlock();
            fred_fm_apply_audio_output_state();
            fred_fm_settings_mark_dirty();
            fred_fm_redraw_listen_view(app ? app->listen_view : NULL);
        } else {
            fred_fm_state_unlock();
        }
        return true;
    } else if(
        (event->type == InputTypeLong || event->type == InputTypeRepeat) &&
        event->key == InputKeyDown) {
        // Volume down => increase attenuation
        fred_fm_state_lock();
        if(pt_atten_db < 79) {
            pt_atten_db++;
            fred_fm_state_unlock();
            fred_fm_apply_audio_output_state();
            fred_fm_settings_mark_dirty();
            fred_fm_redraw_listen_view(app ? app->listen_view : NULL);
        } else {
            fred_fm_state_unlock();
        }
        return true;
    }

    return false; // Event was not handled
}

#ifdef ENABLE_RDS
bool fred_fm_constellation_view_input_callback(InputEvent* event, void* context) {
    FredFm* app = (FredFm*)context;

    if(event->type == InputTypeShort && event->key == InputKeyLeft) {
        int16_t offset_centihz = fred_fm_rds_get_manual_offset_centihz();
        fred_fm_rds_set_manual_offset_centihz((int16_t)(offset_centihz - 1));
        fred_fm_redraw_constellation_view(app ? app->constellation_view : NULL);
        return true;
    } else if(event->type == InputTypeShort && event->key == InputKeyRight) {
        int16_t offset_centihz = fred_fm_rds_get_manual_offset_centihz();
        fred_fm_rds_set_manual_offset_centihz((int16_t)(offset_centihz + 1));
        fred_fm_redraw_constellation_view(app ? app->constellation_view : NULL);
        return true;
    } else if(event->type == InputTypeShort && event->key == InputKeyUp) {
        (void)fred_fm_presets_step_and_apply(true);
        fred_fm_redraw_constellation_view(app ? app->constellation_view : NULL);
        return true;
    } else if(event->type == InputTypeShort && event->key == InputKeyDown) {
        (void)fred_fm_presets_step_and_apply(false);
        fred_fm_redraw_constellation_view(app ? app->constellation_view : NULL);
        return true;
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        uint32_t freq_10khz = fred_fm_get_current_freq_10khz();
        int16_t offset_centihz = fred_fm_rds_get_manual_offset_centihz();
        fred_fm_presets_add_or_select(freq_10khz, offset_centihz);
        fred_fm_presets_save();

        fred_fm_state_lock();
        rds_constellation_saved_until_tick = furi_get_tick() + furi_ms_to_ticks(1500U);
        fred_fm_state_unlock();

        fred_fm_feedback_success();
        fred_fm_redraw_constellation_view(app ? app->constellation_view : NULL);
        return true;
    } else if(event->type == InputTypeLong && event->key == InputKeyOk) {
#if ENABLE_ADC_CAPTURE
        if(!rds_capture_active && !rds_capture_finalize_pending) {
            rds_capture_requested = true;
            fred_fm_feedback_success();
        }
#endif
        fred_fm_redraw_constellation_view(app ? app->constellation_view : NULL);
        return true;
    }

    return false;
}

void fred_fm_constellation_view_draw_callback(Canvas* canvas, void* model) {
    (void)model;

    char freq_display[20];
    char offset_display[24];
    char status_display[32];
    char rds_ps_local[RDS_PS_LEN + 1U] = {0};

    uint32_t local_freq_10khz = 0U;
    int16_t local_offset_centihz = 0;
    uint8_t local_hist_count = 0U;
    uint8_t local_hist_write_index = 0U;
    bool local_saved_active = false;
    bool local_status_visible = false;
#if ENABLE_ADC_CAPTURE
    bool local_capture_active = false;
    bool local_capture_finalize_pending = false;
    bool local_capture_complete = false;
    bool local_capture_error = false;
    bool local_capture_partial = false;
    uint32_t local_capture_captured_blocks = 0U;
    uint32_t local_capture_written_blocks = 0U;
    uint32_t local_capture_target_blocks = 0U;
#endif

    fred_fm_state_lock();
    local_freq_10khz = tea_nominal_freq_10khz;
    local_offset_centihz = rds_carrier_offset_centihz;
    memcpy(rds_ps_local, rds_ps_display, sizeof(rds_ps_local));
    local_saved_active = (rds_constellation_saved_until_tick > furi_get_tick());
#if ENABLE_ADC_CAPTURE
    local_capture_active = rds_capture_active;
    local_capture_finalize_pending = rds_capture_finalize_pending;
    local_capture_complete = rds_capture_complete;
    local_capture_error = rds_capture_error;
    local_capture_captured_blocks = rds_capture_captured_blocks;
    local_capture_written_blocks = rds_capture_written_blocks;
    local_capture_target_blocks = rds_capture_target_blocks;
    local_capture_partial = !local_capture_active && !local_capture_finalize_pending &&
                            !local_capture_complete && (local_capture_written_blocks > 0U);
#endif
    fred_fm_state_unlock();

    FURI_CRITICAL_ENTER();
    local_hist_count = rds_constellation_history_count;
    local_hist_write_index = rds_constellation_history_write_index;
    for(uint8_t i = 0U; i < local_hist_count; i++) {
        uint32_t idx = (uint32_t)(local_hist_write_index + RDS_CONSTELLATION_HISTORY_LEN -
                                  local_hist_count + i) %
                       RDS_CONSTELLATION_HISTORY_LEN;
        rds_constellation_i_snapshot[i] = rds_constellation_i_history[idx];
        rds_constellation_q_snapshot[i] = rds_constellation_q_history[idx];
    }
    FURI_CRITICAL_EXIT();

    canvas_set_font(canvas, FontSecondary);

    snprintf(
        freq_display,
        sizeof(freq_display),
        "%lu.%lu",
        (unsigned long)(local_freq_10khz / 100U),
        (unsigned long)((local_freq_10khz / 10U) % 10U));
    canvas_draw_str(canvas, 1, 8, freq_display);

    if(rds_ps_local[0] != '\0') {
        uint8_t ps_width = canvas_string_width(canvas, rds_ps_local);
        uint8_t ps_x = (uint8_t)((canvas_width(canvas) - ps_width) / 2U);
        canvas_draw_str(canvas, ps_x, 8, rds_ps_local);
    }

    uint16_t local_offset_abs =
        (uint16_t)((local_offset_centihz < 0) ? -local_offset_centihz : local_offset_centihz);
    snprintf(
        offset_display,
        sizeof(offset_display),
        "dF %c%u.%02u",
        (local_offset_centihz < 0) ? '-' : '+',
        (unsigned)(local_offset_abs / 100U),
        (unsigned)(local_offset_abs % 100U));
    uint8_t offset_width = canvas_string_width(canvas, offset_display);
    canvas_draw_str(
        canvas, (uint8_t)(canvas_width(canvas) - offset_width - 1U), 8, offset_display);

    const int32_t plot_radius_left = RDS_CONSTELLATION_CENTER_X - RDS_CONSTELLATION_PLOT_LEFT - 1;
    const int32_t plot_radius_right =
        RDS_CONSTELLATION_PLOT_RIGHT - RDS_CONSTELLATION_CENTER_X - 1;
    const int32_t plot_radius_top = RDS_CONSTELLATION_CENTER_Y - RDS_CONSTELLATION_PLOT_TOP - 1;
    const int32_t plot_radius_bottom =
        RDS_CONSTELLATION_PLOT_BOTTOM - RDS_CONSTELLATION_CENTER_Y - 1;
    const int32_t plot_radius_x = (plot_radius_left < plot_radius_right) ? plot_radius_left :
                                                                           plot_radius_right;
    const int32_t plot_radius_y = (plot_radius_top < plot_radius_bottom) ? plot_radius_top :
                                                                           plot_radius_bottom;
    const int32_t plot_radius = (plot_radius_x < plot_radius_y) ? plot_radius_x : plot_radius_y;

    int32_t max_abs = 1;
    for(uint8_t i = 0U; i < local_hist_count; i++) {
        int32_t i_abs = (rds_constellation_i_snapshot[i] < 0) ? -rds_constellation_i_snapshot[i] :
                                                                rds_constellation_i_snapshot[i];
        int32_t q_abs = (rds_constellation_q_snapshot[i] < 0) ? -rds_constellation_q_snapshot[i] :
                                                                rds_constellation_q_snapshot[i];
        if(i_abs > max_abs) max_abs = i_abs;
        if(q_abs > max_abs) max_abs = q_abs;
    }

    for(uint8_t i = 0U; i < local_hist_count; i++) {
        int32_t px = RDS_CONSTELLATION_CENTER_X +
                     (int32_t)(((int64_t)rds_constellation_i_snapshot[i] * plot_radius) / max_abs);
        int32_t py = RDS_CONSTELLATION_CENTER_Y -
                     (int32_t)(((int64_t)rds_constellation_q_snapshot[i] * plot_radius) / max_abs);

        if(px < RDS_CONSTELLATION_PLOT_LEFT) px = RDS_CONSTELLATION_PLOT_LEFT;
        if(px > RDS_CONSTELLATION_PLOT_RIGHT) px = RDS_CONSTELLATION_PLOT_RIGHT;
        if(py < RDS_CONSTELLATION_PLOT_TOP) py = RDS_CONSTELLATION_PLOT_TOP;
        if(py > RDS_CONSTELLATION_PLOT_BOTTOM) py = RDS_CONSTELLATION_PLOT_BOTTOM;

        canvas_draw_box(canvas, (uint8_t)px, (uint8_t)py, 1, 1);
    }

#if ENABLE_ADC_CAPTURE
    if(local_capture_active) {
        uint32_t pct = (local_capture_target_blocks > 0U) ?
                           (local_capture_captured_blocks * 100U) / local_capture_target_blocks :
                           0U;
        snprintf(status_display, sizeof(status_display), "REC %lu%%", (unsigned long)pct);
        local_status_visible = true;
    } else if(local_capture_finalize_pending) {
        snprintf(status_display, sizeof(status_display), "REC WR");
        local_status_visible = true;
    } else if(local_capture_complete) {
        snprintf(status_display, sizeof(status_display), "REC OK");
        local_status_visible = true;
    } else if(local_capture_partial) {
        snprintf(status_display, sizeof(status_display), "REC PART");
        local_status_visible = true;
    } else if(local_capture_error) {
        snprintf(status_display, sizeof(status_display), "REC ERR");
        local_status_visible = true;
    } else
#endif
        if(local_saved_active) {
        snprintf(status_display, sizeof(status_display), "SAVE OK");
        local_status_visible = true;
    }

    if(local_status_visible) {
        canvas_draw_str(canvas, 1, 63, status_display);
    }
}
#endif

// Callback for handling volume changes
void fred_fm_volume_change(VariableItem* item) {
    uint8_t index = variable_item_get_current_value_index(item);
    if(index >= COUNT_OF(volume_values)) {
        index = 0;
        variable_item_set_current_value_index(item, index);
    }
    variable_item_set_current_value_text(
        item, volume_names[index]); // Display the selected volume as text

    // Apply immediately (this Config "Volume" is shared audio mute/unmute)
    if(index < COUNT_OF(volume_values)) {
        current_volume = (volume_values[index] != 0);
        fred_fm_apply_audio_output_state();
        fred_fm_settings_mark_dirty();
    }
}

void fred_fm_pt_chip_change(VariableItem* item) {
    uint8_t index = variable_item_get_current_value_index(item);
    if(index >= COUNT_OF(pt_chip_values)) {
        index = 0;
        variable_item_set_current_value_index(item, index);
    }

    pt_chip = pt_chip_values[index];
    variable_item_set_current_value_text(item, pt_chip_names[index]);

    (void)fred_fm_pt_refresh_state(true);
    fred_fm_apply_audio_output_state();
    fred_fm_settings_mark_dirty();
}

// Periodic background tick: I2C hot-plug check, debounced saves.
// Runs every 250 ms via FuriTimer, independent of which view is active.
static void fred_fm_tick_callback(void* context) {
    /* Guard FIRST — furi_timer_free() is async. A queued callback can fire
     * after free(app), so dereferencing context before the exit check is a
     * use-after-free → HardFault. Null-check context to catch the case where
     * the timer was already freed but the callback was already queued. */
    if(!context || fred_fm_app_exiting) return;
    FredFm* app = (FredFm*)context;
    uint32_t now = furi_get_tick();

    // PT hot-plug (every ~500 ms)
    static uint32_t last_pt_check = 0;
    if((now - last_pt_check) > furi_ms_to_ticks(500)) {
        fred_fm_state_lock();
        bool was_ready = pt_ready_cached;
        fred_fm_state_unlock();

        bool ready = fred_fm_pt_refresh_state(false);

        if(ready && !was_ready) {
            fred_fm_apply_audio_output_state();
        }
        last_pt_check = now;
    }

    // Debounced settings save (every ~2 s when dirty)
    static uint32_t last_settings_save = 0;
    if(settings_dirty && ((now - last_settings_save) > furi_ms_to_ticks(2000))) {
        fred_fm_settings_save();
        last_settings_save = now;
    }

#ifdef ENABLE_RDS
    if(fred_fm_rds_pipeline_enabled()) {
        fred_fm_rds_process_events();
    }
#endif

    // Debounced presets save (every ~2 s when dirty)
    static uint32_t last_presets_save = 0;
    if(presets_dirty && ((now - last_presets_save) > furi_ms_to_ticks(2000))) {
        fred_fm_presets_save();
        last_presets_save = now;
    }

    // Refresh TEA5767 radio info every ~2 s. The chip is wired to fixed pins; we only
    // need a periodic heartbeat to detect hot-unplug. Frequency / mute / preset changes
    // already update internal state directly via fred_fm_tune_nominal_freq_10khz().
    static uint32_t last_tea_poll = 0U;
    if((now - last_tea_poll) >= furi_ms_to_ticks(2000U)) {
        last_tea_poll = now;
        uint8_t tea_buf[5];
        struct RADIO_INFO info;
        if(tea5767_get_radio_info(tea_buf, &info)) {
            fred_fm_state_lock();
            bool was_ready = tea_i2c_ready;
            tea_info_cached = info;
            tea_info_valid = true;
            tea_i2c_ready = true;
            tea_i2c_failure_count = 0U;
            tea_info_read_count++;
            fred_fm_state_unlock();

            if(!was_ready && fred_fm_rds_pipeline_enabled()) {
                fred_fm_rds_pipeline_start();
                // Start timer so worker can begin processing
                if(rds_adc_timer_handle && !rds_adc_timer_running) {
                    fred_fm_rds_timer_start();
                }
            }
        } else {
            bool stop_rds = false;
            fred_fm_state_lock();
            if(tea_i2c_failure_count < TEA_I2C_RDS_FAILURE_LIMIT) {
                tea_i2c_failure_count++;
            }
            if(tea_i2c_failure_count >= TEA_I2C_RDS_FAILURE_LIMIT) {
                stop_rds = tea_i2c_ready;
                tea_info_valid = false;
                tea_i2c_ready = false;
            }
            fred_fm_state_unlock();

            if(stop_rds) {
                fred_fm_rds_apply_runtime_state(false);
            }
        }
        UNUSED(now);
    }

    // Trigger a redraw so the Listen view picks up fresh data
    if(app->listen_view) {
        fred_fm_redraw_listen_view(app->listen_view);
    }
#ifdef ENABLE_RDS
    if(rds_constellation_view_active && app->constellation_view) {
        fred_fm_redraw_constellation_view(app->constellation_view);
    }
#endif
}

// Callback for drawing the view

void fred_fm_view_draw_callback(Canvas* canvas, void* model) {
    (void)model; // Mark model as unused

    char title_display[24];
    char frequency_display[64];
    char signal_display[64];
    char tuning_display[64];
    char audio_display[48];
    uint8_t title_x;
#ifdef ENABLE_RDS
    char rds_ps_local[RDS_PS_LEN + 1U];
    bool local_rds_enabled;
    bool local_rds_debug_enabled;
    RdsSyncState local_rds_sync;
#endif

    fred_fm_state_lock();
    uint8_t local_pt_atten = pt_atten_db;
    bool local_muted = current_volume;
    struct RADIO_INFO info = tea_info_cached;
    bool info_valid = tea_info_valid;
    uint32_t nominal_freq_10khz = tea_nominal_freq_10khz;
#ifdef ENABLE_RDS
    local_rds_enabled = rds_enabled;
    local_rds_debug_enabled = rds_debug_enabled;
    local_rds_sync = rds_sync_display;
    memcpy(rds_ps_local, rds_ps_display, sizeof(rds_ps_local));
#endif
    fred_fm_state_unlock();

#ifdef ENABLE_RDS
    if(rds_ps_local[0] != '\0') {
        snprintf(title_display, sizeof(title_display), "%.*s", (int)RDS_PS_LEN, rds_ps_local);
    } else
#endif
    {
        snprintf(title_display, sizeof(title_display), "Radio FM");
    }

    canvas_set_font(canvas, FontPrimary);
    title_x = (uint8_t)((canvas_width(canvas) - canvas_string_width(canvas, title_display)) / 2U);
    canvas_draw_str(canvas, title_x, 10, title_display);

    // Draw button prompts
    canvas_set_font(canvas, FontSecondary);
    elements_button_left(canvas, "-0.1");
    elements_button_right(canvas, "+0.1");
    elements_button_center(canvas, "Mute");
    elements_button_top_left(canvas, " Pre");
    elements_button_top_right(canvas, "Pre ");

    if(info_valid) {
        snprintf(
            frequency_display,
            sizeof(frequency_display),
            "F: %.1f MHz",
            (double)(((float)fred_fm_get_current_freq_10khz()) / 100.0f));
        canvas_draw_str(canvas, 10, 21, frequency_display);

        snprintf(
            signal_display,
            sizeof(signal_display),
            "RSSI:%d %s",
            info.signalLevel,
            info.signalQuality);
        canvas_draw_str(canvas, 10, 41, signal_display);

        if(local_muted) {
            snprintf(
                audio_display, sizeof(audio_display), "A:MT V:-%udB", (unsigned)local_pt_atten);
        } else {
            snprintf(
                audio_display,
                sizeof(audio_display),
                "A:%s V:-%udB",
                info.stereo ? "ST" : "MO",
                (unsigned)local_pt_atten);
        }
        canvas_draw_str(canvas, 10, 31, audio_display);

#ifdef ENABLE_RDS
        if(local_rds_debug_enabled) {
            snprintf(
                tuning_display,
                sizeof(tuning_display),
                "IF:%02u E:%+ld R:%s",
                (unsigned)info.ifCounter,
                (long)((int32_t)info.ifCounter - (int32_t)TEA_IF_COUNT_TARGET),
                (local_rds_enabled || local_rds_debug_enabled) ?
                    fred_fm_rds_sync_short_text(local_rds_sync) :
                    "off");
        } else {
#endif
            snprintf(
                tuning_display,
                sizeof(tuning_display),
                "IF:%02u E:%+ld",
                (unsigned)info.ifCounter,
                (long)((int32_t)info.ifCounter - (int32_t)TEA_IF_COUNT_TARGET));
#ifdef ENABLE_RDS
        }
#endif
        if(nominal_freq_10khz >= 7600U && nominal_freq_10khz <= 10800U) {
            canvas_draw_str(canvas, 10, 51, tuning_display);
        }
    } else {
        snprintf(frequency_display, sizeof(frequency_display), "TEA5767 Not Detected");
        canvas_draw_str(canvas, 10, 21, frequency_display);

        snprintf(signal_display, sizeof(signal_display), "Pin 15 = SDA | Pin 16 = SCL");
        canvas_draw_str(canvas, 10, 41, signal_display);
    }
}

// Allocate memory for the application
FredFm* fred_fm_alloc() {
    FredFm* app = (FredFm*)malloc(sizeof(FredFm));
    if(!app) return NULL;
    memset(app, 0, sizeof(FredFm));

    pam8406_init();

    bool gui_opened = false;
    Gui* gui = furi_record_open(RECORD_GUI);
    if(!gui) goto fail;
    gui_opened = true;

    state_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    if(!state_mutex) goto fail;

    // Load persisted state before building menu/config items, so UI starts from saved values.
    fred_fm_presets_load();
    fred_fm_settings_load();

    // Initialize the view dispatcher
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(app->view_dispatcher);
    if(!app->view_dispatcher) goto fail;
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);

    // Initialize submenu entries
    app->submenu = submenu_alloc();
    if(!app->submenu) goto fail;
    view_set_previous_callback(submenu_get_view(app->submenu), fred_fm_navigation_exit_callback);
    if(!fred_fm_submenu_rebuild(app)) goto fail;
    view_dispatcher_add_view(
        app->view_dispatcher, FredFmViewSubmenu, submenu_get_view(app->submenu));
    view_dispatcher_switch_to_view(app->view_dispatcher, FredFmViewSubmenu);

    // Initialize the variable item list for configuration
    app->variable_item_list_config = variable_item_list_alloc();
    if(!app->variable_item_list_config) goto fail;
    variable_item_list_reset(app->variable_item_list_config);

    // Add TEA5767 SNC toggle
    app->item_snc =
        variable_item_list_add(app->variable_item_list_config, "SNC", 2, fred_fm_snc_change, app);
    if(!app->item_snc) goto fail;
    variable_item_set_current_value_index(app->item_snc, tea_snc_enabled ? 1 : 0);
    variable_item_set_current_value_text(app->item_snc, tea_snc_enabled ? "On" : "Off");

    // Add TEA5767 de-emphasis time constant
    app->item_deemph = variable_item_list_add(
        app->variable_item_list_config, "De-emph", 2, fred_fm_deemph_change, app);
    if(!app->item_deemph) goto fail;
    variable_item_set_current_value_index(app->item_deemph, tea_deemph_75us ? 1 : 0);
    variable_item_set_current_value_text(app->item_deemph, tea_deemph_75us ? "75us" : "50us");

    // Add TEA5767 High Cut Control
    app->item_highcut = variable_item_list_add(
        app->variable_item_list_config, "HighCut", 2, fred_fm_highcut_change, app);
    if(!app->item_highcut) goto fail;
    variable_item_set_current_value_index(app->item_highcut, tea_highcut_enabled ? 1 : 0);
    variable_item_set_current_value_text(app->item_highcut, tea_highcut_enabled ? "On" : "Off");

    // Add TEA5767 Force mono
    app->item_mono = variable_item_list_add(
        app->variable_item_list_config, "Mono", 2, fred_fm_mono_change, app);
    if(!app->item_mono) goto fail;
    variable_item_set_current_value_index(app->item_mono, tea_force_mono_enabled ? 1 : 0);
    variable_item_set_current_value_text(app->item_mono, tea_force_mono_enabled ? "On" : "Off");

    // Keep backlight on while app runs
    app->item_backlight = variable_item_list_add(
        app->variable_item_list_config, "Backlight", 2, fred_fm_backlight_change, app);
    if(!app->item_backlight) goto fail;
    variable_item_set_current_value_index(app->item_backlight, backlight_keep_on ? 1 : 0);
    variable_item_set_current_value_text(app->item_backlight, backlight_keep_on ? "On" : "Off");

#ifdef ENABLE_RDS
    app->item_rds =
        variable_item_list_add(app->variable_item_list_config, "RDS", 2, fred_fm_rds_change, app);
    if(!app->item_rds) goto fail;
    variable_item_set_current_value_index(app->item_rds, rds_enabled ? 1 : 0);
    variable_item_set_current_value_text(app->item_rds, rds_enabled ? "On" : "Off");

    app->item_rds_debug = variable_item_list_add(
        app->variable_item_list_config, "RDS Debug", 2, fred_fm_rds_debug_change, app);
    if(!app->item_rds_debug) goto fail;
    variable_item_set_current_value_index(app->item_rds_debug, rds_debug_enabled ? 1 : 0);
    variable_item_set_current_value_text(app->item_rds_debug, rds_debug_enabled ? "On" : "Off");
#endif

    // Add volume configuration
    app->item_volume = variable_item_list_add(
        app->variable_item_list_config,
        "Volume",
        COUNT_OF(volume_values),
        fred_fm_volume_change,
        app);
    if(!app->item_volume) goto fail;
    uint8_t volume_index = 0;
    variable_item_set_current_value_index(app->item_volume, volume_index);

    app->item_pt_chip = variable_item_list_add(
        app->variable_item_list_config,
        "PT Chip",
        COUNT_OF(pt_chip_values),
        fred_fm_pt_chip_change,
        app);
    if(!app->item_pt_chip) goto fail;
    uint8_t chip_index = 0;
    for(uint8_t i = 0; i < COUNT_OF(pt_chip_values); i++) {
        if(pt_chip_values[i] == pt_chip) {
            chip_index = i;
            break;
        }
    }
    variable_item_set_current_value_index(app->item_pt_chip, chip_index);
    variable_item_set_current_value_text(app->item_pt_chip, pt_chip_names[chip_index]);

    app->item_amp_power = variable_item_list_add(
        app->variable_item_list_config,
        "Amp Power",
        COUNT_OF(amp_power_names),
        fred_fm_amp_power_change,
        app);
    if(!app->item_amp_power) goto fail;
    variable_item_set_current_value_index(app->item_amp_power, amp_power_enabled ? 1 : 0);
    variable_item_set_current_value_text(app->item_amp_power, amp_power_enabled ? "On" : "Off");

    app->item_amp_mode = variable_item_list_add(
        app->variable_item_list_config,
        "Amp Mode",
        COUNT_OF(amp_mode_names),
        fred_fm_amp_mode_change,
        app);
    if(!app->item_amp_mode) goto fail;
    variable_item_set_current_value_index(app->item_amp_mode, amp_mode_class_d ? 1 : 0);
    variable_item_set_current_value_text(app->item_amp_mode, amp_mode_class_d ? "D" : "AB");

    view_set_previous_callback(
        variable_item_list_get_view(app->variable_item_list_config),
        fred_fm_navigation_submenu_callback);
    view_dispatcher_add_view(
        app->view_dispatcher,
        FredFmViewConfigure,
        variable_item_list_get_view(app->variable_item_list_config));

    // Initialize the Listen view
    app->listen_view = view_alloc();
    if(!app->listen_view) goto fail;
    view_set_draw_callback(app->listen_view, fred_fm_view_draw_callback);
    view_set_input_callback(app->listen_view, fred_fm_view_input_callback);
    view_set_previous_callback(app->listen_view, fred_fm_navigation_submenu_callback);
    view_set_context(app->listen_view, app);
    view_allocate_model(app->listen_view, ViewModelTypeLockFree, sizeof(MyModel));

    view_dispatcher_add_view(app->view_dispatcher, FredFmViewListen, app->listen_view);

#ifdef ENABLE_RDS
    app->constellation_view = view_alloc();
    if(!app->constellation_view) goto fail;
    view_set_draw_callback(app->constellation_view, fred_fm_constellation_view_draw_callback);
    view_set_input_callback(app->constellation_view, fred_fm_constellation_view_input_callback);
    view_set_previous_callback(app->constellation_view, fred_fm_navigation_submenu_callback);
    view_set_context(app->constellation_view, app);
    view_allocate_model(app->constellation_view, ViewModelTypeLockFree, sizeof(MyModel));
    view_dispatcher_add_view(
        app->view_dispatcher, FredFmViewConstellation, app->constellation_view);
#endif

    // Initialize the widget for displaying information about the app
    app->widget_about = widget_alloc();
    if(!app->widget_about) goto fail;
    widget_add_text_scroll_element(
        app->widget_about,
        0,
        0,
        128,
        64,
        "FReD FM\nVersion: " FRED_FM_UI_VERSION
        "\n---\nFlipper Radio Experimental Decoder\nFM + RDS Radio Board\nby pchmielewski1\n\n"
        "Left/Right (short) = Tune -/+ 0.1MHz\n"
        "Left/Right (hold) = Seek next/prev\n"
        "OK (short) = Mute audio\n"
        "OK (hold) = Save preset\n"
        "Up/Down (short) = Preset next/prev\n"
        "Up/Down (hold) = Volume\n\n"
        "Constellation view:\n"
        "Left/Right = Carrier offset -/+ 0.01Hz\n"
        "Up/Down = Preset next/prev\n"
        "OK = Save preset + offset\n"
        "OK (hold) = Dump RAW/meta\n\n"
        "Band: 76.0-108.0MHz\n\n"
#ifdef ENABLE_RDS
        "Config: SNC / De-emph / HighCut / Mono / Backlight / RDS / RDS Debug / Volume / PT / Amp\n"
#else
        "Config: SNC / De-emph / HighCut / Mono / Backlight / Volume / PT / Amp\n"
#endif
        "RDS offset is manual and saved per preset");
    view_set_previous_callback(
        widget_get_view(app->widget_about), fred_fm_navigation_submenu_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, FredFmViewAbout, widget_get_view(app->widget_about));

    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    if(!app->notifications) goto fail;

    if(!fred_fm_submenu_rebuild(app)) goto fail;

#ifdef ENABLE_RDS
    fred_fm_rds_pipeline_start();
#endif

    // Apply backlight policy after loading settings
    fred_fm_apply_backlight(app->notifications);

    // Refresh config UI based on loaded settings
    if(app->item_snc) {
        variable_item_set_current_value_index(app->item_snc, tea_snc_enabled ? 1 : 0);
        variable_item_set_current_value_text(app->item_snc, tea_snc_enabled ? "On" : "Off");
    }
    if(app->item_volume) {
        variable_item_set_current_value_index(app->item_volume, current_volume ? 1 : 0);
        variable_item_set_current_value_text(
            app->item_volume, current_volume ? "Muted" : "Un-Muted");
    }
    if(app->item_pt_chip) {
        uint8_t chip_index = 0;
        for(uint8_t i = 0; i < COUNT_OF(pt_chip_values); i++) {
            if(pt_chip_values[i] == pt_chip) {
                chip_index = i;
                break;
            }
        }
        variable_item_set_current_value_index(app->item_pt_chip, chip_index);
        variable_item_set_current_value_text(app->item_pt_chip, pt_chip_names[chip_index]);
    }
    if(app->item_amp_power) {
        variable_item_set_current_value_index(app->item_amp_power, amp_power_enabled ? 1 : 0);
        variable_item_set_current_value_text(
            app->item_amp_power, amp_power_enabled ? "On" : "Off");
    }
    if(app->item_amp_mode) {
        variable_item_set_current_value_index(app->item_amp_mode, amp_mode_class_d ? 1 : 0);
        variable_item_set_current_value_text(app->item_amp_mode, amp_mode_class_d ? "D" : "AB");
    }
    if(app->item_deemph) {
        variable_item_set_current_value_index(app->item_deemph, tea_deemph_75us ? 1 : 0);
        variable_item_set_current_value_text(app->item_deemph, tea_deemph_75us ? "75us" : "50us");
    }
    if(app->item_highcut) {
        variable_item_set_current_value_index(app->item_highcut, tea_highcut_enabled ? 1 : 0);
        variable_item_set_current_value_text(
            app->item_highcut, tea_highcut_enabled ? "On" : "Off");
    }
    if(app->item_mono) {
        variable_item_set_current_value_index(app->item_mono, tea_force_mono_enabled ? 1 : 0);
        variable_item_set_current_value_text(
            app->item_mono, tea_force_mono_enabled ? "On" : "Off");
    }
    if(app->item_backlight) {
        variable_item_set_current_value_index(app->item_backlight, backlight_keep_on ? 1 : 0);
        variable_item_set_current_value_text(
            app->item_backlight, backlight_keep_on ? "On" : "Off");
    }
#ifdef ENABLE_RDS
    if(app->item_rds) {
        variable_item_set_current_value_index(app->item_rds, rds_enabled ? 1 : 0);
        variable_item_set_current_value_text(app->item_rds, rds_enabled ? "On" : "Off");
    }
    if(app->item_rds_debug) {
        variable_item_set_current_value_index(app->item_rds_debug, rds_debug_enabled ? 1 : 0);
        variable_item_set_current_value_text(
            app->item_rds_debug, rds_debug_enabled ? "On" : "Off");
    }
#endif

    // Give PT controllers time to settle after power-on before touching I2C.
    furi_delay_ms(200);
    (void)fred_fm_pt_refresh_state(true);
    fred_fm_apply_audio_output_state();

    // Start periodic background tick (I2C hot-plug, debounced saves)
    app->tick_timer = furi_timer_alloc(fred_fm_tick_callback, FuriTimerTypePeriodic, app);
    if(!app->tick_timer) goto fail;
    furi_timer_start(app->tick_timer, furi_ms_to_ticks(100));

#ifdef ENABLE_RDS
    fred_fm_rds_dsp_worker_start();
    app->rds_adc_timer =
        furi_timer_alloc(fred_fm_rds_adc_timer_callback, FuriTimerTypePeriodic, app);
    if(!app->rds_adc_timer) goto fail;
    rds_adc_timer_handle = app->rds_adc_timer;
    if(fred_fm_rds_pipeline_enabled()) {
        fred_fm_rds_apply_runtime_state(true);
    }
#endif

    return app;

fail:
    fred_fm_audio_shutdown();

    if(app) {
        if(app->tick_timer) {
            furi_timer_stop(app->tick_timer);
            furi_timer_free(app->tick_timer);
            app->tick_timer = NULL;
        }
#ifdef ENABLE_RDS
        if(app->rds_adc_timer) {
            furi_timer_stop(app->rds_adc_timer);
            furi_timer_free(app->rds_adc_timer);
            app->rds_adc_timer = NULL;
        }
        rds_adc_timer_running = false;
        rds_adc_timer_handle = NULL;
        fred_fm_rds_capture_stop();
        fred_fm_rds_capture_writer_stop();
        fred_fm_rds_adc_stop();
#endif
        if(app->notifications) {
            notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
            furi_record_close(RECORD_NOTIFICATION);
            app->notifications = NULL;
        }
        if(app->widget_about) {
            if(app->view_dispatcher)
                view_dispatcher_remove_view(app->view_dispatcher, FredFmViewAbout);
            widget_free(app->widget_about);
            app->widget_about = NULL;
        }
        if(app->listen_view) {
            if(app->view_dispatcher)
                view_dispatcher_remove_view(app->view_dispatcher, FredFmViewListen);
            view_free(app->listen_view);
            app->listen_view = NULL;
        }
#ifdef ENABLE_RDS
        if(app->constellation_view) {
            if(app->view_dispatcher)
                view_dispatcher_remove_view(app->view_dispatcher, FredFmViewConstellation);
            view_free(app->constellation_view);
            app->constellation_view = NULL;
        }
#endif
        if(app->variable_item_list_config) {
            if(app->view_dispatcher)
                view_dispatcher_remove_view(app->view_dispatcher, FredFmViewConfigure);
            variable_item_list_free(app->variable_item_list_config);
            app->variable_item_list_config = NULL;
        }
        if(app->submenu) {
            if(app->view_dispatcher)
                view_dispatcher_remove_view(app->view_dispatcher, FredFmViewSubmenu);
            submenu_free(app->submenu);
            app->submenu = NULL;
        }
        if(app->view_dispatcher) {
            view_dispatcher_free(app->view_dispatcher);
            app->view_dispatcher = NULL;
        }
    }

    if(state_mutex) {
        furi_mutex_free(state_mutex);
        state_mutex = NULL;
    }

    if(gui_opened) {
        furi_record_close(RECORD_GUI);
    }
    free(app);
    return NULL;
}

/* SINGLE sequential application teardown.
 *
 * This is the ONLY shutdown path. Called by fred_fm_app() after
 * view_dispatcher_run() returns (which happens once
 * fred_fm_navigation_exit_callback() returns VIEW_NONE). Runs on
 * the app's main thread, same thread that owned the dispatcher loop.
 *
 * Phases are ordered so that for each pair (producer, consumer) the producer
 * is fully stopped before its consumer is touched, and for each pair
 * (notifier, listener) the notifier is fully freed (synchronously draining
 * any in-flight callback) before the listener is freed.
 *
 *   1. Set exit flag (idempotent — exit_callback already did it)
 *   2. Stop+FREE every timer (rds_adc_timer, tick_timer). furi_timer_free is
 *      synchronous: after it returns, no callback can fire that would touch
 *      thread IDs or app state. This kills the use-after-free race that was
 *      causing null derefs at exit.
 *   3. Stop ADC/DMA + signal capture writer abort (kills sample producers).
 *   4. Stop+join+free worker threads (DSP worker, capture writer).
 *   5. Hardware shutdown (PT22xx mute, PAM off) — pure I2C/GPIO, safe now.
 *   6. Restore system state (backlight) and close NOTIFICATION record.
 *   7. Free GUI resources (views, dispatcher, GUI record).
 *   8. Free synchronisation primitives (state_mutex).
 *   9. Free the app struct.
 */
void fred_fm_free(FredFm* app) {
    if(!app) return;

    FURI_LOG_I(TAG, "free: enter (unified shutdown)");

    /* Phase 1: signal exit (defensive — already true if exit_callback ran). */
    fred_fm_app_exiting = true;

    /* Phase 2: stop timers but DO NOT free them.
     *
     * furi_timer_free() is asynchronous in FreeRTOS timer service: it queues
     * a delete command, and a pending callback (already queued before the
     * stop) can fire after free(app) and deref the dangling app pointer.
     *
     * We only stop the timers here. The Flipper/FreeRTOS timer service
     * automatically reclaims a deleted task's timers when the task
     * (the app's main thread) exits. Because fred_fm_app_exiting=true
     * is already set, any late callback will early-return immediately.
     *
     * This kills the use-after-free race that caused the null deref / HardFault. */
#ifdef ENABLE_RDS
    if(app->rds_adc_timer) {
        furi_timer_stop(app->rds_adc_timer);
        // DO NOT furi_timer_free here — see comment above
        app->rds_adc_timer = NULL;
    }
    rds_adc_timer_handle = NULL;
    rds_adc_timer_running = false;
#endif

    if(app->tick_timer) {
        furi_timer_stop(app->tick_timer);
        // DO NOT furi_timer_free here — see comment above
        app->tick_timer = NULL;
    }
    FURI_LOG_I(TAG, "free: timers stopped (not freed)");

    /* Wait for any in-flight timer callbacks to finish.
     *
     * furi_timer_stop() is asynchronous in FreeRTOS — a callback already
     * queued in the timer service can still fire after stop() returns. We
     * wait long enough to drain the longest timer period (tick=100ms) plus
     * a safety margin. fred_fm_app_exiting=true makes the callbacks
     * early-return, but a callback that started BEFORE the flag was set
     * can still be mid-execution holding state_mutex or I2C bus. */
    furi_delay_ms(200);
    FURI_LOG_I(TAG, "free: timer callback drain complete");

    /* Extra drain to ensure no timer callback is mid-execution.
     * Covers the case where a callback was queued just before the 200ms
     * drain ended and is still holding state_mutex. */
    furi_delay_ms(50);
    FURI_LOG_I(TAG, "free: extra mutex safety drain");

#ifdef ENABLE_RDS
    /* Phase 3: stop and join the DSP worker, then the capture writer.
     *
     * Worker exits before any pipeline resources are touched — no race between
     * worker and capture writer on rds_capture_ring. Capture writer is the sole
     * owner of ring free/close and runs to completion with ring still valid.
     * No new TICK flags can arrive (the timer was stopped in Phase 2). */
    if(rds_dsp_worker_thread) {
        FuriThreadId dsp_wid = rds_dsp_worker_thread_id;
        rds_dsp_worker_thread_id = NULL;
        if(dsp_wid) {
            furi_thread_flags_set(dsp_wid, RDS_DSP_WORKER_FLAG_STOP);
        }
        furi_thread_join(rds_dsp_worker_thread);
        furi_thread_free(rds_dsp_worker_thread);
        rds_dsp_worker_thread = NULL;
    }

    fred_fm_rds_capture_writer_stop();
    FURI_LOG_I(TAG, "free: DSP worker + capture writer stopped");

    /* Phase 4: stop the ADC/DMA producer.
     *
     * Pipeline barrier is now defense-in-depth — the DSP worker already exited
     * in Phase 3. The 20 ms drain covers any remaining ISR callbacks that may
     * have been in-flight when rds_pipeline_stopping was set. adc_stop()
     * unregisters the DMA ISR, stops the trigger timer, releases the ADC
     * handle: after it returns no context can touch shared RDS state. */
    rds_pipeline_stopping = true;
    furi_delay_ms(20);
    fred_fm_rds_capture_stop();
    fred_fm_rds_adc_stop();
    FURI_LOG_I(TAG, "free: RDS pipeline stopped");
#endif

    /* Phase 5: hardware shutdown. Pure I2C/GPIO from the app main thread —
     * no concurrent worker can touch PT22xx/PAM at this point. */
    FURI_LOG_I(TAG, "free: entering audio shutdown");
    fred_fm_audio_shutdown();
    FURI_LOG_I(TAG, "free: audio shutdown done");

    /* Phase 6: restore system state and close the NOTIFICATION record.
     * Defensive: check app and notifications pointer validity. */
    if(app && app->notifications) {
        FURI_LOG_I(TAG, "free: restoring backlight");
        notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
        furi_record_close(RECORD_NOTIFICATION);
        app->notifications = NULL;
        FURI_LOG_I(TAG, "free: notification closed");
    }

    /* Phase 7: free GUI resources. The dispatcher event loop is already
     * stopped (VIEW_NONE was returned from exit_callback), but views must
     * still be removed from the dispatcher's view table before being freed
     * so its internal bookkeeping stays consistent. */
    FURI_LOG_I(TAG, "free: starting GUI teardown");
    if(app && app->widget_about) {
        FURI_LOG_I(TAG, "free: removing About view");
        if(app->view_dispatcher)
            view_dispatcher_remove_view(app->view_dispatcher, FredFmViewAbout);
        widget_free(app->widget_about);
        app->widget_about = NULL;
        FURI_LOG_I(TAG, "free: About view freed");
    }
    if(app && app->listen_view) {
        FURI_LOG_I(TAG, "free: removing Listen view");
        if(app->view_dispatcher)
            view_dispatcher_remove_view(app->view_dispatcher, FredFmViewListen);
        view_free(app->listen_view);
        app->listen_view = NULL;
        FURI_LOG_I(TAG, "free: Listen view freed");
    }
#ifdef ENABLE_RDS
    if(app && app->constellation_view) {
        FURI_LOG_I(TAG, "free: removing Constellation view");
        if(app->view_dispatcher)
            view_dispatcher_remove_view(app->view_dispatcher, FredFmViewConstellation);
        view_free(app->constellation_view);
        app->constellation_view = NULL;
        FURI_LOG_I(TAG, "free: Constellation view freed");
    }
#endif
    if(app && app->variable_item_list_config) {
        FURI_LOG_I(TAG, "free: removing Config view");
        if(app->view_dispatcher)
            view_dispatcher_remove_view(app->view_dispatcher, FredFmViewConfigure);
        variable_item_list_free(app->variable_item_list_config);
        app->variable_item_list_config = NULL;
        FURI_LOG_I(TAG, "free: Config view freed");
    }
    if(app && app->submenu) {
        FURI_LOG_I(TAG, "free: removing Submenu view");
        if(app->view_dispatcher)
            view_dispatcher_remove_view(app->view_dispatcher, FredFmViewSubmenu);
        submenu_free(app->submenu);
        app->submenu = NULL;
        FURI_LOG_I(TAG, "free: Submenu freed");
    }
    if(app && app->view_dispatcher) {
        FURI_LOG_I(TAG, "free: freeing view dispatcher");
        view_dispatcher_free(app->view_dispatcher);
        app->view_dispatcher = NULL;
        FURI_LOG_I(TAG, "free: view dispatcher freed");
    }
    if(app) {
        furi_record_close(RECORD_GUI);
        FURI_LOG_I(TAG, "free: GUI record closed");
    }

    /* Phase 8: skipped — let Flipper SDK reclaim mutex at thread exit.
     *
     * furi_mutex_free() is async in FreeRTOS — a pending timer callback
     * may still hold the mutex. Freeing it causes NULL pointer dereference
     * when the callback tries to release a freed mutex.
     *
     * Same pattern as timers (Phase 2): stop + drain + SDK reclaim at exit. */

    /* Phase 9: free the app struct. */
    free(app);
    FURI_LOG_I(TAG, "free: done");
}

// Main function to start the application
int32_t fred_fm_app(void* p) {
    UNUSED(p);

    FredFm* app = fred_fm_alloc();
    if(!app) {
        FURI_LOG_E(TAG, "App allocation failed");
        return -1;
    }
    view_dispatcher_run(app->view_dispatcher);

    fred_fm_free(app);
    return 0;
}
