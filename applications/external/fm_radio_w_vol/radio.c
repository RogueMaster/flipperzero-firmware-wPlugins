/**
 *
 * @author Coolshrimp - CoolshrimpModz.com
 *
 * @brief FM Radio using the TEA5767 FM radio chip.
 * @version 0.10+pt22xx.0
 * @date 2023-09-29
 * 
 * @copyright GPLv3
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

// Set to 1 to enable RDS decoder (uses ADC on PA4, extra ~3 kB flash).
// Set to 0 to disable RDS completely (UI items hidden, no ADC).
#define ENABLE_RDS 1

// Set to 1 to enable raw ADC capture to SD card (long-press OK).
// Useful for offline spectrum analysis; disable for normal builds.
#define ENABLE_ADC_CAPTURE 0

#ifdef ENABLE_RDS
#include "RDS/RDSCore.h"
#include "RDS/RDSDsp.h"
#include "RDS/RDSAcquisition.h"
#endif

// Define a macro for enabling the backlight always on.
// NOTE: For some setups this can inject audible PWM/DC-DC noise into the audio path.
// Uncomment only if you really want forced backlight.
// #define BACKLIGHT_ALWAYS_ON

#define TAG                "FMRadio"
#define FMRADIO_UI_VERSION "0.10+pt22xx.dev"

// Volume config options (used by Config menu)
static const uint8_t volume_values[] = {0, 1};
static const char* volume_names[] = {"Un-Muted", "Muted"};
static bool current_volume = false;

static const PT22xxChip pt_chip_values[] = {PT22xxChipPT2257, PT22xxChipPT2259};
static const char* pt_chip_names[] = {"PT2257", "PT2259-S"};
static PT22xxChip pt_chip = PT22xxChipPT2257;

// Dedicated boards use a fixed 8-bit PT I2C address byte.
static const uint8_t pt_i2c_addr8 = 0x88;

// PT attenuation in dB: 0..79 (0 => max volume, 79 => min volume)
static uint8_t pt_atten_db = 20;
static bool pt_ready_cached = false;
static bool pt_initialized_cached = false;

static void fmradio_state_lock(void);
static void fmradio_state_unlock(void);
static uint32_t fmradio_get_current_freq_10khz(void);
#ifdef ENABLE_RDS
static void fmradio_rds_on_tuned_frequency_changed(void);
void fmradio_rds_process_adc_block(const uint16_t* samples, size_t count, uint16_t adc_midpoint);
static void fmradio_rds_acquisition_block_callback(
    const uint16_t* samples,
    size_t count,
    uint16_t adc_midpoint,
    void* context);
static bool fmradio_rds_adc_start(void);
static void fmradio_rds_adc_stop(void);
static void fmradio_rds_adc_timer_callback(void* context);
static void fmradio_rds_update_ui_snapshot(void);
static const char* fmradio_rds_sync_short_text(RdsSyncState state);
static void fmradio_rds_metadata_reset(void);
static void fmradio_rds_metadata_save(void);
#else
static inline void fmradio_rds_on_tuned_frequency_changed(void) {
}
#endif

static void fmradio_pt_apply_config(void) {
    pt22xx_set_chip(pt_chip);
    pt22xx_set_i2c_addr(pt_i2c_addr8);
}

static bool fmradio_pt_refresh_state(bool force_init) {
    bool local_initialized;

    fmradio_state_lock();
    local_initialized = pt_initialized_cached;
    fmradio_state_unlock();

    fmradio_pt_apply_config();
    bool ready = pt22xx_is_device_ready();

    if(!ready) {
        local_initialized = false;
    } else if(force_init || !local_initialized) {
        local_initialized = pt22xx_init();
        if(!local_initialized) ready = false;
    }

    fmradio_state_lock();
    pt_ready_cached = ready;
    pt_initialized_cached = local_initialized;
    fmradio_state_unlock();

    return ready;
}

static const char* fmradio_pt_active_name(void) {
    return pt22xx_get_chip_name();
}

static void fmradio_apply_pt_state(void) {
    fmradio_state_lock();
    bool local_ready = pt_ready_cached;
    bool local_muted = current_volume;
    uint8_t local_atten_db = pt_atten_db;
    fmradio_state_unlock();

    if(!local_ready) return;

    PT22xxState state = {
        .attenuation_db = local_atten_db,
        .muted = local_muted,
    };

    (void)pt22xx_apply_state(&state);
}

#define SETTINGS_DIR      EXT_PATH("apps_data/fmradio_controller_pt2257")
#define SETTINGS_FILE     EXT_PATH("apps_data/fmradio_controller_pt2257/settings.fff")
#define SETTINGS_FILETYPE "FMRadio PT Settings"
#define SETTINGS_VERSION  (3U)
#ifdef ENABLE_RDS
#define RDS_RUNTIME_META_FILE EXT_PATH("apps_data/fmradio_controller_pt2257/rds_runtime_meta.txt")
#endif

#define PRESETS_FILE     EXT_PATH("apps_data/fmradio_controller_pt2257/presets.fff")
#define PRESETS_FILETYPE "FMRadio Presets"
#define PRESETS_VERSION  (1U)
#define PRESETS_MAX      (32U)

static bool settings_dirty = false;

static bool tea_snc_enabled = false;
static bool tea_deemph_75us = false;
static bool tea_softmute_enabled = true;
static bool tea_highcut_enabled = false;
static bool tea_force_mono_enabled = false;
#ifdef ENABLE_RDS
static bool rds_enabled = false;
#endif

static bool backlight_keep_on = false;

static uint32_t preset_freq_10khz[PRESETS_MAX];
static uint8_t preset_count = 0;
static uint8_t preset_index = 0;
static bool presets_dirty = false;

static uint32_t seek_last_step_tick = 0;

static FuriMutex* state_mutex = NULL;

/* Cached TEA5767 radio info — refreshed every tick, consumed by draw callback */
static struct RADIO_INFO tea_info_cached;
static bool tea_info_valid = false;
static uint32_t tea_info_read_count = 0;

#ifdef ENABLE_RDS
static RDSCore rds_core;
static RDSDsp rds_dsp;
static RdsAcquisition rds_acquisition;
static char rds_ps_display[RDS_PS_LEN + 1U];
static RdsSyncState rds_sync_display = RdsSyncStateSearch;
static uint32_t rds_ok_blocks_display = 0U;
#define RDS_ADC_FIXED_MIDPOINT 2072U
static const GpioPin* rds_adc_pin = &gpio_ext_pa4;
static FuriHalAdcChannel rds_adc_channel = FuriHalAdcChannel9;
static FuriTimer* rds_adc_timer_handle = NULL;
#endif

// Built-in frequency list for Config menu quick-jump
static const float frequency_values[] = {88.1,  88.9,  89.1,  90.3,  91.5,  91.7,  92.0,
                                         92.5,  94.1,  95.9,  96.3,  96.9,  97.3,  98.1,
                                         98.7,  99.1,  99.9,  100.7, 101.3, 102.7, 103.9,
                                         104.5, 105.1, 105.3, 105.5, 105.6, 106.5, 107.1};

static uint32_t current_frequency_index = 0; // Default to the first frequency

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

static void fmradio_state_lock(void) {
    if(state_mutex) {
        (void)furi_mutex_acquire(state_mutex, FuriWaitForever);
    }
}

static void fmradio_state_unlock(void) {
    if(state_mutex) {
        (void)furi_mutex_release(state_mutex);
    }
}

static uint32_t fmradio_get_current_freq_10khz(void) {
    float freq = tea5767_GetFreq();
    if(freq < 0.0f) {
        // fallback if TEA5767 is not available
        if(current_frequency_index < COUNT_OF(frequency_values)) {
            freq = frequency_values[current_frequency_index];
        } else {
            freq = 87.5f;
        }
    }

    // TEA5767 driver uses 10kHz units (MHz * 100)
    uint32_t freq_10khz = (uint32_t)(freq * 100.0f);
    return clamp_u32(freq_10khz, 7600U, 10800U);
}

static void fmradio_settings_mark_dirty(void) {
    settings_dirty = true;
}

static void fmradio_presets_mark_dirty(void) {
    presets_dirty = true;
}

static bool fmradio_preset_find(uint32_t freq_10khz, uint8_t* found_index) {
    for(uint8_t i = 0; i < preset_count; i++) {
        if(preset_freq_10khz[i] == freq_10khz) {
            if(found_index) *found_index = i;
            return true;
        }
    }
    return false;
}

