/**
 * @file core.h
 * @brief Shared app state, tuning, presets, settings, and seek logic.
 */
#pragma once

#include "src/fred_fm/include/config.h"
#include "src/fred_fm/include/types.h"
#include <furi.h>
#include <storage/storage.h>
#include <notification/notification.h>
#include "src/drivers/tea5767/TEA5767.h"
#include "src/drivers/pt/PT22xx.h"

extern volatile bool fred_fm_app_exiting;
extern volatile bool rds_pipeline_stopping;
extern FuriMutex* state_mutex;

extern const uint8_t volume_values[2];
extern const char* volume_names[2];
extern bool current_volume;
extern const PT22xxChip pt_chip_values[2];
extern const char* pt_chip_names[2];
extern PT22xxChip pt_chip;
extern const char* amp_power_names[2];
extern const char* amp_mode_names[2];
extern bool amp_power_enabled;
extern bool amp_mode_class_d;
extern const uint8_t pt_i2c_addr8;
extern uint8_t pt_atten_db;
extern bool pt_ready_cached;
extern bool pt_initialized_cached;

extern bool settings_dirty;
extern bool tea_snc_enabled;
extern bool tea_deemph_75us;
extern bool tea_highcut_enabled;
extern bool tea_force_mono_enabled;
extern bool backlight_keep_on;

extern uint32_t preset_freq_10khz[PRESETS_MAX];
extern int16_t preset_carrier_offset_centihz[PRESETS_MAX];
extern uint8_t preset_count;
extern uint8_t preset_index;
extern bool presets_dirty;

extern uint32_t seek_last_step_tick;
extern uint32_t tea_nominal_freq_10khz;
extern uint32_t tea_last_tune_tick;
extern struct RADIO_INFO tea_info_cached;
extern bool tea_info_valid;
extern bool tea_i2c_ready;
extern uint32_t tea_info_read_count;

/** Clamp @p value to [@p min, @p max]. */
uint32_t clamp_u32(uint32_t value, uint32_t min, uint32_t max);

/** Acquire global state mutex. */
void fred_fm_state_lock(void);

/** Release global state mutex. */
void fred_fm_state_unlock(void);

/** Current nominal tune frequency in 10 kHz units. */
uint32_t fred_fm_get_current_freq_10khz(void);

/** Tune TEA5767 to @p freq_10khz and update nominal state. */
void fred_fm_tune_nominal_freq_10khz(uint32_t freq_10khz);

/** Read hardware frequency and sync nominal tracker. */
void fred_fm_sync_nominal_frequency_from_hardware(void);

/** Mark settings dirty for deferred SD save. */
void fred_fm_settings_mark_dirty(void);

/** Mark preset list dirty for deferred SD save. */
void fred_fm_presets_mark_dirty(void);

/** Round preset frequency to valid 10 kHz grid. */
uint32_t fred_fm_normalize_preset_freq_10khz(uint32_t freq_10khz);

/** Clamp manual RDS offset to configured centihertz range. */
int16_t fred_fm_clamp_manual_offset_centihz(int16_t offset_centihz);

/** Encode offset as preset bias600 storage value. */
uint32_t fred_fm_preset_offset_to_bias600(int16_t offset_centihz);

/** Decode preset bias600 storage value to centihertz offset. */
int16_t fred_fm_preset_offset_from_bias600(uint32_t bias600);

/** Find preset index for @p freq_10khz; false if not stored. */
bool fred_fm_preset_find(uint32_t freq_10khz, uint8_t* found_index);

/** Ensure app data directory exists on SD. */
void fred_fm_ensure_app_data_dir(Storage* storage);

/** Load presets from SD; fall back to defaults on error. */
void fred_fm_presets_load(void);

/** Save presets to SD when dirty. */
void fred_fm_presets_save(void);

/** Short success haptic/LED feedback. */
void fred_fm_feedback_success(void);

/** Add or select preset for frequency and RDS offset. */
void fred_fm_presets_add_or_select(uint32_t freq_10khz, int16_t offset_centihz);

/** Step preset list forward/back and apply tuning. */
bool fred_fm_presets_step_and_apply(bool forward);

/** Seek one step up or down with rate limiting. */
void fred_fm_seek_step(bool direction_up);

/** Load settings from SD; apply hardware options. */
void fred_fm_settings_load(void);

/** Save settings to SD when dirty. */
void fred_fm_settings_save(void);

/** Apply keep-backlight-on notification sequence. */
void fred_fm_apply_backlight(NotificationApp* notifications);
