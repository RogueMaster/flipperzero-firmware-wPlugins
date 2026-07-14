/**
 * @file core.c
 * @brief Shared app state, presets, settings, and tuner/seek helpers.
 */
#include "src/fred_fm/include/config.h"
#include "src/fred_fm/include/types.h"
#include "src/fred_fm/core/core.h"

#include <furi.h>
#include <furi_hal.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <flipper_format/flipper_format.h>
#include <string.h>
#include <stdlib.h>
#include "src/drivers/tea5767/TEA5767.h"
#ifdef ENABLE_RDS
#include "src/fred_fm/rds/rds.h"
#include <furi_hal_gpio.h>
#endif

const uint8_t volume_values[] = {0, 1};
const char* volume_names[] = {"Un-Muted", "Muted"};
bool current_volume = false;

const PT22xxChip pt_chip_values[] = {PT22xxChipPT2257, PT22xxChipPT2259};
const char* pt_chip_names[] = {"PT2257", "PT2259-S"};
PT22xxChip pt_chip = PT22xxChipPT2257;

const char* amp_power_names[] = {"Off", "On"};
const char* amp_mode_names[] = {"AB", "D"};
bool amp_power_enabled = true;
bool amp_mode_class_d = false;
FuriMutex* state_mutex = NULL;

typedef struct FredFm FredFm;

const uint8_t pt_i2c_addr8 = 0x88; /* fixed PT I2C address on FReD FM board */
uint8_t pt_atten_db = 20; /* 0 = loudest, 79 = quietest */

/* Set during exit/teardown so timers, workers, and ISRs bail out early. */
volatile bool fred_fm_app_exiting = false;
volatile bool rds_pipeline_stopping = false;

bool pt_ready_cached = false;
bool pt_initialized_cached = false;

bool settings_dirty = false;

bool tea_snc_enabled = false;
bool tea_deemph_75us = false;
bool tea_highcut_enabled = false;
bool tea_force_mono_enabled = false;
#ifdef ENABLE_RDS
bool rds_enabled = true;
bool rds_debug_enabled = false;
#endif

bool backlight_keep_on = false;

uint32_t preset_freq_10khz[PRESETS_MAX];
int16_t preset_carrier_offset_centihz[PRESETS_MAX];
uint8_t preset_count = 0;
uint8_t preset_index = 0;
bool presets_dirty = false;

uint32_t seek_last_step_tick = 0;
uint32_t tea_nominal_freq_10khz = 8750U;
uint32_t tea_last_tune_tick = 0U;

#define TEA_IF_COUNT_TARGET 0x37U

/* Last TEA5767 status snapshot (Listen view draw reads this). */
struct RADIO_INFO tea_info_cached;
bool tea_info_valid = false;
bool tea_i2c_ready = false;
uint32_t tea_info_read_count = 0;

#ifdef ENABLE_RDS
RDSCore rds_core;
RDSDsp rds_dsp;
RdsAcquisition rds_acquisition;
int16_t rds_carrier_offset_centihz = 0;
int32_t rds_constellation_i_history[RDS_CONSTELLATION_HISTORY_LEN];
int32_t rds_constellation_q_history[RDS_CONSTELLATION_HISTORY_LEN];
int32_t rds_constellation_i_snapshot[RDS_CONSTELLATION_HISTORY_LEN];
int32_t rds_constellation_q_snapshot[RDS_CONSTELLATION_HISTORY_LEN];
uint8_t rds_constellation_history_count = 0U;
uint8_t rds_constellation_history_write_index = 0U;
uint32_t rds_constellation_saved_until_tick = 0U;
bool rds_constellation_view_active = false;
uint32_t rds_runtime_sample_rate_hz = 0U;
char rds_ps_display[RDS_PS_LEN + 1U];
RdsSyncState rds_sync_display = RdsSyncStateSearch;
#define RDS_ADC_FIXED_MIDPOINT 2048U
const GpioPin* rds_adc_pin = &gpio_ext_pa4;
FuriHalAdcChannel rds_adc_channel = FuriHalAdcChannel9;
FuriTimer* rds_adc_timer_handle = NULL;
bool rds_adc_timer_running = false;
#define RDS_DSP_WORKER_STACK_SIZE 4096U
#define RDS_DSP_WORKER_FLAG_TICK  0x01U
#define RDS_DSP_WORKER_FLAG_STOP  0x02U
FuriThread* rds_dsp_worker_thread = NULL;
volatile FuriThreadId rds_dsp_worker_thread_id = NULL;
uint8_t tea_i2c_failure_count = 0U;

#endif

/** Clamp @p value to [@p min, @p max]. */
uint32_t clamp_u32(uint32_t value, uint32_t min, uint32_t max) {
    if(value < min) return min;
    if(value > max) return max;
    return value;
}

void fred_fm_state_lock(void) {
    if(state_mutex) {
        (void)furi_mutex_acquire(state_mutex, FuriWaitForever);
    }
}