static void fmradio_presets_load(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    FuriString* filetype = furi_string_alloc();
    uint32_t version = 0;

    preset_count = 0;
    preset_index = 0;

    do {
        if(!flipper_format_file_open_existing(ff, PRESETS_FILE)) break;
        if(!flipper_format_read_header(ff, filetype, &version)) break;
        if(version != PRESETS_VERSION) break;

        uint32_t count = 0;
        if(!flipper_format_read_uint32(ff, "Count", &count, 1)) break;
        if(count > PRESETS_MAX) count = PRESETS_MAX;

        if(count > 0) {
            if(!flipper_format_read_uint32(ff, "Freq10kHz", preset_freq_10khz, (uint16_t)count))
                break;
        }

        preset_count = (uint8_t)count;

        uint32_t idx = 0;
        if(flipper_format_read_uint32(ff, "Index", &idx, 1)) {
            if(preset_count > 0) preset_index = (uint8_t)clamp_u32(idx, 0, preset_count - 1);
        }
    } while(false);

    flipper_format_file_close(ff);
    flipper_format_free(ff);
    furi_string_free(filetype);
    furi_record_close(RECORD_STORAGE);
}

static void fmradio_presets_save(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, SETTINGS_DIR);
    FlipperFormat* ff = flipper_format_file_alloc(storage);

    bool ok = false;
    do {
        if(!flipper_format_file_open_always(ff, PRESETS_FILE)) break;
        if(!flipper_format_write_header_cstr(ff, PRESETS_FILETYPE, PRESETS_VERSION)) break;

        uint32_t count = preset_count;
        if(!flipper_format_write_uint32(ff, "Count", &count, 1)) break;
        if(preset_count > 0) {
            if(!flipper_format_write_uint32(ff, "Freq10kHz", preset_freq_10khz, preset_count))
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

static void fmradio_feedback_success(void) {
    NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);
    notification_message(notifications, &sequence_success);
    furi_record_close(RECORD_NOTIFICATION);
}

static void fmradio_presets_add_or_select(uint32_t freq_10khz) {
    freq_10khz = clamp_u32(freq_10khz, 7600U, 10800U);

    uint8_t found = 0;
    if(fmradio_preset_find(freq_10khz, &found)) {
        preset_index = found;
        return;
    }

    if(preset_count < PRESETS_MAX) {
        preset_index = preset_count;
        preset_freq_10khz[preset_count] = freq_10khz;
        preset_count++;
    } else {
        if(preset_index >= PRESETS_MAX) preset_index = 0;
        preset_freq_10khz[preset_index] = freq_10khz;
    }

    fmradio_presets_mark_dirty();
}

static void fmradio_seek_step(bool direction_up) {
    // SEEK-only behavior (scan logic disabled by request).
    // Debounce repeated long-hold events so seek does not run too fast.
    uint32_t now = furi_get_tick();
    if((now - seek_last_step_tick) < furi_ms_to_ticks(SEEK_MIN_INTERVAL_MS)) {
        return;
    }

    uint32_t cur = fmradio_get_current_freq_10khz();
    uint32_t next = direction_up ? clamp_u32(cur + 10U, 7600U, 10800U) :
                                   ((cur > 7610U) ? (cur - 10U) : 7600U);

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

    fmradio_rds_on_tuned_frequency_changed();
}

static void fmradio_settings_load(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    FuriString* filetype = furi_string_alloc();
    uint32_t version = 0;

    do {
        if(!flipper_format_file_open_existing(ff, SETTINGS_FILE)) break;
        if(!flipper_format_read_header(ff, filetype, &version)) break;
        if((version == 0U) || (version > SETTINGS_VERSION)) break;

        bool snc = false;
        if(flipper_format_read_bool(ff, "TeaSNC", &snc, 1)) {
            tea_snc_enabled = snc;
        }
        tea5767_set_snc_enabled(tea_snc_enabled);

        bool deemph_75 = false;
        if(flipper_format_read_bool(ff, "TeaDeemph75us", &deemph_75, 1)) {
            tea_deemph_75us = deemph_75;
        }
        tea5767_set_deemphasis_75us_enabled(tea_deemph_75us);

        bool softmute = true;
        if(flipper_format_read_bool(ff, "TeaSoftMute", &softmute, 1)) {
            tea_softmute_enabled = softmute;
        }
        tea5767_set_softmute_enabled(tea_softmute_enabled);

        bool highcut = false;
        if(flipper_format_read_bool(ff, "TeaHighCut", &highcut, 1)) {
            tea_highcut_enabled = highcut;
        }
        tea5767_set_high_cut_enabled(tea_highcut_enabled);

        bool mono = false;
        if(flipper_format_read_bool(ff, "TeaForceMono", &mono, 1)) {
            tea_force_mono_enabled = mono;
        }
        tea5767_set_force_mono_enabled(tea_force_mono_enabled);

        bool bl = false;
        if(flipper_format_read_bool(ff, "BacklightKeepOn", &bl, 1)) {
            backlight_keep_on = bl;
        }

        uint32_t freq_10khz = 0;
        if(flipper_format_read_uint32(ff, "Freq10kHz", &freq_10khz, 1)) {
            freq_10khz = clamp_u32(freq_10khz, 7600U, 10800U);
            tea5767_SetFreqMHz(((float)freq_10khz) / 100.0f);
        }

        uint32_t atten = 0;
        if(flipper_format_read_uint32(ff, "PtAttenDb", &atten, 1)) {
            if(atten > 79) atten = 79;
            pt_atten_db = (uint8_t)atten;
        }

        bool muted = false;
        if(flipper_format_read_bool(ff, "PtMuted", &muted, 1)) {
            current_volume = muted;
        }

        if(version >= 2U) {
            uint32_t chip_type = 0;
            if(flipper_format_read_uint32(ff, "PtChipType", &chip_type, 1)) {
                if(chip_type <= (uint32_t)PT22xxChipPT2259) {
                    pt_chip = (PT22xxChip)chip_type;
                }
            }
        }

        if(version >= 3U) {
#ifdef ENABLE_RDS
            bool rds = false;
            if(flipper_format_read_bool(ff, "RdsEnabled", &rds, 1)) {
                rds_enabled = rds;
            }
#endif
        }

    } while(false);

    flipper_format_file_close(ff);
    flipper_format_free(ff);
    furi_string_free(filetype);
    furi_record_close(RECORD_STORAGE);
}

static void fmradio_settings_save(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, SETTINGS_DIR);
    FlipperFormat* ff = flipper_format_file_alloc(storage);

    bool ok = false;
    do {
        if(!flipper_format_file_open_always(ff, SETTINGS_FILE)) break;
        if(!flipper_format_write_header_cstr(ff, SETTINGS_FILETYPE, SETTINGS_VERSION)) break;

        bool snc = tea_snc_enabled;
        if(!flipper_format_write_bool(ff, "TeaSNC", &snc, 1)) break;

        bool deemph_75 = tea_deemph_75us;
        if(!flipper_format_write_bool(ff, "TeaDeemph75us", &deemph_75, 1)) break;

        bool softmute = tea_softmute_enabled;
        if(!flipper_format_write_bool(ff, "TeaSoftMute", &softmute, 1)) break;

        bool highcut = tea_highcut_enabled;
        if(!flipper_format_write_bool(ff, "TeaHighCut", &highcut, 1)) break;

        bool mono = tea_force_mono_enabled;
        if(!flipper_format_write_bool(ff, "TeaForceMono", &mono, 1)) break;

        bool bl = backlight_keep_on;
        if(!flipper_format_write_bool(ff, "BacklightKeepOn", &bl, 1)) break;

#ifdef ENABLE_RDS
        bool rds = rds_enabled;
        if(!flipper_format_write_bool(ff, "RdsEnabled", &rds, 1)) break;
#endif

        uint32_t freq_10khz = fmradio_get_current_freq_10khz();
        if(!flipper_format_write_uint32(ff, "Freq10kHz", &freq_10khz, 1)) break;

        uint32_t atten = pt_atten_db;
        if(!flipper_format_write_uint32(ff, "PtAttenDb", &atten, 1)) break;

        bool muted = (current_volume != 0);
        if(!flipper_format_write_bool(ff, "PtMuted", &muted, 1)) break;

        uint32_t chip_type = (uint32_t)pt_chip;
        if(!flipper_format_write_uint32(ff, "PtChipType", &chip_type, 1)) break;

        ok = true;
    } while(false);

    flipper_format_file_close(ff);
    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);

    if(ok) settings_dirty = false;
}

static void fmradio_apply_backlight(NotificationApp* notifications) {
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
 * Records raw uint16_t ADC samples to SD for offline spectrum analysis.
 * Activated by long-press OK (works with RDS on or off).
 * Uses RAM buffer — callback does fast memcpy, SD write after capture. */
#define RDS_CAPTURE_FILE      EXT_PATH("apps_data/fmradio_controller_pt2257/rds_capture_u16le.raw")
#define RDS_CAPTURE_META_FILE EXT_PATH("apps_data/fmradio_controller_pt2257/rds_capture_meta.txt")

static volatile bool rds_capture_active = false;
static volatile bool rds_capture_requested = false;
static volatile bool rds_capture_flush_pending = false;
static uint16_t* rds_capture_buf = NULL;
static uint32_t rds_capture_buf_capacity = 0U; /* in samples */
static uint32_t rds_capture_buf_pos = 0U;

static void fmradio_rds_capture_start(void) {
    if(rds_capture_active || rds_capture_flush_pending) return;

    /* Try to allocate RAM buffer: 16 KB → 8 KB → 4 KB */
    static const uint32_t try_bytes[] = {16U * 1024U, 8U * 1024U, 4U * 1024U};
    rds_capture_buf = NULL;
    for(size_t i = 0; i < sizeof(try_bytes) / sizeof(try_bytes[0]); i++) {
        rds_capture_buf = malloc(try_bytes[i]);
        if(rds_capture_buf) {
            rds_capture_buf_capacity = try_bytes[i] / sizeof(uint16_t);
            break;
        }
    }
    if(!rds_capture_buf) {
        FURI_LOG_W(TAG, "ADC capture: malloc failed");
        return;
    }

    rds_capture_buf_pos = 0U;
    rds_capture_flush_pending = false;
    rds_capture_active = true;
    FURI_LOG_I(
        TAG, "ADC capture started (%lu samples buf)", (unsigned long)rds_capture_buf_capacity);
}

/* Called from cleanup paths — free resources without SD write */
static void fmradio_rds_capture_stop(void) {
    rds_capture_active = false;
    rds_capture_flush_pending = false;
    rds_capture_requested = false;

    if(rds_capture_buf) {
        free(rds_capture_buf);
        rds_capture_buf = NULL;
    }
    rds_capture_buf_capacity = 0U;
    rds_capture_buf_pos = 0U;
}

/* Called from block callback — MUST be fast (memcpy only) */
static void fmradio_rds_capture_write_block(const uint16_t* samples, size_t count) {
    if(!rds_capture_active || !rds_capture_buf) return;

    uint32_t remaining = rds_capture_buf_capacity - rds_capture_buf_pos;
    if(remaining == 0U) {
        rds_capture_active = false;
        rds_capture_flush_pending = true;
        return;
    }

    size_t to_copy = (count > remaining) ? remaining : count;
    memcpy(&rds_capture_buf[rds_capture_buf_pos], samples, to_copy * sizeof(uint16_t));
    rds_capture_buf_pos += (uint32_t)to_copy;

    if(rds_capture_buf_pos >= rds_capture_buf_capacity) {
        rds_capture_active = false;
        rds_capture_flush_pending = true;
    }
}

/* Called from timer callback (thread context) — safe to do SD I/O */
static void fmradio_rds_capture_flush_to_sd(void) {
    if(!rds_capture_flush_pending) return;
    rds_capture_flush_pending = false;

    FURI_LOG_I(TAG, "ADC capture flushing %lu samples to SD", (unsigned long)rds_capture_buf_pos);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) goto cleanup;

    storage_simply_mkdir(storage, SETTINGS_DIR);

    /* Write raw samples */
    File* f = storage_file_alloc(storage);
    if(f && storage_file_open(f, RDS_CAPTURE_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        if(rds_capture_buf && rds_capture_buf_pos > 0) {
            /* Write in 4 KB chunks to avoid hogging the timer task */
            const size_t chunk = 2048U; /* 2048 samples = 4 KB */
            uint32_t written = 0U;
            while(written < rds_capture_buf_pos) {
                size_t n = rds_capture_buf_pos - written;
                if(n > chunk) n = chunk;
                storage_file_write(f, &rds_capture_buf[written], n * sizeof(uint16_t));
                written += (uint32_t)n;
            }
        }
        storage_file_close(f);
    }
    if(f) storage_file_free(f);

    /* Write companion meta file */
    File* meta = storage_file_alloc(storage);
    if(meta && storage_file_open(meta, RDS_CAPTURE_META_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        char line[128];
        RdsAcquisitionStats stats;
        rds_acquisition_get_stats(&rds_acquisition, &stats);
        int n;
#define CAP_META(fmt, ...)                                \
    n = snprintf(line, sizeof(line), fmt, ##__VA_ARGS__); \
    if(n > 0) storage_file_write(meta, line, (size_t)n)

        CAP_META("capture_samples=%lu\n", (unsigned long)rds_capture_buf_pos);
        CAP_META(
            "configured_sample_rate_hz=%lu\n", (unsigned long)stats.configured_sample_rate_hz);
        CAP_META("measured_sample_rate_hz=%lu\n", (unsigned long)stats.measured_sample_rate_hz);
        CAP_META("adc_midpoint=%u\n", (unsigned)stats.adc_midpoint);
        CAP_META("tuned_freq_10khz=%lu\n", (unsigned long)fmradio_get_current_freq_10khz());
        CAP_META("capture_buf_capacity=%lu\n", (unsigned long)rds_capture_buf_capacity);
#undef CAP_META
        storage_file_close(meta);
    }
    if(meta) storage_file_free(meta);

    furi_record_close(RECORD_STORAGE);

    FURI_LOG_I(TAG, "ADC capture flush done");

cleanup:
    if(rds_capture_buf) {
        free(rds_capture_buf);
        rds_capture_buf = NULL;
    }
    rds_capture_buf_capacity = 0U;
    rds_capture_buf_pos = 0U;

    /* If RDS is off, we started ADC just for capture — stop it now */
    if(!rds_enabled) {
        if(rds_adc_timer_handle) {
            furi_timer_stop(rds_adc_timer_handle);
        }
        fmradio_rds_adc_stop();
    }
}

#else /* !ENABLE_ADC_CAPTURE */
static inline void fmradio_rds_capture_stop(void) {
}
static inline void fmradio_rds_capture_flush_to_sd(void) {
}
#endif /* ENABLE_ADC_CAPTURE */

static void fmradio_rds_metadata_reset(void) {
    rds_acquisition_reset(&rds_acquisition);
}

static void fmradio_rds_metadata_save(void) {
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

    storage_simply_mkdir(storage, SETTINGS_DIR);
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
    META_WRITE("dma_buffer_samples=%u\n", (unsigned)stats.dma_buffer_samples);
    META_WRITE("dma_block_samples=%u\n", (unsigned)stats.block_samples);
    META_WRITE("dma_half_events=%lu\n", (unsigned long)stats.dma_half_events);
    META_WRITE("dma_full_events=%lu\n", (unsigned long)stats.dma_full_events);
    META_WRITE("total_dma_blocks=%lu\n", (unsigned long)stats.total_dma_blocks);
    META_WRITE("delivered_blocks=%lu\n", (unsigned long)stats.delivered_blocks);
    META_WRITE("dropped_blocks=%lu\n", (unsigned long)stats.dropped_blocks);
    META_WRITE(
        "drop_rate_pct=%lu.%02lu\n",
        (unsigned long)(drop_rate_pct_x100 / 100U),
        (unsigned long)(drop_rate_pct_x100 % 100U));
    META_WRITE("pending_blocks=%u\n", (unsigned)stats.pending_blocks);
    META_WRITE("pending_peak_blocks=%u\n", (unsigned)stats.pending_peak_blocks);
    META_WRITE("adc_overrun_count=%lu\n", (unsigned long)stats.adc_overrun_count);
    META_WRITE("samples_delivered=%lu\n", (unsigned long)stats.samples_delivered);
    META_WRITE("dsp_symbol_count=%lu\n", (unsigned long)rds_dsp.symbol_count);
    META_WRITE("dsp_timing_adjust_q16=%ld\n", (long)rds_dsp.timing_adjust_q16);
    META_WRITE("dsp_timing_error_avg=%ld\n", (long)rds_dsp.timing_error_avg_q8);
    META_WRITE(
        "dsp_symbol_confidence_avg_q16=%lu\n", (unsigned long)rds_dsp.symbol_confidence_avg_q16);
    META_WRITE("dsp_block_symbols_last=%lu\n", (unsigned long)rds_dsp.block_symbol_count_last);
    META_WRITE(
        "dsp_block_confidence_last_q16=%lu\n", (unsigned long)rds_dsp.block_confidence_last_q16);
    META_WRITE(
        "dsp_block_confidence_avg_q16=%lu\n", (unsigned long)rds_dsp.block_confidence_avg_q16);
    META_WRITE(
        "dsp_corrected_confidence_avg_q16=%lu\n",
        (unsigned long)rds_dsp.corrected_confidence_avg_q16);
    META_WRITE(
        "dsp_uncorrectable_confidence_avg_q16=%lu\n",
        (unsigned long)rds_dsp.uncorrectable_confidence_avg_q16);
    META_WRITE(
        "dsp_block_corrected_count_last=%lu\n", (unsigned long)rds_dsp.block_corrected_count_last);
    META_WRITE(
        "dsp_block_uncorrectable_count_last=%lu\n",
        (unsigned long)rds_dsp.block_uncorrectable_count_last);
    META_WRITE(
        "dsp_block_corrected_confidence_last_q16=%lu\n",
        (unsigned long)rds_dsp.block_corrected_confidence_last_q16);
    META_WRITE(
        "dsp_block_uncorrectable_confidence_last_q16=%lu\n",
        (unsigned long)rds_dsp.block_uncorrectable_confidence_last_q16);
    META_WRITE("dsp_pilot_level_q8=%lu\n", (unsigned long)rds_dsp.pilot_level_q8);
    META_WRITE("dsp_rds_band_level_q8=%lu\n", (unsigned long)rds_dsp.rds_band_level_q8);
    META_WRITE("dsp_avg_abs_hp_q8=%lu\n", (unsigned long)rds_dsp.avg_abs_hp_q8);
    META_WRITE("dsp_avg_vector_mag_q8=%lu\n", (unsigned long)rds_dsp.avg_vector_mag_q8);
    META_WRITE("dsp_avg_decision_mag_q8=%lu\n", (unsigned long)rds_dsp.avg_decision_mag_q8);
    META_WRITE(
        "core_pilot_level_x1000=%lu\n", (unsigned long)(rds_core.pilot_level_q8 * 1000UL / 256UL));
    META_WRITE(
        "core_rds_band_level_x1000=%lu\n",
        (unsigned long)(rds_core.rds_band_level_q8 * 1000UL / 256UL));
    META_WRITE(
        "core_lock_quality_x1000=%lu\n",
        (unsigned long)(rds_core.lock_quality_q16 * 1000UL / 65535UL));
    META_WRITE("sync_state=%lu\n", (unsigned long)rds_sync_display);
    META_WRITE("ok_blocks_display=%lu\n", (unsigned long)rds_ok_blocks_display);
    META_WRITE("valid_blocks=%lu\n", (unsigned long)rds_core.valid_blocks);
    META_WRITE("corrected_blocks=%lu\n", (unsigned long)rds_core.corrected_blocks);
    META_WRITE("uncorrectable_blocks=%lu\n", (unsigned long)rds_core.uncorrectable_blocks);
    META_WRITE("sync_losses=%lu\n", (unsigned long)rds_core.sync_losses);
    META_WRITE("bit_slip_repairs=%lu\n", (unsigned long)rds_core.bit_slip_repairs);
    META_WRITE("tuned_freq_10khz=%lu\n", (unsigned long)fmradio_get_current_freq_10khz());
    META_WRITE("dsp_dc_estimate_q8=%ld\n", (long)rds_dsp.dc_estimate_q8);
    META_WRITE("core_groups_complete=%lu\n", (unsigned long)rds_core.groups_complete);
    META_WRITE("core_groups_type0=%lu\n", (unsigned long)rds_core.groups_type0);
    META_WRITE("core_groups_type2=%lu\n", (unsigned long)rds_core.groups_type2);
    META_WRITE("core_groups_other=%lu\n", (unsigned long)rds_core.groups_other);
    META_WRITE("core_pi_updates=%lu\n", (unsigned long)rds_core.pi_updates);
    META_WRITE("core_ps_updates=%lu\n", (unsigned long)rds_core.ps_updates);
    META_WRITE("core_last_pi=0x%04X\n", (unsigned)rds_core.last_pi);
    META_WRITE("core_ps_segment_mask=0x%02X\n", (unsigned)rds_core.ps_segment_mask);
    META_WRITE("core_ps_candidate=%.8s\n", rds_core.program.ps_candidate);
    META_WRITE("core_ps_ready=%u\n", (unsigned)rds_core.program.ps_ready);
    META_WRITE("core_presync_attempts=%lu\n", (unsigned long)rds_core.presync_attempts);
    META_WRITE(
        "core_presync_max_consecutive=%lu\n", (unsigned long)rds_core.presync_max_consecutive);
    META_WRITE("core_presync_consecutive_now=%u\n", (unsigned)rds_core.presync_consecutive);
    META_WRITE("core_flywheel_errors=%u\n", (unsigned)rds_core.flywheel_errors);
    META_WRITE("core_flywheel_limit=%u\n", (unsigned)rds_core.flywheel_limit);
    META_WRITE(
        "core_quality_gate_pilot_fail=%lu\n", (unsigned long)rds_core.quality_gate_pilot_fail);
    META_WRITE("core_quality_gate_rds_fail=%lu\n", (unsigned long)rds_core.quality_gate_rds_fail);
    META_WRITE("core_search_valid=%lu\n", (unsigned long)rds_core.search_valid);
    META_WRITE("core_search_corrected=%lu\n", (unsigned long)rds_core.search_corrected);
    META_WRITE("core_search_uncorrectable=%lu\n", (unsigned long)rds_core.search_uncorrectable);
    META_WRITE("core_sync_valid=%lu\n", (unsigned long)rds_core.sync_valid);
    META_WRITE("core_sync_corrected=%lu\n", (unsigned long)rds_core.sync_corrected);
    META_WRITE("core_sync_uncorrectable=%lu\n", (unsigned long)rds_core.sync_uncorrectable);
    META_WRITE("core_sync_bits_total=%lu\n", (unsigned long)rds_core.sync_bits_total);
    META_WRITE("core_events_emitted=%lu\n", (unsigned long)rds_core.events_emitted);
    META_WRITE("core_events_dropped=%lu\n", (unsigned long)rds_core.events_dropped);
    META_WRITE("core_pilot_detected=%u\n", (unsigned)rds_core.pilot_detected);
    META_WRITE("core_rds_carrier_detected=%u\n", (unsigned)rds_core.rds_carrier_detected);
    META_WRITE("core_expected_next_block=%u\n", (unsigned)rds_core.expected_next_block);

#undef META_WRITE

    storage_file_close(meta_file);
    storage_file_free(meta_file);
    furi_record_close(RECORD_STORAGE);
}

static void fmradio_rds_clear_station_name(void) {
    fmradio_state_lock();
    memset(rds_ps_display, 0, sizeof(rds_ps_display));
    rds_sync_display = RdsSyncStateSearch;
    rds_ok_blocks_display = 0U;
    fmradio_state_unlock();
}

static void fmradio_rds_update_ui_snapshot(void) {
    fmradio_state_lock();
    rds_sync_display = rds_core.sync_state;
    rds_ok_blocks_display = rds_core.valid_blocks + rds_core.corrected_blocks;
    fmradio_state_unlock();
}

static const char* fmradio_rds_sync_short_text(RdsSyncState state) {
    switch(state) {
    case RdsSyncStateSearch:
        return "srch";
    case RdsSyncStatePreSync:
        return "pre";
    case RdsSyncStateSync:
        return "sync";
    case RdsSyncStateLost:
        return "lost";
    default:
        return "?";
    }
}

static void fmradio_rds_on_tuned_frequency_changed(void) {
    fmradio_rds_clear_station_name();
    if(rds_enabled) {
        rds_core_set_tick_ms(&rds_core, furi_get_tick());
        rds_core_restart_sync(&rds_core);
        rds_dsp_reset(&rds_dsp);
        fmradio_rds_update_ui_snapshot();
        fmradio_rds_metadata_reset();
    }
    fmradio_settings_mark_dirty();
}

static void fmradio_rds_process_events(void) {
    RdsEvent event;

    rds_core_set_tick_ms(&rds_core, furi_get_tick());

    while(rds_core_pop_event(&rds_core, &event)) {
        if(event.type == RdsEventTypePsUpdated) {
            fmradio_state_lock();
            memcpy(rds_ps_display, event.ps, RDS_PS_LEN);
            rds_ps_display[RDS_PS_LEN] = '\0';
            fmradio_state_unlock();
        }
    }

    fmradio_rds_update_ui_snapshot();
}

void fmradio_rds_process_adc_block(const uint16_t* samples, size_t count, uint16_t adc_midpoint) {
    static uint8_t ui_snapshot_div = 0U;

#if ENABLE_ADC_CAPTURE
    /* Start capture on request (deferred from input callback to ISR-safe context) */
    if(rds_capture_requested && !rds_capture_active) {
        rds_capture_requested = false;
        fmradio_rds_capture_start();
    }

    /* During capture: ONLY write raw samples, skip DSP to save CPU */
    if(rds_capture_active) {
        fmradio_rds_capture_write_block(samples, count);
        return;
    }
#endif

    if(!rds_enabled) return;

    rds_core_set_tick_ms(&rds_core, furi_get_tick());
    rds_dsp_process_u16_samples(&rds_dsp, &rds_core, samples, count, adc_midpoint);
    ui_snapshot_div++;
    if(ui_snapshot_div >= 4U) {
        fmradio_rds_update_ui_snapshot();
        ui_snapshot_div = 0U;
    }
}

static void fmradio_rds_acquisition_block_callback(
    const uint16_t* samples,
    size_t count,
    uint16_t adc_midpoint,
    void* context) {
    UNUSED(context);
    fmradio_rds_process_adc_block(samples, count, adc_midpoint);
}

static bool fmradio_rds_adc_start(void) {
    bool started = rds_acquisition_start(&rds_acquisition);
    if(!started) {
        FURI_LOG_W(TAG, "RDS acquisition start failed");
    } else {
        RdsAcquisitionStats stats;
        rds_acquisition_get_stats(&rds_acquisition, &stats);
        rds_dsp_init(&rds_dsp, stats.configured_sample_rate_hz);
    }
    return started;
}

static void fmradio_rds_adc_stop(void) {
    rds_acquisition_stop(&rds_acquisition);
}

static void fmradio_rds_adc_timer_callback(void* context) {
    UNUSED(context);

#if ENABLE_ADC_CAPTURE
    /* Flush completed capture buffer to SD (safe: thread context) */
    fmradio_rds_capture_flush_to_sd();

    if(!rds_enabled && !rds_capture_active && !rds_capture_requested) return;
#else
    if(!rds_enabled) return;
#endif
    rds_acquisition_on_timer_tick(&rds_acquisition);
}

static void fmradio_controller_rds_change(VariableItem* item) {
    UNUSED(variable_item_get_context(item));
    uint8_t index = variable_item_get_current_value_index(item);
    rds_enabled = (index != 0);
    variable_item_set_current_value_text(item, rds_enabled ? "On" : "Off");

    fmradio_rds_clear_station_name();
    if(rds_enabled) {
        rds_core_set_tick_ms(&rds_core, furi_get_tick());
        rds_core_reset(&rds_core);
        rds_dsp_reset(&rds_dsp);
        fmradio_rds_metadata_reset();
        (void)fmradio_rds_adc_start();
        if(rds_adc_timer_handle) {
            furi_timer_start(rds_adc_timer_handle, furi_ms_to_ticks(RDS_ACQ_TIMER_MS));
        }
    } else {
        if(rds_adc_timer_handle) {
            furi_timer_stop(rds_adc_timer_handle);
        }
        fmradio_rds_metadata_save();
        fmradio_rds_capture_stop();
        fmradio_rds_adc_stop();
    }
    fmradio_settings_mark_dirty();
}
#endif /* ENABLE_RDS */

static void fmradio_controller_snc_change(VariableItem* item) {
    UNUSED(variable_item_get_context(item));

    uint8_t index = variable_item_get_current_value_index(item);
    tea_snc_enabled = (index != 0);
    variable_item_set_current_value_text(item, tea_snc_enabled ? "On" : "Off");

    tea5767_set_snc_enabled(tea_snc_enabled);
    (void)tea5767_set_snc(tea_snc_enabled);
    fmradio_settings_mark_dirty();
}

static void fmradio_controller_deemph_change(VariableItem* item) {
    UNUSED(variable_item_get_context(item));
    uint8_t index = variable_item_get_current_value_index(item);

    // index 0 => 50us, index 1 => 75us
    tea_deemph_75us = (index != 0);
    variable_item_set_current_value_text(item, tea_deemph_75us ? "75us" : "50us");

    tea5767_set_deemphasis_75us_enabled(tea_deemph_75us);
    (void)tea5767_set_deemphasis_75us(tea_deemph_75us);
    fmradio_settings_mark_dirty();
}

static void fmradio_controller_softmute_change(VariableItem* item) {
    UNUSED(variable_item_get_context(item));
    uint8_t index = variable_item_get_current_value_index(item);
    tea_softmute_enabled = (index != 0);
    variable_item_set_current_value_text(item, tea_softmute_enabled ? "On" : "Off");
    tea5767_set_softmute_enabled(tea_softmute_enabled);
    (void)tea5767_set_softmute(tea_softmute_enabled);
    fmradio_settings_mark_dirty();
}

static void fmradio_controller_highcut_change(VariableItem* item) {
    UNUSED(variable_item_get_context(item));
    uint8_t index = variable_item_get_current_value_index(item);
    tea_highcut_enabled = (index != 0);
    variable_item_set_current_value_text(item, tea_highcut_enabled ? "On" : "Off");
    tea5767_set_high_cut_enabled(tea_highcut_enabled);
    (void)tea5767_set_high_cut(tea_highcut_enabled);
    fmradio_settings_mark_dirty();
}

static void fmradio_controller_mono_change(VariableItem* item) {
    UNUSED(variable_item_get_context(item));
    uint8_t index = variable_item_get_current_value_index(item);
    tea_force_mono_enabled = (index != 0);
    variable_item_set_current_value_text(item, tea_force_mono_enabled ? "On" : "Off");
    tea5767_set_force_mono_enabled(tea_force_mono_enabled);
    (void)tea5767_set_force_mono(tea_force_mono_enabled);
    fmradio_settings_mark_dirty();
}

static void fmradio_controller_backlight_change(VariableItem* item) {
    UNUSED(variable_item_get_context(item));
    uint8_t index = variable_item_get_current_value_index(item);
    backlight_keep_on = (index != 0);
    variable_item_set_current_value_text(item, backlight_keep_on ? "On" : "Off");

    NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);
    fmradio_apply_backlight(notifications);
    furi_record_close(RECORD_NOTIFICATION);

    fmradio_settings_mark_dirty();
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
    FMRadioSubmenuIndexConfigure,
    FMRadioSubmenuIndexListen,
    FMRadioSubmenuIndexAbout,
} FMRadioSubmenuIndex;

typedef enum {
    FMRadioViewSubmenu,
    FMRadioViewConfigure,
    FMRadioViewListen,
    FMRadioViewAbout,
} FMRadioView;

// Define a struct to hold the application's components
typedef struct {
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;
    Submenu* submenu;
    VariableItemList* variable_item_list_config;
    VariableItem* item_freq;
    VariableItem* item_volume;
    VariableItem* item_pt_chip;
    VariableItem* item_snc;
    VariableItem* item_deemph;
    VariableItem* item_softmute;
    VariableItem* item_highcut;
    VariableItem* item_mono;
    VariableItem* item_backlight;
#ifdef ENABLE_RDS
    VariableItem* item_rds;
#endif
    View* listen_view;
    Widget* widget_about;
    FuriTimer* tick_timer;
#ifdef ENABLE_RDS
    FuriTimer* rds_adc_timer;
#endif
} FMRadio;

// Model struct for the Listen view (state lives in globals; kept for view_commit_model redraws)
typedef struct {
    uint8_t _dummy; // Flipper view system requires a non-zero model
} MyModel;

// Callback for navigation events

uint32_t fmradio_controller_navigation_exit_callback(void* context) {
    UNUSED(context);
    // Pre-close shutdown path: disable RDS work and let final teardown release resources.
#ifdef ENABLE_RDS
    if(rds_adc_timer_handle) {
        furi_timer_stop(rds_adc_timer_handle);
    }
    fmradio_rds_metadata_save();
    fmradio_rds_capture_stop();
    fmradio_rds_adc_stop();

    rds_enabled = false;
#endif

    uint8_t buffer[5]; // Create a buffer to hold the TEA5767 register values
    tea5767_sleep(buffer); // Call the tea5767_sleep function, passing the buffer as an argument

    // Persist last state (frequency/mute/attenuation)
    fmradio_settings_save();
    fmradio_presets_save();
    return VIEW_NONE;
}

// Callback for navigating to the submenu
uint32_t fmradio_controller_navigation_submenu_callback(void* context) {
    UNUSED(context);
    return FMRadioViewSubmenu;
}

// Callback for handling submenu selections
void fmradio_controller_submenu_callback(void* context, uint32_t index) {
    FMRadio* app = (FMRadio*)context;
    switch(index) {
    case FMRadioSubmenuIndexConfigure:
        view_dispatcher_switch_to_view(app->view_dispatcher, FMRadioViewConfigure);
        break;
    case FMRadioSubmenuIndexListen:
        view_dispatcher_switch_to_view(app->view_dispatcher, FMRadioViewListen);
        break;
    case FMRadioSubmenuIndexAbout:
        view_dispatcher_switch_to_view(app->view_dispatcher, FMRadioViewAbout);
        break;
    default:
        break;
    }
}

bool fmradio_controller_view_input_callback(InputEvent* event, void* context) {
    UNUSED(context);
    if(event->type == InputTypeLong && event->key == InputKeyLeft) {
        fmradio_seek_step(false);
        return true;
    } else if(event->type == InputTypeLong && event->key == InputKeyRight) {
        fmradio_seek_step(true);
        return true;
    } else if(event->type == InputTypeShort && event->key == InputKeyLeft) {
        // Use integer 10kHz math to avoid PLL quantization drift
        uint32_t fq = fmradio_get_current_freq_10khz();
        // Snap to nearest 100 kHz (10 units) grid before stepping
        fq = ((fq + 5) / 10) * 10;
        if(fq > 10)
            fq -= 10;
        else
            fq = 7600;
        fq = clamp_u32(fq, 7600U, 10800U);
        tea5767_SetFreqMHz(fq / 100.0f);
        fmradio_rds_on_tuned_frequency_changed();
        return true;
    } else if(event->type == InputTypeShort && event->key == InputKeyRight) {
        // Use integer 10kHz math to avoid PLL quantization drift
        uint32_t fq = fmradio_get_current_freq_10khz();
        // Snap to nearest 100 kHz (10 units) grid before stepping
        fq = ((fq + 5) / 10) * 10;
        fq += 10;
        fq = clamp_u32(fq, 7600U, 10800U);
        tea5767_SetFreqMHz(fq / 100.0f);
        fmradio_rds_on_tuned_frequency_changed();
        return true;
    } else if(event->type == InputTypeLong && event->key == InputKeyOk) {
#if ENABLE_ADC_CAPTURE
        // Start ADC capture to SD card (works with RDS on or off)
        if(!rds_capture_active) {
            if(!rds_enabled) {
                /* ADC not running — start it just for capture */
                (void)fmradio_rds_adc_start();
                if(rds_adc_timer_handle) {
                    furi_timer_start(rds_adc_timer_handle, furi_ms_to_ticks(RDS_ACQ_TIMER_MS));
                }
            }
            rds_capture_requested = true;
            fmradio_feedback_success();
            return true;
        }
#endif
        // Save current frequency to presets: select if already present, otherwise append
        uint32_t freq_10khz = fmradio_get_current_freq_10khz();
        fmradio_presets_add_or_select(freq_10khz);
        fmradio_presets_save();
        fmradio_feedback_success();
        return true;
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        fmradio_state_lock();
        current_volume = !current_volume;
        fmradio_state_unlock();
        fmradio_apply_pt_state();
        fmradio_settings_mark_dirty();
        return true; // Event was handled
    } else if(event->type == InputTypeShort && event->key == InputKeyUp) {
        fmradio_state_lock();
        if(preset_count > 0) {
            preset_index = (preset_index + 1) % preset_count;
            tea5767_SetFreqMHz(((float)preset_freq_10khz[preset_index]) / 100.0f);
            fmradio_presets_mark_dirty();
            fmradio_state_unlock();
        } else {
            fmradio_state_unlock();
            // Increment the current frequency index and loop back if at the end
            current_frequency_index = (current_frequency_index + 1) %
                                      (sizeof(frequency_values) / sizeof(frequency_values[0]));
            // Set the new frequency
            tea5767_SetFreqMHz(frequency_values[current_frequency_index]);
        }
        fmradio_rds_on_tuned_frequency_changed();
        return true; // Event was handled
    } else if(event->type == InputTypeShort && event->key == InputKeyDown) {
        fmradio_state_lock();
        if(preset_count > 0) {
            if(preset_index == 0) {
                preset_index = preset_count - 1;
            } else {
                preset_index--;
            }
            tea5767_SetFreqMHz(((float)preset_freq_10khz[preset_index]) / 100.0f);
            fmradio_presets_mark_dirty();
            fmradio_state_unlock();
        } else {
            fmradio_state_unlock();
            // Decrement the current frequency index and loop back if at the beginning
            if(current_frequency_index == 0) {
                current_frequency_index =
                    (sizeof(frequency_values) / sizeof(frequency_values[0])) - 1;
            } else {
                current_frequency_index--;
            }
            // Set the new frequency
            tea5767_SetFreqMHz(frequency_values[current_frequency_index]);
        }
        fmradio_rds_on_tuned_frequency_changed();
        return true; // Event was handled
    } else if(
        (event->type == InputTypeLong || event->type == InputTypeRepeat) &&
        event->key == InputKeyUp) {
        // Volume up => reduce attenuation
        fmradio_state_lock();
        if(pt_atten_db > 0) {
            pt_atten_db--;
            fmradio_state_unlock();
            fmradio_apply_pt_state();
            fmradio_settings_mark_dirty();
        } else {
            fmradio_state_unlock();
        }
        return true;
    } else if(
        (event->type == InputTypeLong || event->type == InputTypeRepeat) &&
        event->key == InputKeyDown) {
        // Volume down => increase attenuation
        fmradio_state_lock();
        if(pt_atten_db < 79) {
            pt_atten_db++;
            fmradio_state_unlock();
            fmradio_apply_pt_state();
            fmradio_settings_mark_dirty();
        } else {
            fmradio_state_unlock();
        }
        return true;
    }

    return false; // Event was not handled
}

// Callback for handling frequency changes
void fmradio_controller_frequency_change(VariableItem* item) {
    uint8_t index = variable_item_get_current_value_index(item);
    if(index >= COUNT_OF(frequency_values)) {
        index = 0;
        variable_item_set_current_value_index(item, index);
    }

    // Apply immediately
    if(index < COUNT_OF(frequency_values)) {
        tea5767_SetFreqMHz(frequency_values[index]);
        fmradio_rds_on_tuned_frequency_changed();
    }

    // Display the selected frequency value as text
    char frequency_display[16]; // Adjust the buffer size as needed
    snprintf(
        frequency_display, sizeof(frequency_display), "%.1f MHz", (double)frequency_values[index]);
    variable_item_set_current_value_text(item, frequency_display);
}

// Callback for handling volume changes
void fmradio_controller_volume_change(VariableItem* item) {
    uint8_t index = variable_item_get_current_value_index(item);
    if(index >= COUNT_OF(volume_values)) {
        index = 0;
        variable_item_set_current_value_index(item, index);
    }
    variable_item_set_current_value_text(
        item, volume_names[index]); // Display the selected volume as text

    // Apply immediately (this Config "Volume" is PT mute/unmute)
    if(index < COUNT_OF(volume_values)) {
        current_volume = (volume_values[index] != 0);
        fmradio_apply_pt_state();
        fmradio_settings_mark_dirty();
    }
}

void fmradio_controller_pt_chip_change(VariableItem* item) {
    uint8_t index = variable_item_get_current_value_index(item);
    if(index >= COUNT_OF(pt_chip_values)) {
        index = 0;
        variable_item_set_current_value_index(item, index);
    }

    pt_chip = pt_chip_values[index];
    variable_item_set_current_value_text(item, pt_chip_names[index]);

    (void)fmradio_pt_refresh_state(true);
    fmradio_apply_pt_state();
    fmradio_settings_mark_dirty();
}

static uint32_t fmradio_find_nearest_freq_index(float mhz) {
    if(COUNT_OF(frequency_values) == 0) return 0;
    uint32_t best = 0;
    float best_diff = fabsf(frequency_values[0] - mhz);
    for(uint32_t i = 1; i < COUNT_OF(frequency_values); i++) {
        float diff = fabsf(frequency_values[i] - mhz);
        if(diff < best_diff) {
            best_diff = diff;
            best = i;
        }
    }
    return best;
}

// Periodic background tick: I2C hot-plug check, debounced saves.
// Runs every 250 ms via FuriTimer, independent of which view is active.
static void fmradio_tick_callback(void* context) {
    FMRadio* app = (FMRadio*)context;
    uint32_t now = furi_get_tick();

    // PT hot-plug (every ~500 ms)
    static uint32_t last_pt_check = 0;
    if((now - last_pt_check) > furi_ms_to_ticks(500)) {
        fmradio_state_lock();
        bool was_ready = pt_ready_cached;
        fmradio_state_unlock();

        bool ready = fmradio_pt_refresh_state(false);

        if(ready && !was_ready) {
            fmradio_apply_pt_state();
        }
        last_pt_check = now;
    }

    // Debounced settings save (every ~2 s when dirty)
    static uint32_t last_settings_save = 0;
    if(settings_dirty && ((now - last_settings_save) > furi_ms_to_ticks(2000))) {
        fmradio_settings_save();
        last_settings_save = now;
    }

#ifdef ENABLE_RDS
    if(rds_enabled) {
        fmradio_rds_process_events();
    }
#endif

    // Debounced presets save (every ~2 s when dirty)
    static uint32_t last_presets_save = 0;
    if(presets_dirty && ((now - last_presets_save) > furi_ms_to_ticks(2000))) {
        fmradio_presets_save();
        last_presets_save = now;
    }

    // Refresh TEA5767 radio info (RSSI, stereo, frequency) every tick
    {
        /* Re-write PLL registers every ~1s to force signal level re-measurement.
           TEA5767 only updates ADC level on PLL lock, not continuously. */
        static uint32_t last_retune = 0;
        if((now - last_retune) > furi_ms_to_ticks(1000)) {
            tea5767_retune();
            last_retune = now;
        }

        uint8_t tea_buf[5];
        struct RADIO_INFO info;
        fmradio_state_lock();
        if(tea5767_get_radio_info(tea_buf, &info)) {
            tea_info_cached = info;
            tea_info_valid = true;
            tea_info_read_count++;
        }
        fmradio_state_unlock();
    }

    // Trigger a redraw so the Listen view picks up fresh data
    if(app->listen_view) {
        view_commit_model(app->listen_view, false);
    }
}

// Callback for drawing the view

void fmradio_controller_view_draw_callback(Canvas* canvas, void* model) {
    (void)model; // Mark model as unused

    char frequency_display[64];
    char signal_display[64];
    char audio_display[48];
    char pt_display[32];
#ifdef ENABLE_RDS
    char rds_ps_local[RDS_PS_LEN + 1U];
    bool local_rds_enabled;
    RdsSyncState local_rds_sync;
    uint32_t local_rds_ok_blocks;
#endif

    // Draw strings on the canvas
    canvas_draw_str(canvas, 45, 10, "FM Radio");

    // Draw button prompts
    canvas_set_font(canvas, FontSecondary);
    elements_button_left(canvas, "-0.1");
    elements_button_right(canvas, "+0.1");
    elements_button_center(canvas, "Mute");
    elements_button_top_left(canvas, " Pre");
    elements_button_top_right(canvas, "Pre ");

    fmradio_state_lock();
    bool local_pt_ready = pt_ready_cached;
    uint8_t local_pt_atten = pt_atten_db;
    bool local_muted = current_volume;
    struct RADIO_INFO info = tea_info_cached;
    bool info_valid = tea_info_valid;
    uint32_t local_read_count = tea_info_read_count;
#ifdef ENABLE_RDS
    local_rds_enabled = rds_enabled;
    local_rds_sync = rds_sync_display;
    local_rds_ok_blocks = rds_ok_blocks_display;
    memcpy(rds_ps_local, rds_ps_display, sizeof(rds_ps_local));
#endif
    fmradio_state_unlock();

    const char* pt_name = fmradio_pt_active_name();

    if(local_pt_ready) {
        snprintf(
            pt_display,
            sizeof(pt_display),
            "%s: OK  Vol: -%udB",
            pt_name,
            (unsigned)local_pt_atten);
    } else {
        snprintf(pt_display, sizeof(pt_display), "%s: ERROR", pt_name);
    }
    canvas_draw_str(canvas, 10, 51, pt_display);

    if(info_valid) {
#ifdef ENABLE_RDS
        if(local_rds_enabled && rds_ps_local[0] != '\0') {
            snprintf(
                frequency_display,
                sizeof(frequency_display),
                "F: %.1f %.*s",
                (double)info.frequency,
                (int)RDS_PS_LEN,
                rds_ps_local);
        } else
#endif
        {
            snprintf(
                frequency_display,
                sizeof(frequency_display),
                "F: %.1f MHz",
                (double)info.frequency);
        }
        canvas_draw_str(canvas, 10, 21, frequency_display);

        snprintf(
            signal_display,
            sizeof(signal_display),
            "RSSI:%d %s t%lu",
            info.signalLevel,
            info.signalQuality,
            (unsigned long)local_read_count);
        canvas_draw_str(canvas, 10, 41, signal_display);

        if(local_muted) {
            snprintf(audio_display, sizeof(audio_display), "A:MT");
        } else {
            snprintf(audio_display, sizeof(audio_display), "A:%s", info.stereo ? "ST" : "MO");
        }

        size_t used = strlen(audio_display);
#ifdef ENABLE_RDS
#if ENABLE_ADC_CAPTURE
        if(rds_capture_active) {
            uint32_t pct = (rds_capture_buf_capacity > 0) ?
                               (rds_capture_buf_pos * 100U) / rds_capture_buf_capacity :
                               0U;
            snprintf(
                audio_display + used,
                sizeof(audio_display) - used,
                " REC %lu%%",
                (unsigned long)pct);
        } else
#endif
        {
            snprintf(
                audio_display + used,
                sizeof(audio_display) - used,
                " R:%s %lu",
                local_rds_enabled ? fmradio_rds_sync_short_text(local_rds_sync) : "off",
                (unsigned long)local_rds_ok_blocks);
        }
#else
        (void)used;
#endif
        canvas_draw_str(canvas, 10, 31, audio_display);
    } else {
        snprintf(frequency_display, sizeof(frequency_display), "TEA5767 Not Detected");
        canvas_draw_str(canvas, 10, 21, frequency_display);

        snprintf(signal_display, sizeof(signal_display), "Pin 15 = SDA | Pin 16 = SCL");
        canvas_draw_str(canvas, 10, 41, signal_display);
    }
}

// Allocate memory for the application
FMRadio* fmradio_controller_alloc() {
    FMRadio* app = (FMRadio*)malloc(sizeof(FMRadio));
    if(!app) return NULL;
    memset(app, 0, sizeof(FMRadio));

    bool gui_opened = false;
    Gui* gui = furi_record_open(RECORD_GUI);
    if(!gui) goto fail;
    gui_opened = true;

    state_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    if(!state_mutex) goto fail;

    // Initialize the view dispatcher
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(app->view_dispatcher);
    if(!app->view_dispatcher) goto fail;
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);

    // Initialize the submenu
    app->submenu = submenu_alloc();
    if(!app->submenu) goto fail;
    submenu_add_item(
        app->submenu,
        "Listen Now",
        FMRadioSubmenuIndexListen,
        fmradio_controller_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Config",
        FMRadioSubmenuIndexConfigure,
        fmradio_controller_submenu_callback,
        app);
    submenu_add_item(
        app->submenu, "About", FMRadioSubmenuIndexAbout, fmradio_controller_submenu_callback, app);
    view_set_previous_callback(
        submenu_get_view(app->submenu), fmradio_controller_navigation_exit_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, FMRadioViewSubmenu, submenu_get_view(app->submenu));
    view_dispatcher_switch_to_view(app->view_dispatcher, FMRadioViewSubmenu);

    // Initialize the variable item list for configuration
    app->variable_item_list_config = variable_item_list_alloc();
    if(!app->variable_item_list_config) goto fail;
    variable_item_list_reset(app->variable_item_list_config);

    // Add TEA5767 SNC toggle
    app->item_snc = variable_item_list_add(
        app->variable_item_list_config, "SNC", 2, fmradio_controller_snc_change, app);
    if(!app->item_snc) goto fail;
    variable_item_set_current_value_index(app->item_snc, tea_snc_enabled ? 1 : 0);
    variable_item_set_current_value_text(app->item_snc, tea_snc_enabled ? "On" : "Off");

    // Add TEA5767 de-emphasis time constant
    app->item_deemph = variable_item_list_add(
        app->variable_item_list_config, "De-emph", 2, fmradio_controller_deemph_change, app);
    if(!app->item_deemph) goto fail;
    variable_item_set_current_value_index(app->item_deemph, tea_deemph_75us ? 1 : 0);
    variable_item_set_current_value_text(app->item_deemph, tea_deemph_75us ? "75us" : "50us");

    // Add TEA5767 SoftMute
    app->item_softmute = variable_item_list_add(
        app->variable_item_list_config, "SoftMute", 2, fmradio_controller_softmute_change, app);
    if(!app->item_softmute) goto fail;
    variable_item_set_current_value_index(app->item_softmute, tea_softmute_enabled ? 1 : 0);
    variable_item_set_current_value_text(app->item_softmute, tea_softmute_enabled ? "On" : "Off");

    // Add TEA5767 High Cut Control
    app->item_highcut = variable_item_list_add(
        app->variable_item_list_config, "HighCut", 2, fmradio_controller_highcut_change, app);
    if(!app->item_highcut) goto fail;
    variable_item_set_current_value_index(app->item_highcut, tea_highcut_enabled ? 1 : 0);
    variable_item_set_current_value_text(app->item_highcut, tea_highcut_enabled ? "On" : "Off");

    // Add TEA5767 Force mono
    app->item_mono = variable_item_list_add(
        app->variable_item_list_config, "Mono", 2, fmradio_controller_mono_change, app);
    if(!app->item_mono) goto fail;
    variable_item_set_current_value_index(app->item_mono, tea_force_mono_enabled ? 1 : 0);
    variable_item_set_current_value_text(app->item_mono, tea_force_mono_enabled ? "On" : "Off");

    // Keep backlight on while app runs
    app->item_backlight = variable_item_list_add(
        app->variable_item_list_config, "Backlight", 2, fmradio_controller_backlight_change, app);
    if(!app->item_backlight) goto fail;
    variable_item_set_current_value_index(app->item_backlight, backlight_keep_on ? 1 : 0);
    variable_item_set_current_value_text(app->item_backlight, backlight_keep_on ? "On" : "Off");

#ifdef ENABLE_RDS
    app->item_rds = variable_item_list_add(
        app->variable_item_list_config, "RDS", 2, fmradio_controller_rds_change, app);
    if(!app->item_rds) goto fail;
    variable_item_set_current_value_index(app->item_rds, rds_enabled ? 1 : 0);
    variable_item_set_current_value_text(app->item_rds, rds_enabled ? "On" : "Off");
#endif

    // Add frequency configuration
    app->item_freq = variable_item_list_add(
        app->variable_item_list_config,
        "Freq (MHz)",
        COUNT_OF(frequency_values),
        fmradio_controller_frequency_change,
        app);
    if(!app->item_freq) goto fail;
    uint32_t frequency_index = 0;
    variable_item_set_current_value_index(app->item_freq, frequency_index);

    // Add volume configuration
    app->item_volume = variable_item_list_add(
        app->variable_item_list_config,
        "Volume",
        COUNT_OF(volume_values),
        fmradio_controller_volume_change,
        app);
    if(!app->item_volume) goto fail;
    uint8_t volume_index = 0;
    variable_item_set_current_value_index(app->item_volume, volume_index);

    app->item_pt_chip = variable_item_list_add(
        app->variable_item_list_config,
        "PT Chip",
        COUNT_OF(pt_chip_values),
        fmradio_controller_pt_chip_change,
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

    view_set_previous_callback(
        variable_item_list_get_view(app->variable_item_list_config),
        fmradio_controller_navigation_submenu_callback);
    view_dispatcher_add_view(
        app->view_dispatcher,
        FMRadioViewConfigure,
        variable_item_list_get_view(app->variable_item_list_config));

    // Initialize the Listen view
    app->listen_view = view_alloc();
    if(!app->listen_view) goto fail;
    view_set_draw_callback(app->listen_view, fmradio_controller_view_draw_callback);
    view_set_input_callback(app->listen_view, fmradio_controller_view_input_callback);
    view_set_previous_callback(app->listen_view, fmradio_controller_navigation_submenu_callback);
    view_allocate_model(app->listen_view, ViewModelTypeLockFree, sizeof(MyModel));

    view_dispatcher_add_view(app->view_dispatcher, FMRadioViewListen, app->listen_view);

    // Initialize the widget for displaying information about the app
    app->widget_about = widget_alloc();
    if(!app->widget_about) goto fail;
    widget_add_text_scroll_element(
        app->widget_about,
        0,
        0,
        128,
        64,
        "FM Radio. (v" FMRADIO_UI_VERSION
        ")\n---\n Created By Coolshrimp\n Fork/extended by pchmielewski1\n\n"
        "Left/Right (short) = Tune -/+ 0.1MHz\n"
        "Left/Right (hold) = Seek next/prev\n"
        "OK (short) = Mute PT\n"
        "OK (hold) = Save to preset\n"
        "Up/Down (short) = Preset next/prev\n"
        "Up/Down (hold) = Volume PT\n\n"
        "Band: 76.0-108.0MHz\n\n"
#ifdef ENABLE_RDS
        "Config: SNC / De-emph / SoftMute / HighCut / Mono / RDS\n"
#else
        "Config: SNC / De-emph / SoftMute / HighCut / Mono\n"
#endif
        "Try toggling while listening for feedback");
    view_set_previous_callback(
        widget_get_view(app->widget_about), fmradio_controller_navigation_submenu_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, FMRadioViewAbout, widget_get_view(app->widget_about));

    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    if(!app->notifications) goto fail;

    // Load persisted state (if present)
    fmradio_presets_load();
    fmradio_settings_load();

#ifdef ENABLE_RDS
    rds_core_set_tick_ms(&rds_core, furi_get_tick());
    rds_core_reset(&rds_core);
    rds_dsp_init(&rds_dsp, RDS_ACQ_TARGET_SAMPLE_RATE_HZ);
    rds_acquisition_init(
        &rds_acquisition,
        rds_adc_pin,
        rds_adc_channel,
        RDS_ADC_FIXED_MIDPOINT,
        fmradio_rds_acquisition_block_callback,
        NULL);
    fmradio_rds_clear_station_name();
    if(rds_enabled) {
        fmradio_rds_metadata_reset();
        (void)fmradio_rds_adc_start();
    }
#endif

    // Apply backlight policy after loading settings
    fmradio_apply_backlight(app->notifications);

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
    if(app->item_freq) {
        float freq = tea5767_GetFreq();
        if(freq > 0.0f) {
            uint32_t idx = fmradio_find_nearest_freq_index(freq);
            variable_item_set_current_value_index(app->item_freq, idx);
            char frequency_display[16];
            snprintf(
                frequency_display,
                sizeof(frequency_display),
                "%.1f MHz",
                (double)frequency_values[idx]);
            variable_item_set_current_value_text(app->item_freq, frequency_display);
        }
    }
    if(app->item_deemph) {
        variable_item_set_current_value_index(app->item_deemph, tea_deemph_75us ? 1 : 0);
        variable_item_set_current_value_text(app->item_deemph, tea_deemph_75us ? "75us" : "50us");
    }
    if(app->item_softmute) {
        variable_item_set_current_value_index(app->item_softmute, tea_softmute_enabled ? 1 : 0);
        variable_item_set_current_value_text(
            app->item_softmute, tea_softmute_enabled ? "On" : "Off");
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
#endif

    // Give PT controllers time to settle after power-on before touching I2C.
    furi_delay_ms(200);
    (void)fmradio_pt_refresh_state(true);
    fmradio_apply_pt_state();

    // Start periodic background tick (I2C hot-plug, debounced saves)
    app->tick_timer = furi_timer_alloc(fmradio_tick_callback, FuriTimerTypePeriodic, app);
    if(!app->tick_timer) goto fail;
    furi_timer_start(app->tick_timer, furi_ms_to_ticks(250));

#ifdef ENABLE_RDS
    app->rds_adc_timer =
        furi_timer_alloc(fmradio_rds_adc_timer_callback, FuriTimerTypePeriodic, app);
    if(!app->rds_adc_timer) goto fail;
    rds_adc_timer_handle = app->rds_adc_timer;
    furi_timer_start(app->rds_adc_timer, furi_ms_to_ticks(RDS_ACQ_TIMER_MS));
#endif

#ifdef BACKLIGHT_ALWAYS_ON
    notification_message(app->notifications, &sequence_display_backlight_enforce_on);
#endif

    return app;

fail:
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
        rds_adc_timer_handle = NULL;
        fmradio_rds_metadata_save();
        fmradio_rds_capture_stop();
        fmradio_rds_adc_stop();
#endif
        if(app->notifications) {
            notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
            furi_record_close(RECORD_NOTIFICATION);
            app->notifications = NULL;
        }
        if(app->widget_about) {
            if(app->view_dispatcher)
                view_dispatcher_remove_view(app->view_dispatcher, FMRadioViewAbout);
            widget_free(app->widget_about);
            app->widget_about = NULL;
        }
        if(app->listen_view) {
            if(app->view_dispatcher)
                view_dispatcher_remove_view(app->view_dispatcher, FMRadioViewListen);
            view_free(app->listen_view);
            app->listen_view = NULL;
        }
        if(app->variable_item_list_config) {
            if(app->view_dispatcher)
                view_dispatcher_remove_view(app->view_dispatcher, FMRadioViewConfigure);
            variable_item_list_free(app->variable_item_list_config);
            app->variable_item_list_config = NULL;
        }
        if(app->submenu) {
            if(app->view_dispatcher)
                view_dispatcher_remove_view(app->view_dispatcher, FMRadioViewSubmenu);
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

// Free memory used by the application
void fmradio_controller_free(FMRadio* app) {
    if(!app) return;

    // Stop background tick timer
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
    rds_adc_timer_handle = NULL;
    fmradio_rds_metadata_save();
    fmradio_rds_capture_stop();
    fmradio_rds_adc_stop();
#endif

    // Always restore auto backlight on exit
    if(app->notifications) {
        notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
        furi_record_close(RECORD_NOTIFICATION);
        app->notifications = NULL;
    }

    if(app->widget_about) {
        if(app->view_dispatcher)
            view_dispatcher_remove_view(app->view_dispatcher, FMRadioViewAbout);
        widget_free(app->widget_about);
        app->widget_about = NULL;
    }
    if(app->listen_view) {
        if(app->view_dispatcher)
            view_dispatcher_remove_view(app->view_dispatcher, FMRadioViewListen);
        view_free(app->listen_view);
        app->listen_view = NULL;
    }
    if(app->variable_item_list_config) {
        if(app->view_dispatcher)
            view_dispatcher_remove_view(app->view_dispatcher, FMRadioViewConfigure);
        variable_item_list_free(app->variable_item_list_config);
        app->variable_item_list_config = NULL;
    }
    if(app->submenu) {
        if(app->view_dispatcher)
            view_dispatcher_remove_view(app->view_dispatcher, FMRadioViewSubmenu);
        submenu_free(app->submenu);
        app->submenu = NULL;
    }
    if(app->view_dispatcher) {
        view_dispatcher_free(app->view_dispatcher);
        app->view_dispatcher = NULL;
    }
    furi_record_close(RECORD_GUI);

    if(state_mutex) {
        furi_mutex_free(state_mutex);
        state_mutex = NULL;
    }

    free(app);
}

// Main function to start the application
int32_t fmradio_controller_app(void* p) {
    UNUSED(p);

    FMRadio* app = fmradio_controller_alloc();
    if(!app) {
        FURI_LOG_E(TAG, "App allocation failed");
        return -1;
    }
    view_dispatcher_run(app->view_dispatcher);

    fmradio_controller_free(app);
    return 0;
}