void fred_fm_state_unlock(void) {
    if(state_mutex) {
        (void)furi_mutex_release(state_mutex);
    }
}

/** Nominal tuned frequency in 10 kHz units (e.g. 8750 = 87.50 MHz). */
uint32_t fred_fm_get_current_freq_10khz(void) {
    uint32_t freq_10khz = tea_nominal_freq_10khz;

    if(freq_10khz < 7600U || freq_10khz > 10800U) {
        freq_10khz = 8750U;
    }

    return clamp_u32(freq_10khz, 7600U, 10800U);
}

/** Tune radio and update cached nominal frequency. */
void fred_fm_tune_nominal_freq_10khz(uint32_t freq_10khz) {
    uint32_t now = furi_get_tick();

    freq_10khz = clamp_u32(freq_10khz, 7600U, 10800U);

    fred_fm_state_lock();
    tea_nominal_freq_10khz = freq_10khz;
    tea_last_tune_tick = now;
    fred_fm_state_unlock();

    tea5767_SetFreqMHz(((float)freq_10khz) / 100.0f);
}

void fred_fm_sync_nominal_frequency_from_hardware(void) {
    float freq = tea5767_GetFreq();
    if(freq <= 0.0f) {
        return;
    }

    fred_fm_state_lock();
    tea_nominal_freq_10khz = clamp_u32((uint32_t)(freq * 100.0f), 7600U, 10800U);
    tea_last_tune_tick = furi_get_tick();
    fred_fm_state_unlock();
}

void fred_fm_settings_mark_dirty(void) {
    settings_dirty = true;
}

void fred_fm_presets_mark_dirty(void) {
    presets_dirty = true;
}

uint32_t fred_fm_normalize_preset_freq_10khz(uint32_t freq_10khz) {
    freq_10khz = clamp_u32(freq_10khz, 7600U, 10800U);
    return clamp_u32(((freq_10khz + 5U) / 10U) * 10U, 7600U, 10800U);
}

int16_t fred_fm_clamp_manual_offset_centihz(int16_t offset_centihz) {
    if(offset_centihz < RDS_MANUAL_OFFSET_MIN_CENTIHZ) {
        return RDS_MANUAL_OFFSET_MIN_CENTIHZ;
    }
    if(offset_centihz > RDS_MANUAL_OFFSET_MAX_CENTIHZ) {
        return RDS_MANUAL_OFFSET_MAX_CENTIHZ;
    }
    return offset_centihz;
}

uint32_t fred_fm_preset_offset_to_bias600(int16_t offset_centihz) {
    int16_t clamped = fred_fm_clamp_manual_offset_centihz(offset_centihz);
    return (uint32_t)(clamped + (int16_t)PRESET_OFFSET_CENTIHZ_BIAS_CENTER);
}

int16_t fred_fm_preset_offset_from_bias600(uint32_t bias600) {
    uint32_t clamped = clamp_u32(bias600, PRESET_OFFSET_BIAS_MIN, PRESET_OFFSET_CENTIHZ_BIAS_MAX);
    return (int16_t)((int32_t)clamped - (int32_t)PRESET_OFFSET_CENTIHZ_BIAS_CENTER);
}

bool fred_fm_preset_find(uint32_t freq_10khz, uint8_t* found_index) {
    freq_10khz = fred_fm_normalize_preset_freq_10khz(freq_10khz);

    for(uint8_t i = 0; i < preset_count; i++) {
        if(preset_freq_10khz[i] == freq_10khz) {
            if(found_index) *found_index = i;
            return true;
        }
    }
    return false;
}

void fred_fm_ensure_app_data_dir(Storage* storage) {
    if(!storage) return;
    FuriString* path = furi_string_alloc_set(APP_DATA_PATH("."));
    storage_common_resolve_path_and_ensure_app_directory(storage, path);
    furi_string_free(path);
}

/** Load presets from SD (APP_DATA_PATH/presets.fff). */
void fred_fm_presets_load(void) {
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

/** Persist presets when @c presets_dirty is set. */
void fred_fm_presets_save(void) {
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

void fred_fm_feedback_success(void) {
    NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);
    notification_message(notifications, &sequence_success);
    furi_record_close(RECORD_NOTIFICATION);
}

void fred_fm_presets_add_or_select(uint32_t freq_10khz, int16_t offset_centihz) {
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

bool fred_fm_presets_step_and_apply(bool forward) {
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

/** SEEK up/down with debounce and TEA5767 READY polling. */
void fred_fm_seek_step(bool direction_up) {
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

/** Load settings from SD (APP_DATA_PATH/settings.fff). */
void fred_fm_settings_load(void) {
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

/** Write settings.fff when @c settings_dirty is set. */
void fred_fm_settings_save(void) {
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

void fred_fm_apply_backlight(NotificationApp* notifications) {
    if(!notifications) return;
    if(backlight_keep_on) {
        notification_message(notifications, &sequence_display_backlight_enforce_on);
    } else {
        notification_message(notifications, &sequence_display_backlight_enforce_auto);
    }
}
