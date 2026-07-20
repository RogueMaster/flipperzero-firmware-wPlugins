/**
 *
 * @author Coolshrimp - CoolshrimpModz.com
 *
 * @brief FM Radio using TEA5767 / Si4703 FM radio chips.
 * @version 2.0
 * @date 2026-07-12
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
#include <gui/icon.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <stdbool.h>  // for true/false
#include <stdint.h>   // for uint8_t, size_t
#include <stdio.h>    // for snprintf
#include <stdlib.h>   // for malloc/free
#include <string.h>   // for string operations
#include <math.h>     // for fabsf
#include "TEA5767/TEA5767.h"
#include "Si4703/Si4703.h"
#include "fm_radio_icons.h"

// Define macros for easier management
#define BACKLIGHT_ALWAYS_ON
#define TAG "FM_Radio"
#define MAX_STATIONS 100
#define MAX_STATION_NAME 64
#define MAX_LINE_LENGTH 128
#define MAX_REGIONS 20

// Declare global variables
uint8_t volume_values[] = {0, 1};
char* volume_names[] = {"Un-Muted", "Muted"};
bool current_volume = 1;  // Current volume state
int* signal_strength;  // Signal strength (unused, consider removing or implementing)
uint8_t tea5767_registers[5];
uint16_t si4703_registers[16];
uint32_t current_station_index = 0;  // Default to the first frequency

typedef enum {
    RadioChipNone = 0,
    RadioChipTEA5767,
    RadioChipSi4703,
} RadioChipType;

static RadioChipType detected_chip = RadioChipNone;

// Station struct to hold frequency and station name
typedef struct {
    float frequency;
    char name[MAX_STATION_NAME];
} Station;

// Region definition
typedef struct {
    char name[64];
    char filename[64];
} Region;

// Audio mode enumeration
typedef enum {
    AUDIO_MODE_STEREO = 0,
    AUDIO_MODE_MONO_LEFT = 1,
    AUDIO_MODE_MONO_RIGHT = 2,
} AudioMode;

// Chip selection override (Auto lets the app detect; forced values skip detection)
typedef enum {
    ChipSelectAuto = 0,
    ChipSelectTEA5767 = 1,
    ChipSelectSi4703 = 2,
} ChipSelect;

// App settings structure
// NOTE: saved raw to settings.cfg — adding fields changes sizeof(AppSettings),
// which invalidates old files once (loader falls back to defaults and re-saves).
typedef struct {
    bool mute_on_exit;
    uint8_t seek_strength;  // 1-15
    AudioMode audio_mode;
    uint32_t region_index;
    ChipSelect chip_select;
    uint8_t start_volume;     // 0=Low 1=Med 2=High 3=Last (remember previous volume)
    uint8_t last_volume_step; // remembered volume step (1-3), used when start_volume==Last
    bool resume_station;      // restore last tuned frequency on startup
    float last_frequency;     // last tuned frequency in MHz (0 = none saved yet)
} AppSettings;

// Default settings
AppSettings app_settings = {
    .mute_on_exit = true,
    .seek_strength = 7,
    .audio_mode = AUDIO_MODE_STEREO,
    .region_index = 0,
    .chip_select = ChipSelectAuto,
    .start_volume = 3,      // Last: remember volume across runs
    .last_volume_step = 1,  // Low until the user changes it
    .resume_station = true,
    .last_frequency = 0.0f,
};

// Start Volume option names (index matches app_settings.start_volume)
const char* start_volume_names[] = {"Low", "Med", "High", "Last"};
#define NUM_START_VOLUME_OPTIONS 4

#define NUM_VOLUME_VALUES (sizeof(volume_values) / sizeof(volume_values[0]))

// Audio mode names
const char* audio_mode_names[] = {
    "Stereo",
    "Mono (Left)",
    "Mono (Right)",
};
#define NUM_AUDIO_MODES 3

// Chip select names
const char* chip_select_names[] = {"Auto", "TEA5767", "Si4703"};
#define NUM_CHIP_SELECT_OPTIONS 3

// Available regions (dynamically populated)
Region regions[MAX_REGIONS];
uint32_t NUM_REGIONS = 0;
bool regions_scanned_from_sd = false;

// Dynamic station list (loaded from SD card)
Station* stations = NULL;
uint32_t num_stations = 0;
uint32_t current_region_index = 0;

// Declare local buffers for text display
char frequency_display[64];
char station_display[256];
char signal_display[64];
char volume_display[32];

// Cache last radio info to avoid heavy I2C work in every draw frame
static struct RADIO_INFO cached_info;
static bool cached_info_valid = false;
static uint32_t cached_info_last_tick = 0;
static const uint32_t radio_info_refresh_ms = 250;

// Consecutive I2C failure counter — triggers fast recovery after threshold
static uint8_t radio_fail_count = 0;
#define RADIO_FAIL_THRESHOLD 5
#define RADIO_RECONNECT_INTERVAL_MS 2000

static uint32_t radio_last_reconnect_tick = 0;
static bool radio_disconnect_notified = false;

// TEA5767 STATUS bytes have no mute-state bit — track it here instead
static bool radio_muted = false;

// Simple float parser for frequencies (avoids locale/sscanf issues)
static bool parse_frequency(const char* text, float* out_value) {
    if(!text || !out_value) return false;

    // Skip UTF-8 BOM if present
    if((unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF) {
        text += 3;
    }

    // Skip leading whitespace
    while(*text == ' ' || *text == '\t') text++;

    int int_part = 0;
    int frac_part = 0;
    int frac_div = 1;
    bool has_digits = false;

    while(*text >= '0' && *text <= '9') {
        has_digits = true;
        int_part = int_part * 10 + (*text - '0');
        text++;
    }

    if(*text == '.') {
        text++;
        while(*text >= '0' && *text <= '9') {
            has_digits = true;
            frac_part = frac_part * 10 + (*text - '0');
            frac_div *= 10;
            text++;
        }
    }

    if(!has_digits) return false;

    *out_value = (float)int_part + ((float)frac_part / (float)frac_div);
    return true;
}

// ============================================================================
// Station Management Functions
// ============================================================================

// Forward declarations (used by unified radio wrappers below)
void apply_audio_mode(uint8_t* buffer, AudioMode mode);
void apply_seek_strength(uint8_t* buffer, uint8_t strength);
static void radio_apply_volume_step(void);

static const char* radio_get_chip_name(void) {
    switch(detected_chip) {
        case RadioChipTEA5767:
            return "TEA5767";
        case RadioChipSi4703:
            return "Si4703";
        default:
            return "None";
    }
}

static bool detect_radio_chip(void) {
    detected_chip = RadioChipNone;
    radio_fail_count = 0;

    if(app_settings.chip_select == ChipSelectTEA5767) {
        // Skip Si4703 GPIO powerup entirely — just probe TEA5767
        furi_delay_ms(10);
        if(tea5767_is_device_ready()) {
            detected_chip = RadioChipTEA5767;
            FURI_LOG_I(TAG, "Chip forced+detected: TEA5767");
            return true;
        }
        FURI_LOG_W(TAG, "TEA5767 forced but not found");
        return false;
    }

    if(app_settings.chip_select == ChipSelectSi4703) {
        if(si4703_is_device_ready()) {
            detected_chip = RadioChipSi4703;
            FURI_LOG_I(TAG, "Chip forced+detected: Si4703");
            return true;
        }
        FURI_LOG_W(TAG, "Si4703 forced but not found");
        return false;
    }

    // Auto: try Si4703 first (needs GPIO reset sequence)
    if(si4703_is_device_ready()) {
        detected_chip = RadioChipSi4703;
        FURI_LOG_I(TAG, "Chip auto-detected: Si4703");
        return true;
    }

    // Si4703 powerup manipulates I2C GPIO — let bus settle before probing TEA5767
    furi_delay_ms(50);

    if(tea5767_is_device_ready()) {
        detected_chip = RadioChipTEA5767;
        FURI_LOG_I(TAG, "Chip auto-detected: TEA5767");
        return true;
    }

    FURI_LOG_W(TAG, "No FM chip detected");
    return false;
}

static bool radio_init(void) {
    if(detected_chip == RadioChipNone) {
        detect_radio_chip();
    }

    radio_muted = false;  // Reset mute state whenever chip is (re)initialized

    if(detected_chip == RadioChipSi4703) {
        if(!si4703_init(si4703_registers)) return false;
        radio_apply_volume_step(); // restore user's volume after (re)init resets it to 8
        return true;
    } else if(detected_chip == RadioChipTEA5767) {
        return tea5767_init(tea5767_registers);
    }

    return false;
}

static uint8_t radio_map_seek_strength_to_si4703_threshold(uint8_t strength) {
    // strength: 1..15 -> threshold: ~10..40 (default around 25)
    if(strength < 1) strength = 1;
    if(strength > 15) strength = 15;
    const uint8_t min_th = 10;
    const uint8_t max_th = 40;
    return (uint8_t)(min_th + ((uint32_t)(strength - 1) * (max_th - min_th)) / 14);
}

static bool radio_set_frequency_from_mhz(float freq_mhz) {
    if(freq_mhz < 87.5f || freq_mhz > 108.0f) freq_mhz = 95.0f;

    if(detected_chip == RadioChipSi4703) {
        const int freq_khz = (int)(freq_mhz * 1000.0f);
        if(!si4703_set_frequency(si4703_registers, freq_khz)) return false;
        si4703_set_stereo(si4703_registers, app_settings.audio_mode == AUDIO_MODE_STEREO);
        return true;
    } else if(detected_chip == RadioChipTEA5767) {
        const int freq_10khz = (int)(freq_mhz * 100.0f);
        uint8_t buffer[5];
        // IMPORTANT: TEA5767 read-back uses a completely different register layout
        // than the write registers (status bytes ≠ config bytes).  Always start
        // from a known write-register state via tea5767_init, then overlay the
        // target frequency and audio mode before writing to the chip.
        if(!tea5767_init(buffer)) return false;
        if(!tea5767_set_frequency(buffer, freq_10khz)) return false;
        apply_audio_mode(buffer, app_settings.audio_mode);
        return tea5767_write_registers(buffer);
    }

    return false;
}

static bool radio_seek_from_current(bool seek_up) {
    if(detected_chip == RadioChipSi4703) {
        // Do NOT pre-tune before seek: si4703_set_frequency starts an async tune
        // (sets si4703_op = Tune) and si4703_seek immediately returns false when
        // si4703_op != None. The Si4703 seek wraps the full band, so no pre-tune needed.
        si4703_set_seek_threshold(si4703_registers, radio_map_seek_strength_to_si4703_threshold(app_settings.seek_strength));
        si4703_set_stereo(si4703_registers, app_settings.audio_mode == AUDIO_MODE_STEREO);
        return si4703_seek(si4703_registers, seek_up);
    } else if(detected_chip == RadioChipTEA5767) {
        uint8_t buffer[5];
        // Read current frequency from chip status registers (STATUS format is correct
        // for extracting PLL frequency bits — it's the write buffer that differs).
        float current_freq = tea5767_GetFreq();
        if(current_freq < 87.5f || current_freq > 108.0f) current_freq = 95.0f;

        // Build a clean write-register buffer via init, then overlay the current
        // frequency so the chip seeks from where it is now.
        if(!tea5767_init(buffer)) return false;
        int freq_10khz = (int)(current_freq * 100.0f);
        tea5767_set_frequency(buffer, freq_10khz);
        apply_seek_strength(buffer, app_settings.seek_strength);
        apply_audio_mode(buffer, app_settings.audio_mode);
        buffer[REG_1] |= REG_1_SM;
        if(seek_up) buffer[REG_3] |= REG_3_SUD;
        else buffer[REG_3] &= ~REG_3_SUD;
        return tea5767_write_registers(buffer);
    }

    return false;
}

// Volume steps for the Si4703's hardware volume register (SYSCONFIG2 bits 3:0).
// The TEA5767 has no volume control, so OK stays a plain mute toggle there.
static const uint8_t si4703_volume_steps[] = {0, 5, 10, 15}; // Muted, Low, Med, High
static const char* volume_step_names[] = {"Muted", "Vol: Low", "Vol: Med", "Vol: High"};
#define VOLUME_STEP_MAX 3
static uint8_t volume_step = 1;      // start at Low; each OK press steps upward
static uint8_t last_volume_step = 1; // restore target when hold-to-mute is released

static void radio_apply_volume_step(void) {
    if(detected_chip != RadioChipSi4703) return;
    if(volume_step == 0) {
        si4703_set_mute(si4703_registers, true);
    } else {
        si4703_set_volume(si4703_registers, si4703_volume_steps[volume_step]);
        si4703_set_mute(si4703_registers, false);
    }
}

// Status-line text: Si4703 shows the volume step, TEA5767 just Muted/Playing
static const char* radio_play_state_str(bool muted) {
    if(muted) return "Muted";
    if(detected_chip == RadioChipSi4703) return volume_step_names[volume_step];
    return "Playing";
}

// OK short press: Si4703 cycles Low -> Med -> High -> Muted -> Low; TEA5767 toggles mute
static void radio_ok_pressed(void) {
    if(detected_chip == RadioChipSi4703) {
        volume_step = (volume_step + 1) % (VOLUME_STEP_MAX + 1);
        if(volume_step != 0) last_volume_step = volume_step;
        radio_apply_volume_step();
    } else if(detected_chip == RadioChipTEA5767) {
        // TEA5767 STATUS bytes have no mute bit — track state ourselves.
        // STATUS byte 0 bits[5:0] = PLL[13:8] and STATUS byte 1 = PLL[7:0]
        // share the same bit positions as WRITE registers, so we can copy just
        // those bits into a clean write buffer — avoids ever writing PLL=0 to chip.
        radio_muted = !radio_muted;
        uint8_t status[5] = {0};
        tea5767_read_registers(status);
        uint8_t buffer[5];
        buffer[0] = status[0] & 0x3F;  // PLL[13:8], MUTE=0, SM=0
        buffer[1] = status[1];          // PLL[7:0]
        buffer[2] = 0xB0;               // SUD=1, SSL=01, HLSI=1, MS=0
        buffer[3] = REG_4_XTAL | REG_4_SMUTE;
        buffer[4] = REG_5_PLLREF | REG_5_DTC;
        apply_audio_mode(buffer, app_settings.audio_mode);
        tea5767_set_mute(buffer, radio_muted);
    }
    // Invalidate display cache so status line updates immediately
    cached_info_valid = false;
}

// OK hold: Si4703 mutes instantly; holding again restores the previous volume.
// TEA5767 has no volume register, so hold is the same mute toggle as a press.
static void radio_ok_held(void) {
    if(detected_chip == RadioChipSi4703) {
        if(volume_step == 0) {
            volume_step = last_volume_step;
        } else {
            last_volume_step = volume_step;
            volume_step = 0;
        }
        radio_apply_volume_step();
        cached_info_valid = false;
    } else if(detected_chip == RadioChipTEA5767) {
        radio_ok_pressed();
    }
}

// Persists tuner position across fast button presses so the cache miss never resets to 88
static float tuner_tracked_freq = 95.0f;

static void radio_step_frequency(bool up) {
    // Sync from cache when valid; otherwise keep accumulating on tuner_tracked_freq
    if(cached_info_valid) tuner_tracked_freq = cached_info.frequency;
    tuner_tracked_freq += up ? 0.1f : -0.1f;
    if(tuner_tracked_freq < 87.5f) tuner_tracked_freq = 87.5f;
    if(tuner_tracked_freq > 108.0f) tuner_tracked_freq = 108.0f;
    radio_set_frequency_from_mhz(tuner_tracked_freq);
    cached_info_valid = false;
}

static void radio_sleep_or_unmute_on_exit(void) {
    if(app_settings.mute_on_exit) {
        if(detected_chip == RadioChipSi4703) {
            si4703_sleep(si4703_registers);
        } else if(detected_chip == RadioChipTEA5767) {
            uint8_t buffer[5];
            tea5767_sleep(buffer);
        }
    } else {
        if(detected_chip == RadioChipSi4703) {
            si4703_set_mute(si4703_registers, false);
        } else if(detected_chip == RadioChipTEA5767) {
            tea5767_MuteOff();
        }
    }
}

// Fast recovery: re-init the already-powered chip without a full GPIO powerup.
// Called after RADIO_FAIL_THRESHOLD consecutive I2C failures.
static void radio_try_fast_recover(void) {
    FURI_LOG_W(TAG, "Attempting fast recovery for %s...", radio_get_chip_name());

    if(detected_chip == RadioChipSi4703) {
        si4703_reset_state();
        // Try I2C only — no GPIO powerup (chip may still be powered after a glitch)
        if(si4703_test_i2c_communication()) {
            FURI_LOG_I(TAG, "Si4703 still on bus, re-initializing");
            si4703_init(si4703_registers);
        } else {
            FURI_LOG_W(TAG, "Si4703 gone from bus, marking lost");
            detected_chip = RadioChipNone;
        }
    } else if(detected_chip == RadioChipTEA5767) {
        if(tea5767_is_device_ready()) {
            FURI_LOG_I(TAG, "TEA5767 still on bus, re-initializing");
            tea5767_init(tea5767_registers);
        } else {
            FURI_LOG_W(TAG, "TEA5767 gone from bus, marking lost");
            detected_chip = RadioChipNone;
        }
    }
}

static void radio_service_connection(NotificationApp* notifications) {
    const uint32_t now = furi_get_tick();
    if(detected_chip != RadioChipNone) return;

    if(!radio_disconnect_notified) {
        FURI_LOG_W(TAG, "Tuner disconnected; automatic reconnect enabled");
        if(notifications) notification_message(notifications, &sequence_error);
        radio_disconnect_notified = true;
    }

    if((now - radio_last_reconnect_tick) < RADIO_RECONNECT_INTERVAL_MS) return;
    radio_last_reconnect_tick = now;
    si4703_reset_state();

    if(detect_radio_chip() && radio_init()) {
        FURI_LOG_I(TAG, "Tuner restored: %s", radio_get_chip_name());
        radio_fail_count = 0;
        cached_info_valid = false;
        radio_disconnect_notified = false;
        if(notifications) notification_message(notifications, &sequence_success);
    } else {
        detected_chip = RadioChipNone;
    }
}

static bool radio_get_info_unified(struct RADIO_INFO* out_info) {
    if(!out_info) return false;

    bool ok = false;

    if(detected_chip == RadioChipSi4703) {
        SI4703_RADIO_INFO si_info;
        ok = si4703_get_radio_info(si4703_registers, &si_info);
        if(ok) {
            radio_fail_count = 0;
            out_info->frequency = si_info.frequency;
            out_info->signalLevel = si_info.signalLevel;
            out_info->stereo = si_info.stereo;
            out_info->muted = si_info.muted;
            strncpy(out_info->signalQuality, si_info.signalQuality, sizeof(out_info->signalQuality));
            out_info->signalQuality[sizeof(out_info->signalQuality) - 1] = '\0';
        }
    } else if(detected_chip == RadioChipTEA5767) {
        uint8_t buffer[5];
        ok = tea5767_get_radio_info(buffer, out_info);
        if(ok) {
            // TEA5767 STATUS has no mute-state bit — use our tracked value
            out_info->muted = radio_muted;
            radio_fail_count = 0;
        }
    }

    if(!ok) {
        radio_fail_count++;
        if(radio_fail_count >= RADIO_FAIL_THRESHOLD) {
            radio_fail_count = 0;
            radio_try_fast_recover();
        }
    }

    return ok;
}

// Apply audio mode to TEA5767
void apply_audio_mode(uint8_t* buffer, AudioMode mode) {
    if(!buffer) return;
    
    switch(mode) {
        case AUDIO_MODE_STEREO:
            buffer[REG_3] &= ~REG_3_MS;  // Clear mono bit for stereo
            buffer[REG_3] &= ~REG_3_MR;  // Unmute right
            buffer[REG_3] &= ~REG_3_ML;  // Unmute left
            break;
        case AUDIO_MODE_MONO_LEFT:
            buffer[REG_3] |= REG_3_MS;   // Set mono bit
            buffer[REG_3] |= REG_3_MR;   // Mute right channel
            buffer[REG_3] &= ~REG_3_ML;  // Unmute left
            break;
        case AUDIO_MODE_MONO_RIGHT:
            buffer[REG_3] |= REG_3_MS;   // Set mono bit
            buffer[REG_3] |= REG_3_ML;   // Mute left channel
            buffer[REG_3] &= ~REG_3_MR;  // Unmute right
            break;
    }
}

// Apply seek strength to TEA5767
void apply_seek_strength(uint8_t* buffer, uint8_t strength) {
    if(!buffer) return;
    
    // Clamp strength to 1-15
    if(strength < 1) strength = 1;
    if(strength > 15) strength = 15;
    
    // TEA5767 uses SSL bits (6-5) in REG_3 for search stop level
    // Map 1-15 to 0-3 (4 levels available)
    uint8_t ssl_level;
    if(strength <= 4) ssl_level = 0;      // Low (1-4)
    else if(strength <= 8) ssl_level = 1;  // Mid-low (5-8)
    else if(strength <= 12) ssl_level = 2; // Mid-high (9-12)
    else ssl_level = 3;                    // High (13-15)
    
    // Clear SSL bits and set new value
    buffer[REG_3] &= ~REG_3_SSL;
    buffer[REG_3] |= (ssl_level << 5);
}

// Load app settings from SD card
bool load_app_settings(Storage* storage) {
    File* file = storage_file_alloc(storage);
    bool result = false;
    
    if(storage_file_open(file, APP_DATA_PATH("settings.cfg"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint16_t bytes_read = storage_file_read(file, &app_settings, sizeof(AppSettings));
        if(bytes_read == sizeof(AppSettings)) {
            // Validate settings
            if(app_settings.seek_strength < 1) app_settings.seek_strength = 1;
            if(app_settings.seek_strength > 15) app_settings.seek_strength = 15;
            if(app_settings.audio_mode > AUDIO_MODE_MONO_RIGHT) app_settings.audio_mode = AUDIO_MODE_STEREO;
            if(app_settings.region_index >= NUM_REGIONS) app_settings.region_index = 0;
            if(app_settings.start_volume >= NUM_START_VOLUME_OPTIONS) app_settings.start_volume = 3;
            if(app_settings.last_volume_step < 1 || app_settings.last_volume_step > VOLUME_STEP_MAX)
                app_settings.last_volume_step = 1;
            if(app_settings.last_frequency < 87.5f || app_settings.last_frequency > 108.0f)
                app_settings.last_frequency = 0.0f;
            result = true;
        }
        storage_file_close(file);
    }
    
    storage_file_free(file);
    return result;
}

// Save app settings to SD card
bool save_app_settings(Storage* storage) {
    File* file = storage_file_alloc(storage);
    bool result = false;
    
    // Ensure directory exists
    storage_simply_mkdir(storage, APP_DATA_PATH(""));
    
    if(storage_file_open(file, APP_DATA_PATH("settings.cfg"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        uint16_t bytes_written = storage_file_write(file, &app_settings, sizeof(AppSettings));
        result = (bytes_written == sizeof(AppSettings));
        storage_file_close(file);
    }
    
    storage_file_free(file);
    return result;
}

// Free allocated station memory
void free_stations() {
    if(stations != NULL) {
        free(stations);
        stations = NULL;
        num_stations = 0;
    }
}

// Scan SD card for .txt files and populate regions
void scan_region_files(Storage* storage) {
    FURI_LOG_I(TAG, "Scanning for region files...");
    NUM_REGIONS = 0;
    
    // Ensure directory exists first
    storage_simply_mkdir(storage, APP_DATA_PATH(""));
    
    // Try direct file check first as fallback
    const char* default_files[] = {"toronto.txt", "usa.txt", "europe.txt", "custom.txt"};
    const char* default_names[] = {"Toronto", "USA", "Europe", "Custom"};
    
    for(size_t i = 0; i < 4; i++) {
        char filepath[128];
        snprintf(filepath, sizeof(filepath), APP_DATA_PATH("%s"), default_files[i]);
        
        if(storage_file_exists(storage, filepath)) {
            strncpy(regions[NUM_REGIONS].filename, default_files[i], sizeof(regions[NUM_REGIONS].filename) - 1);
            regions[NUM_REGIONS].filename[sizeof(regions[NUM_REGIONS].filename) - 1] = '\0';
            strncpy(regions[NUM_REGIONS].name, default_names[i], sizeof(regions[NUM_REGIONS].name) - 1);
            regions[NUM_REGIONS].name[sizeof(regions[NUM_REGIONS].name) - 1] = '\0';
            FURI_LOG_I(TAG, "Found file %s, added region %lu: %s", filepath, NUM_REGIONS, regions[NUM_REGIONS].name);
            NUM_REGIONS++;
        } else {
            FURI_LOG_W(TAG, "File does not exist: %s", filepath);
        }
    }
    
    if(NUM_REGIONS > 0) {
        regions_scanned_from_sd = true;
        FURI_LOG_I(TAG, "Total regions found: %lu", NUM_REGIONS);
    } else {
        FURI_LOG_E(TAG, "No region files found on SD card");
    }
}

// Populate default regions as fallback
void populate_default_regions() {
    NUM_REGIONS = 4;
    regions_scanned_from_sd = false;
    
    strncpy(regions[0].name, "Toronto", sizeof(regions[0].name) - 1);
    strncpy(regions[0].filename, "toronto.txt", sizeof(regions[0].filename) - 1);
    
    strncpy(regions[1].name, "USA", sizeof(regions[1].name) - 1);
    strncpy(regions[1].filename, "usa.txt", sizeof(regions[1].filename) - 1);
    
    strncpy(regions[2].name, "Europe", sizeof(regions[2].name) - 1);
    strncpy(regions[2].filename, "europe.txt", sizeof(regions[2].filename) - 1);
    
    strncpy(regions[3].name, "Custom", sizeof(regions[3].name) - 1);
    strncpy(regions[3].filename, "custom.txt", sizeof(regions[3].filename) - 1);
    
    FURI_LOG_I(TAG, "Using default regions: %lu", NUM_REGIONS);
}

// Create default station files if they don't exist
void create_default_region_files(Storage* storage) {
    // Create app data directory
    storage_simply_mkdir(storage, APP_DATA_PATH(""));
    
    // Toronto stations (default)
    const char* toronto_stations = 
        "88.1|CIND Indie 88\n"
        "88.9|CIRV Portuguese\n"
        "89.5|CIUT FM\n"
        "90.3|Ici Musique\n"
        "91.1|JAZZ.FM91\n"
        "92.5|KiSS Radio\n"
        "93.5|93.5 Today\n"
        "94.1|CBC Music\n"
        "95.9|KX96\n"
        "96.3|Classical 96.3\n"
        "97.3|Boom 97.3\n"
        "98.1|CHFI\n"
        "98.7|Flow 98.7\n"
        "99.1|CBC Radio 1\n"
        "99.9|99.9 Toronto\n"
        "100.7|CHIN 100.7\n"
        "101.3|CMR Diversity\n"
        "102.1|102.1 Edge\n"
        "103.5|Z103.5\n"
        "103.9|Proud FM\n"
        "104.5|CHUM\n"
        "105.5|VIBE1055 FM\n"
        "106.5|106.5 Elmnt\n"
        "107.1|Q107\n";
    
    // USA stations
    const char* usa_stations = 
        "88.5|NPR\n"
        "89.9|WNYC\n"
        "91.5|KUOW Seattle\n"
        "92.3|WXRT Chicago\n"
        "93.3|KTCL Denver\n"
        "94.7|KBCO Boulder\n"
        "95.5|KLOS Los Angeles\n"
        "96.5|WKLH Milwaukee\n"
        "97.1|KBPI Denver\n"
        "98.7|WRCH Hartford\n"
        "99.5|WGAR Cleveland\n"
        "100.3|KILT Houston\n"
        "101.1|WCBS New York\n"
        "102.7|KIIS Los Angeles\n"
        "103.5|WGRR Cincinnati\n"
        "104.3|KRBE Houston\n"
        "105.1|WMYX Milwaukee\n"
        "106.7|KROQ Los Angeles\n"
        "107.9|WNEW Washington\n";
    
    // Europe stations
    const char* europe_stations = 
        "88.0|BBC Radio 2\n"
        "89.1|BBC Radio 1\n"
        "90.2|Classic FM\n"
        "91.3|Radio France\n"
        "92.4|RDS Rome\n"
        "93.5|SWR3 Germany\n"
        "94.6|Radio 1 Spain\n"
        "95.8|RTE Ireland\n"
        "97.4|Radio 538 NL\n"
        "98.8|Absolute Radio\n"
        "100.0|BBC Radio London\n"
        "101.9|Capital FM\n"
        "103.5|Heart FM\n"
        "105.4|Radio X\n"
        "106.2|Smooth Radio\n"
        "107.8|Radio Monte Carlo\n";
    
    // Custom template
    const char* custom_stations = 
        "88.5|My Station 1\n"
        "95.5|My Station 2\n"
        "101.5|My Station 3\n"
        "# Edit this file to add your own stations\n"
        "# Format: frequency|station_name\n"
        "# Example: 99.5|Cool FM\n";
    
    // Write files if they don't exist
    File* file;
    
    file = storage_file_alloc(storage);
    if(!storage_file_exists(storage, APP_DATA_PATH("toronto.txt"))) {
        if(storage_file_open(file, APP_DATA_PATH("toronto.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            storage_file_write(file, toronto_stations, strlen(toronto_stations));
            storage_file_close(file);
        }
    }
    
    if(!storage_file_exists(storage, APP_DATA_PATH("usa.txt"))) {
        if(storage_file_open(file, APP_DATA_PATH("usa.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            storage_file_write(file, usa_stations, strlen(usa_stations));
            storage_file_close(file);
        }
    }
    
    if(!storage_file_exists(storage, APP_DATA_PATH("europe.txt"))) {
        if(storage_file_open(file, APP_DATA_PATH("europe.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            storage_file_write(file, europe_stations, strlen(europe_stations));
            storage_file_close(file);
        }
    }
    
    if(!storage_file_exists(storage, APP_DATA_PATH("custom.txt"))) {
        if(storage_file_open(file, APP_DATA_PATH("custom.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            storage_file_write(file, custom_stations, strlen(custom_stations));
            storage_file_close(file);
        }
    }
    
    storage_file_free(file);
}

// Load stations from a file
bool load_stations_from_file(Storage* storage, const char* filename) {
    char filepath[128];
    snprintf(filepath, sizeof(filepath), APP_DATA_PATH("%s"), filename);
    
    FURI_LOG_I(TAG, "Attempting to load stations from: %s", filepath);
    
    // Check if file exists
    if(!storage_file_exists(storage, filepath)) {
        FURI_LOG_E(TAG, "File does not exist: %s", filepath);
        return false;
    }
    
    // Free existing stations
    free_stations();
    
    File* file = storage_file_alloc(storage);
    if(!storage_file_open(file, filepath, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E(TAG, "Failed to open file: %s", filepath);
        storage_file_free(file);
        return false;
    }
    
    uint64_t file_size = storage_file_size(file);
    FURI_LOG_I(TAG, "File opened successfully, size: %llu bytes", file_size);
    
    if(file_size == 0 || file_size > 32768) {
        FURI_LOG_E(TAG, "Invalid file size: %llu", file_size);
        storage_file_close(file);
        storage_file_free(file);
        return false;
    }
    
    // Allocate buffer for entire file
    char* file_content = malloc(file_size + 1);
    if(!file_content) {
        FURI_LOG_E(TAG, "Failed to allocate memory for file content");
        storage_file_close(file);
        storage_file_free(file);
        return false;
    }
    
    // Read entire file
    uint16_t bytes_read = storage_file_read(file, file_content, file_size);
    file_content[bytes_read] = '\0';
    
    storage_file_close(file);
    storage_file_free(file);
    
    FURI_LOG_I(TAG, "Read %u bytes from file", bytes_read);
    
    // Log first 100 characters for debugging
    if(bytes_read > 0) {
        char preview[101];
        size_t preview_len = bytes_read < 100 ? bytes_read : 100;
        memcpy(preview, file_content, preview_len);
        preview[preview_len] = '\0';
        FURI_LOG_I(TAG, "File preview: %s", preview);
    }
    
    // Allocate temporary station array
    Station* temp_stations = malloc(sizeof(Station) * MAX_STATIONS);
    if(!temp_stations) {
        free(file_content);
        return false;
    }
    
    uint32_t count = 0;
    char* line_start = file_content;
    char* line_end;
    
    FURI_LOG_I(TAG, "Starting line-by-line parsing...");
    
    // Parse line by line
    while(line_start < file_content + bytes_read && count < MAX_STATIONS) {
        // Find end of line
        line_end = strchr(line_start, '\n');
        if(!line_end) {
            line_end = file_content + bytes_read;
        }
        
        // Copy line to buffer
        size_t line_len = line_end - line_start;
        if(line_len > 0 && line_len < MAX_LINE_LENGTH) {
            char line[MAX_LINE_LENGTH];
            memcpy(line, line_start, line_len);
            line[line_len] = '\0';
            
            // Remove carriage return if present
            if(line_len > 0 && line[line_len - 1] == '\r') {
                line[line_len - 1] = '\0';
                line_len--;
            }
            
            FURI_LOG_D(TAG, "Processing line (%u chars): '%s'", (unsigned int)line_len, line);
            
            // Skip comments and empty lines
            if(line_len > 0 && line[0] != '#') {
                // Parse line: frequency|name
                char* separator = strchr(line, '|');
                if(separator != NULL) {
                    *separator = '\0';
                    FURI_LOG_D(TAG, "Found separator, freq part: '%s', name part: '%s'", line, separator + 1);
                    float freq = 0.0f;
                    if(parse_frequency(line, &freq)) {
                        FURI_LOG_D(TAG, "Parsed frequency: %.1f", (double)freq);
                        if(freq >= 87.5f && freq <= 108.0f) {
                            temp_stations[count].frequency = freq;
                            // Skip leading whitespace in name
                            char* name_start = separator + 1;
                            while(*name_start == ' ' || *name_start == '\t') name_start++;
                            strncpy(temp_stations[count].name, name_start, MAX_STATION_NAME - 1);
                            temp_stations[count].name[MAX_STATION_NAME - 1] = '\0';
                            FURI_LOG_I(TAG, "Station %lu: %.1f - %s", count, (double)freq, temp_stations[count].name);
                            count++;
                        } else {
                            FURI_LOG_W(TAG, "Frequency out of range: %.1f", (double)freq);
                        }
                    } else {
                        FURI_LOG_W(TAG, "Failed to parse frequency from: '%s'", line);
                    }
                } else {
                    FURI_LOG_D(TAG, "No separator in line");
                }
            } else {
                FURI_LOG_D(TAG, "Skipping comment/empty line");
            }
        }
        
        // Move to next line
        line_start = line_end + 1;
    }
    
    free(file_content);
    
    FURI_LOG_I(TAG, "Parsed %lu stations from file", count);
    
    if(count > 0) {
        // Allocate final station array with exact size
        stations = malloc(sizeof(Station) * count);
        if(stations) {
            memcpy(stations, temp_stations, sizeof(Station) * count);
            num_stations = count;
            FURI_LOG_I(TAG, "Successfully loaded %lu stations", num_stations);
        } else {
            FURI_LOG_E(TAG, "Failed to allocate memory for stations");
        }
    } else {
        FURI_LOG_W(TAG, "No valid stations found in file");
    }
    
    free(temp_stations);
    return stations != NULL && num_stations > 0;
}

// Load default stations for a region
void load_default_stations_for_region(Storage* storage, uint32_t region_index) {
    if(region_index < NUM_REGIONS) {
        FURI_LOG_I(TAG, "Loading stations for region %lu: %s", region_index, regions[region_index].name);
        current_region_index = region_index;
        if(!load_stations_from_file(storage, regions[region_index].filename)) {
            // If file doesn't exist, create defaults
            FURI_LOG_W(TAG, "File load failed, creating default files");
            create_default_region_files(storage);
            load_stations_from_file(storage, regions[region_index].filename);
        }
        FURI_LOG_I(TAG, "Final station count: %lu", num_stations);
    }
}

// Function prototypes for forward declarations
void elements_button_top_left(Canvas* canvas, const char* str);
void elements_button_top_right(Canvas* canvas, const char* str);
//lib can only do bottom left/right
void elements_button_top_left(Canvas* canvas, const char* str) {
    const uint8_t button_height = 10;
    const uint8_t vertical_offset = 3;
    const uint8_t horizontal_offset = 3;
    // You may need to declare or pass 'button_width' here.
    const uint8_t string_width = canvas_string_width(canvas, str);
    // 'button_width' should be declared or passed here.
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
void elements_button_top_right(Canvas* canvas, const char* str) {
    const uint8_t button_height = 10;
    const uint8_t vertical_offset = 3;
    const uint8_t horizontal_offset = 3;
    // You may need to declare or pass 'button_width' here.
    const uint8_t string_width = canvas_string_width(canvas, str);

    // 'button_width' should be declared or passed here.
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
    MyAppSubmenuIndexConfigure,
    MyAppSubmenuIndexFlipTheWorld,
    MyAppSubmenuIndexTuner,
    MyAppSubmenuIndexSettings,
    MyAppSubmenuIndexFileViewer,
    MyAppSubmenuIndexAbout,
} MyAppSubmenuIndex;

typedef enum {
    MyAppViewSubmenu,
    MyAppViewConfigure,
    MyAppViewFlipTheWorld,
    MyAppViewTuner,
    MyAppViewSettings,
    MyAppViewFileViewer,
    MyAppViewAbout,
} MyAppView;

// Define a struct to hold the application's components
typedef struct {
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;
    Submenu* submenu;
    VariableItemList* variable_item_list_config;
    VariableItemList* variable_item_list_settings;
    View* view_flip_the_world;
    View* view_tuner;
    View* view_file_viewer;
    Widget* widget_about;
    Storage* storage;
} MyApp;

#define RADIO_HEARTBEAT_EVENT 1U

static bool my_app_custom_event_callback(void* context, uint32_t event) {
    MyApp* app = context;
    if(event != RADIO_HEARTBEAT_EVENT) return false;
    radio_service_connection(app->notifications);
    view_commit_model(app->view_flip_the_world, true);
    view_commit_model(app->view_tuner, true);
    return true;
}

static void my_app_tick_event_callback(void* context) {
    MyApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, RADIO_HEARTBEAT_EVENT);
}

// Define a model struct for your application
typedef struct {
    uint32_t current_station_index;
    uint8_t volume_index;
} MyModel;

// File viewer model
typedef struct {
    uint32_t current_region;
    uint32_t current_line;
    uint32_t total_lines;
    char lines[50][MAX_LINE_LENGTH];
} FileViewerModel;

// Callback for navigation events

uint32_t my_app_navigation_exit_callback(void* context) {
    UNUSED(context);
    radio_sleep_or_unmute_on_exit();
    
    return VIEW_NONE;
}

// Callback for navigating to the submenu
uint32_t my_app_navigation_submenu_callback(void* context) {
    UNUSED(context);
    return MyAppViewSubmenu;
}

// Callback for handling submenu selections
void my_app_submenu_callback(void* context, uint32_t index) {
    MyApp* app = (MyApp*)context;
    switch(index) {
    case MyAppSubmenuIndexConfigure:
        view_dispatcher_switch_to_view(app->view_dispatcher, MyAppViewConfigure);
        break;
    case MyAppSubmenuIndexFlipTheWorld:
        view_dispatcher_switch_to_view(app->view_dispatcher, MyAppViewFlipTheWorld);
        break;
    case MyAppSubmenuIndexTuner:
        view_dispatcher_switch_to_view(app->view_dispatcher, MyAppViewTuner);
        break;
    case MyAppSubmenuIndexSettings:
        view_dispatcher_switch_to_view(app->view_dispatcher, MyAppViewSettings);
        break;
    case MyAppSubmenuIndexFileViewer:
        view_dispatcher_switch_to_view(app->view_dispatcher, MyAppViewFileViewer);
        break;
    case MyAppSubmenuIndexAbout:
        view_dispatcher_switch_to_view(app->view_dispatcher, MyAppViewAbout);
        break;
    default:
        break;
    }
}

bool my_app_view_input_callback(InputEvent* event, void* context) {
    UNUSED(context);
    
    if(event->type == InputTypeShort && event->key == InputKeyLeft) {
        radio_seek_from_current(false);
        current_volume = 0;
        return true;
    } else if(event->type == InputTypeShort && event->key == InputKeyRight) {
        radio_seek_from_current(true);
        current_volume = 0;
        return true;
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        radio_ok_pressed();
        current_volume = (current_volume == 0) ? 1 : 0;
        return true;
    } else if(event->type == InputTypeLong && event->key == InputKeyOk) {
        radio_ok_held();
        return true;
    } else if(event->type == InputTypeShort && event->key == InputKeyUp) {
        // Increment the current station index and loop back if at the end
        if(num_stations > 0) {
            current_station_index = (current_station_index + 1) % num_stations;
            // Set the new frequency with audio mode
            radio_set_frequency_from_mhz(stations[current_station_index].frequency);
            current_volume = 0;
        }
        return true;
    } else if(event->type == InputTypeShort && event->key == InputKeyDown) {
        // Decrement the current station index and loop back if at the beginning
        if(num_stations > 0) {
            if(current_station_index == 0) {
                current_station_index = num_stations - 1;
            } else {
                current_station_index--;
            }
            // Set the new frequency with audio mode
            radio_set_frequency_from_mhz(stations[current_station_index].frequency);
            current_volume = 0;
        }
        return true;
    }
    return false;
}

// Callback for handling region changes
void my_app_region_change(VariableItem* item) {
    MyApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    
    // Load stations for selected region
    load_default_stations_for_region(app->storage, index);
    
    // Reset station index
    current_station_index = 0;
    
    // Update settings
    app_settings.region_index = index;
    save_app_settings(app->storage);
    
    // Update display
    variable_item_set_current_value_text(item, regions[index].name);
    
    // Set first station if available
    if(num_stations > 0) {
        radio_set_frequency_from_mhz(stations[0].frequency);
    }
}

// Callback for handling frequency changes
void my_app_frequency_change(VariableItem* item) {
    MyApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    MyModel* model = view_get_model(app->view_flip_the_world);
    model->current_station_index = index;

    // Display the selected frequency value as text
    if(num_stations > 0 && index < num_stations) {
        char frequency_display[16];
        snprintf(frequency_display, sizeof(frequency_display), "%.1f MHz", (double)stations[index].frequency);
        variable_item_set_current_value_text(item, frequency_display);
    }
}

// Callback for handling volume changes
void my_app_volume_change(VariableItem* item) {
    MyApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(
        item, volume_names[index]); // Display the selected volume as text
    MyModel* model = view_get_model(app->view_flip_the_world);
    model->volume_index = index;
}

// Callback for mute on exit setting
void my_app_mute_on_exit_change(VariableItem* item) {
    MyApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app_settings.mute_on_exit = (index == 1);
    variable_item_set_current_value_text(item, app_settings.mute_on_exit ? "On" : "Off");
    save_app_settings(app->storage);
}

// Callback for seek strength setting
void my_app_seek_strength_change(VariableItem* item) {
    MyApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app_settings.seek_strength = index + 1; // 1-15
    
    char strength_text[8];
    snprintf(strength_text, sizeof(strength_text), "%d", app_settings.seek_strength);
    variable_item_set_current_value_text(item, strength_text);
    save_app_settings(app->storage);
}

// Callback for audio mode setting
void my_app_audio_mode_change(VariableItem* item) {
    MyApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app_settings.audio_mode = (AudioMode)index;

    variable_item_set_current_value_text(item, audio_mode_names[index]);
    save_app_settings(app->storage);

    // Apply audio mode immediately — for TEA5767 always use a clean write buffer
    if(detected_chip == RadioChipSi4703) {
        si4703_set_stereo(si4703_registers, app_settings.audio_mode == AUDIO_MODE_STEREO);
    } else if(detected_chip == RadioChipTEA5767) {
        float current_freq = tea5767_GetFreq();
        if(current_freq < 87.5f || current_freq > 108.0f) current_freq = 95.0f;
        uint8_t buffer[5];
        if(tea5767_init(buffer)) {
            int freq_10khz = (int)(current_freq * 100.0f);
            tea5767_set_frequency(buffer, freq_10khz);
            apply_audio_mode(buffer, app_settings.audio_mode);
            tea5767_set_mute(buffer, radio_muted);
            tea5767_write_registers(buffer);
        }
    }
}

// Callback for start volume setting (Low/Med/High/Last)
void my_app_start_volume_change(VariableItem* item) {
    MyApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app_settings.start_volume = index;
    variable_item_set_current_value_text(item, start_volume_names[index]);
    save_app_settings(app->storage);

    // Fixed levels apply immediately for instant feedback; "Last" leaves volume alone
    if(index <= 2 && detected_chip == RadioChipSi4703) {
        volume_step = index + 1;
        last_volume_step = volume_step;
        radio_apply_volume_step();
        cached_info_valid = false;
    }
}

// Callback for resume-last-station setting
void my_app_resume_station_change(VariableItem* item) {
    MyApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app_settings.resume_station = (index == 1);
    variable_item_set_current_value_text(item, app_settings.resume_station ? "On" : "Off");
    save_app_settings(app->storage);
}

// Callback for chip selection override
void my_app_chip_select_change(VariableItem* item) {
    MyApp* app = variable_item_get_context(item);
    (void)app;
    uint8_t index = variable_item_get_current_value_index(item);
    app_settings.chip_select = (ChipSelect)index;
    variable_item_set_current_value_text(item, chip_select_names[index]);
    save_app_settings(app->storage);

    // Re-detect with the new preference and re-initialize
    cached_info_valid = false;
    radio_fail_count = 0;
    detected_chip = RadioChipNone;
    si4703_reset_state();
    if(detect_radio_chip()) {
        radio_init();
    }
}

// Load file content for viewer
void load_file_content(MyApp* app, uint32_t region_index) {
    if(region_index >= NUM_REGIONS) return;
    
    FileViewerModel* model = view_get_model(app->view_file_viewer);
    model->current_region = region_index;
    model->current_line = 0;
    model->total_lines = 0;
    
    char filepath[128];
    snprintf(filepath, sizeof(filepath), APP_DATA_PATH("%s"), regions[region_index].filename);
    
    File* file = storage_file_alloc(app->storage);
    if(storage_file_open(file, filepath, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint64_t file_size = storage_file_size(file);
        if(file_size > 0 && file_size < 32768) {
            char* content = malloc(file_size + 1);
            if(content) {
                uint16_t bytes_read = storage_file_read(file, content, file_size);
                content[bytes_read] = '\0';
                
                // Parse into lines
                char* line_start = content;
                char* line_end;
                
                while(line_start < content + bytes_read && model->total_lines < 50) {
                    line_end = strchr(line_start, '\n');
                    if(!line_end) line_end = content + bytes_read;
                    
                    size_t line_len = line_end - line_start;
                    if(line_len > 0 && line_len < MAX_LINE_LENGTH) {
                        memcpy(model->lines[model->total_lines], line_start, line_len);
                        model->lines[model->total_lines][line_len] = '\0';
                        
                        // Remove carriage return
                        if(line_len > 0 && model->lines[model->total_lines][line_len - 1] == '\r') {
                            model->lines[model->total_lines][line_len - 1] = '\0';
                        }
                        
                        model->total_lines++;
                    }
                    
                    line_start = line_end + 1;
                }
                
                free(content);
            }
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    
    view_commit_model(app->view_file_viewer, false);
}

// Callback when entering Listen Now view
void my_app_view_enter_callback(void* context) {
    MyApp* app = (MyApp*)context;
    FURI_LOG_I(TAG, "Entering Listen Now view");
    FURI_LOG_I(TAG, "Current num_stations: %lu, NUM_REGIONS: %lu, region_index: %lu", num_stations, NUM_REGIONS, app_settings.region_index);
    
    // Reset cached info so first draw triggers a refresh
    cached_info_valid = false;

    // Always reload stations for the selected region to ensure they are current
    if(NUM_REGIONS > 0 && app_settings.region_index < NUM_REGIONS) {
        FURI_LOG_I(TAG, "Reloading stations for region %lu: %s", app_settings.region_index, regions[app_settings.region_index].name);
        load_default_stations_for_region(app->storage, app_settings.region_index);
        FURI_LOG_I(TAG, "After reload: num_stations=%lu", num_stations);
        current_station_index = 0;

        if(num_stations > 0) {
            float target = stations[0].frequency;

            if(app_settings.resume_station) {
                // Prefer the live chip frequency (mid-session re-entry); fall back
                // to the frequency persisted from the previous run (fresh start).
                float resume = 0.0f;
                if(cached_info.frequency >= 87.5f && cached_info.frequency <= 108.0f) {
                    resume = cached_info.frequency;
                } else if(app_settings.last_frequency >= 87.5f && app_settings.last_frequency <= 108.0f) {
                    resume = app_settings.last_frequency;
                }
                if(resume > 0.0f) {
                    target = resume;
                    // Line up the preset index if the frequency matches one
                    for(uint32_t i = 0; i < num_stations; i++) {
                        if(fabsf(stations[i].frequency - resume) < 0.05f) {
                            current_station_index = i;
                            break;
                        }
                    }
                }
            }

            if(radio_set_frequency_from_mhz(target)) {
                FURI_LOG_I(TAG, "Tuning to %.1f MHz on view enter", (double)target);
            }
        }
    } else {
        FURI_LOG_W(TAG, "Invalid region configuration: NUM_REGIONS=%lu, region_index=%lu", NUM_REGIONS, app_settings.region_index);
    }
}

// File viewer draw callback
void file_viewer_draw_callback(Canvas* canvas, void* model) {
    FileViewerModel* fv_model = (FileViewerModel*)model;
    
    canvas_set_font(canvas, FontPrimary);
    if(NUM_REGIONS > 0 && fv_model->current_region < NUM_REGIONS) {
        canvas_draw_str(canvas, 4, 10, regions[fv_model->current_region].name);
    }
    
    canvas_set_font(canvas, FontSecondary);
    elements_button_left(canvas, "Prev");
    elements_button_right(canvas, "Next");
    
    // Draw lines (3 lines max to avoid button overlap)
    uint8_t y_pos = 22;
    uint8_t lines_per_screen = 3;
    
    for(uint32_t i = fv_model->current_line; i < fv_model->total_lines && i < fv_model->current_line + lines_per_screen; i++) {
        canvas_draw_str(canvas, 2, y_pos, fv_model->lines[i]);
        y_pos += 10;
    }
}

// File viewer input callback
bool file_viewer_input_callback(InputEvent* event, void* context) {
    MyApp* app = (MyApp*)context;
    
    if(event->type == InputTypeShort) {
        FileViewerModel* model = view_get_model(app->view_file_viewer);
        
        if(event->key == InputKeyLeft) {
            // Previous region
            if(NUM_REGIONS > 0) {
                if(model->current_region == 0) {
                    model->current_region = NUM_REGIONS - 1;
                } else {
                    model->current_region--;
                }
                view_commit_model(app->view_file_viewer, false);
                load_file_content(app, model->current_region);
            }
            return true;
        } else if(event->key == InputKeyRight) {
            // Next region
            if(NUM_REGIONS > 0) {
                model->current_region = (model->current_region + 1) % NUM_REGIONS;
                view_commit_model(app->view_file_viewer, false);
                load_file_content(app, model->current_region);
            }
            return true;
        } else if(event->key == InputKeyUp) {
            // Scroll up
            if(model->current_line > 0) {
                model->current_line--;
                view_commit_model(app->view_file_viewer, true);
            }
            return true;
        } else if(event->key == InputKeyDown) {
            // Scroll down
            if(model->current_line + 3 < model->total_lines) {
                model->current_line++;
                view_commit_model(app->view_file_viewer, true);
            }
            return true;
        }
    }
    
    return false;
}

// Top bar: [Scan- black button] [Presets ↑↓ center] [Scan+ black button]
static void draw_top_bar(Canvas* canvas) {
    canvas_set_font(canvas, FontSecondary);
    const uint8_t btn_w = 36;

    // Scan- black button (D-pad left)
    canvas_draw_box(canvas, 0, 0, btn_w, 12);
    canvas_draw_line(canvas, btn_w, 0, btn_w, 11);
    canvas_draw_line(canvas, btn_w + 1, 1, btn_w + 1, 11);
    canvas_invert_color(canvas);
    canvas_draw_icon_ex(canvas, 2, 2, &I_ButtonUp_7x4, IconRotation270); // left arrow 4w×7h
    canvas_draw_str(canvas, 7, 9, "Scan-");
    canvas_invert_color(canvas);

    // Scan+ black button (D-pad right)
    const uint8_t btn_x = 128 - btn_w;
    canvas_draw_box(canvas, btn_x, 0, btn_w, 12);
    canvas_draw_line(canvas, btn_x - 1, 0, btn_x - 1, 11);
    canvas_draw_line(canvas, btn_x - 2, 1, btn_x - 2, 11);
    canvas_invert_color(canvas);
    canvas_draw_str(canvas, btn_x + 2, 9, "Scan+");
    canvas_draw_icon_ex(canvas, 117, 2, &I_ButtonUp_7x4, IconRotation90); // right arrow — 119+7=126, safely on screen
    canvas_invert_color(canvas);

    // Presets center (D-pad up/down) — stacked ↑↓ icons + label, centered in 56px middle zone
    uint8_t pstr_w = canvas_string_width(canvas, "Presets");
    uint8_t group_w = 7 + 2 + pstr_w;
    uint8_t gx = 64 - group_w / 2;
    canvas_draw_icon(canvas, gx, 1, &I_ButtonUp_7x4);                     // ↑ (7×4) y=1
    canvas_draw_icon_ex(canvas, gx, 7, &I_ButtonUp_7x4, IconRotation180); // ↓ (7×4) y=7
    canvas_draw_str(canvas, gx + 9, 9, "Presets");
}


// ============================================================================
// Tuner View — visual station seeker (88–108 MHz dial with needle)
// ============================================================================

// Full-screen connection splash shown while the app probes for a tuner.
static void draw_connection_splash(Canvas* canvas) {
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 0, AlignCenter, AlignTop, "Connect FM Tuner");
    canvas_draw_line(canvas, 8, 12, 119, 12);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 22, "Si4703:");
    canvas_draw_str(canvas, 2, 30, "Pin 15 = SDIO | Pin 16 = SCLK");
    canvas_draw_str(canvas, 2, 38, "Pin 4 = RST (Si Only)");
    canvas_draw_str(canvas, 2, 47, "TEA5767:");
    canvas_draw_str(canvas, 2, 55, "Pin 15 = SDA | Pin 16 = SCL");
    canvas_draw_str(canvas, 2, 63, "3V3 = VCC | GND = GND");
}

static void tuner_draw_callback(Canvas* canvas, void* model) {
    (void)model;

    // Refresh cache if stale
    const uint32_t now_tick = furi_get_tick();
    if(!cached_info_valid || (now_tick - cached_info_last_tick) > radio_info_refresh_ms) {
        struct RADIO_INFO tmp;
        if(radio_get_info_unified(&tmp)) {
            cached_info = tmp;
            cached_info_valid = true;
        }
        cached_info_last_tick = now_tick;
    }

    // No chip — show same error screen as the main view, then bail
    if(detected_chip == RadioChipNone) {
        draw_connection_splash(canvas);
        return;
    }

    // Use cached chip frequency when fresh; fall back to tuner_tracked_freq (not 88.0)
    // so fast presses never cause the needle to snap back to the bottom of the dial.
    float freq = tuner_tracked_freq;
    if(cached_info_valid) {
        freq = cached_info.frequency;
        tuner_tracked_freq = freq; // keep tracker in sync
    }
    if(freq < 88.0f) freq = 88.0f;
    if(freq > 108.0f) freq = 108.0f;

    // --- Tuner dial (ruler at y=20, x=4..124, 6px/MHz) ---
    const uint8_t ruler_y = 20;
    const uint8_t x_min = 4;
    const uint8_t x_max = 124;

    canvas_draw_line(canvas, x_min, ruler_y, x_max, ruler_y);

    // Tick marks above the ruler
    for(int f = 88; f <= 108; f++) {
        uint8_t tx = (uint8_t)(x_min + (f - 88) * 6);
        if(f % 10 == 8 || f == 108) {
            canvas_draw_line(canvas, tx, ruler_y - 5, tx, ruler_y); // major: 5px
        } else if(f % 2 == 0) {
            canvas_draw_line(canvas, tx, ruler_y - 3, tx, ruler_y); // minor: 3px
        }
    }

    // Frequency labels below the ruler
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, ruler_y + 8, "88");
    canvas_draw_str(canvas, 59, ruler_y + 8, "98");
    canvas_draw_str(canvas, 109, ruler_y + 8, "108");

    // --- Needle ---
    uint8_t nx = (uint8_t)(x_min + (freq - 88.0f) * 6.0f);
    if(nx < x_min) nx = x_min;
    if(nx > x_max) nx = x_max;
    // Downward-pointing filled triangle at top of screen, stem to ruler
    canvas_draw_line(canvas, nx - 4, 0, nx + 4, 0);
    canvas_draw_line(canvas, nx - 3, 1, nx + 3, 1);
    canvas_draw_line(canvas, nx - 2, 2, nx + 2, 2);
    canvas_draw_line(canvas, nx - 1, 3, nx + 1, 3);
    canvas_draw_line(canvas, nx,     4, nx,     ruler_y + 1); // stem

    // --- Current frequency (large, centered) ---
    canvas_set_font(canvas, FontPrimary);
    char freq_str[16];
    snprintf(freq_str, sizeof(freq_str), "%.1f MHz", (double)freq);
    uint8_t fw = canvas_string_width(canvas, freq_str);
    canvas_draw_str(canvas, 64 - fw / 2, 38, freq_str);

    // --- Status ---
    canvas_set_font(canvas, FontSecondary);
    if(cached_info_valid) {
        char status_str[24];
        snprintf(status_str, sizeof(status_str), "%s  %s",
            radio_play_state_str(cached_info.muted),
            cached_info.stereo ? "Stereo" : "Mono");
        uint8_t sw = canvas_string_width(canvas, status_str);
        canvas_draw_str(canvas, 64 - sw / 2, 48, status_str);
    } else {
        canvas_draw_str(canvas, 2, 48, "Connecting...");
    }

    // --- Bottom hint bar: [← → Tune] [OK Vol/Mute] [↑↓ Seek] ---
    // elements_button_center draws the proper SDK center button (y=52..63, centered on x=64)
    elements_button_center(canvas, detected_chip == RadioChipSi4703 ? "Vol" : "Mute");

    // Left zone — ← → Tune (drawn after center button so they sit on the same row)
    canvas_draw_icon_ex(canvas, 3, 56, &I_ButtonUp_7x4, IconRotation270); // ← (4w×7h)
    canvas_draw_icon_ex(canvas, 4, 56, &I_ButtonUp_7x4, IconRotation90);  // → (4w×7h)
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 14, 63, "Tune");

    // Right zone — ↑↓ Seek, stacked icons
    canvas_draw_icon(canvas,    89, 54, &I_ButtonUp_7x4);                    // ↑ (7w×4h)
    canvas_draw_icon_ex(canvas, 89, 60, &I_ButtonUp_7x4, IconRotation180);   // ↓ (7w×4h)
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 98, 63, "Seek");
}

static bool tuner_input_callback(InputEvent* event, void* context) {
    (void)context;
    bool consumed = false;

    if(event->type == InputTypeLong && event->key == InputKeyOk) {
        radio_ok_held();
        return true;
    }

    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        switch(event->key) {
        case InputKeyLeft:
            radio_step_frequency(false);
            consumed = true;
            break;
        case InputKeyRight:
            radio_step_frequency(true);
            consumed = true;
            break;
        case InputKeyUp:
            radio_seek_from_current(true);
            cached_info_valid = false;
            consumed = true;
            break;
        case InputKeyDown:
            radio_seek_from_current(false);
            cached_info_valid = false;
            consumed = true;
            break;
        case InputKeyOk:
            if(event->type == InputTypeShort) {
                radio_ok_pressed();
                cached_info_valid = false;
            }
            consumed = true;
            break;
        default:
            break;
        }
    }
    return consumed;
}

// Callback for drawing the view
void my_app_view_draw_callback(Canvas* canvas, void* model) {
    (void)model;

    if(detected_chip == RadioChipNone) {
        draw_connection_splash(canvas);
        return;
    }

    // --- Top bar: [Scan-] [Presets ↑↓] [Scan+] ---
    draw_top_bar(canvas);

    // --- Bottom bar: Vol/Mute (standard SDK center button) ---
    elements_button_center(canvas, detected_chip == RadioChipSi4703 ? "Vol" : "Mute");

    // --- Refresh radio info at a controlled rate ---
    const uint32_t now_tick = furi_get_tick();
    if(!cached_info_valid || (now_tick - cached_info_last_tick) > radio_info_refresh_ms) {
        struct RADIO_INFO info;
        if(radio_get_info_unified(&info)) {
            cached_info = info;
            cached_info_valid = true;
            cached_info_last_tick = now_tick;
        } else {
            cached_info_last_tick = now_tick;
        }
    }

    // --- Content area (y=13 to y=52) ---
    canvas_set_font(canvas, FontPrimary);

    if(cached_info_valid) {
        struct RADIO_INFO info = cached_info;

        // Line 1 (y=22): Station name, truncated to ~18 chars, with index
        if(num_stations > 0 && current_station_index < num_stations) {
            const float diff = fabsf(stations[current_station_index].frequency - info.frequency);
            if(diff < 0.15f) {
                // Truncate station name to 14 chars so index always fits on 128px
                char name[15];
                strncpy(name, stations[current_station_index].name, 14);
                name[14] = '\0';
                snprintf(station_display, sizeof(station_display), "%s [%lu/%lu]",
                    name, current_station_index + 1, num_stations);
            } else {
                // Landed on an un-preset frequency after seeking
                snprintf(station_display, sizeof(station_display), "Scanned: %.1f MHz", (double)info.frequency);
            }
        } else {
            snprintf(station_display, sizeof(station_display),
                NUM_REGIONS > 0 ? "No stations loaded" : "No region file");
        }
        canvas_draw_str(canvas, 2, 22, station_display);

        // Line 2 (y=33): Frequency prominently
        snprintf(frequency_display, sizeof(frequency_display), "%.1f MHz", (double)info.frequency);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 33, frequency_display);

        // Line 3 (y=42): Mute/Play + Stereo/Mono status
        canvas_set_font(canvas, FontSecondary);
        snprintf(volume_display, sizeof(volume_display), "%s  %s",
            radio_play_state_str(info.muted),
            info.stereo ? "Stereo" : "Mono");
        canvas_draw_str(canvas, 2, 42, volume_display);

        // Line 4 (y=51): Signal quality + chip name, or "Seeking..." if busy
        bool is_seeking = (detected_chip == RadioChipSi4703 && si4703_is_busy());
        if(is_seeking) {
            snprintf(signal_display, sizeof(signal_display), "Seeking...");
        } else {
            snprintf(signal_display, sizeof(signal_display), "Sig: %s  [%s]",
                info.signalQuality, radio_get_chip_name());
        }
        canvas_draw_str(canvas, 2, 51, signal_display);

    } else {
        // No valid chip data — show error
        canvas_set_font(canvas, FontPrimary);
        if(detected_chip == RadioChipNone) {
            draw_connection_splash(canvas);
        } else {
            // Chip was detected but comms are failing (recovering)
            snprintf(station_display, sizeof(station_display), "%s: reconnecting", radio_get_chip_name());
            canvas_draw_str(canvas, 2, 22, station_display);
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 2, 33, "Check wiring...");
            canvas_draw_str(canvas, 2, 42, "SCL:16  SDA:15");
        }
    }
}

// Allocate memory for the application
MyApp* my_app_alloc() {
    MyApp* app = (MyApp*)malloc(sizeof(MyApp));
    if(!app) return NULL;
    memset(app, 0, sizeof(MyApp));
    Gui* gui = furi_record_open(RECORD_GUI);
    
    // Initialize storage
    app->storage = furi_record_open(RECORD_STORAGE);
    
    // Create default region files
    create_default_region_files(app->storage);
    
    // Scan for region files dynamically
    scan_region_files(app->storage);
    
    // Fallback to default regions if scan failed
    if(NUM_REGIONS == 0) {
        FURI_LOG_W(TAG, "No regions found via scan, using defaults");
        populate_default_regions();
    }
    
    FURI_LOG_I(TAG, "Final NUM_REGIONS: %lu", NUM_REGIONS);
    
    // Load app settings from SD card
    if(!load_app_settings(app->storage)) {
        // Settings file doesn't exist, create default
        save_app_settings(app->storage);
    }

    // Apply start-volume setting before the chip is initialized
    // (radio_init applies volume_step to the Si4703 after si4703_init)
    volume_step = (app_settings.start_volume <= 2) ? (uint8_t)(app_settings.start_volume + 1) :
                                                     app_settings.last_volume_step;
    last_volume_step = volume_step;
    
    // Validate region index
    if(app_settings.region_index >= NUM_REGIONS && NUM_REGIONS > 0) {
        app_settings.region_index = 0;
    }
    
    // Load stations for saved region
    if(NUM_REGIONS > 0) {
        FURI_LOG_I(TAG, "Loading stations for region index %lu", app_settings.region_index);
        load_default_stations_for_region(app->storage, app_settings.region_index);
        FURI_LOG_I(TAG, "Stations loaded: %lu", num_stations);
    } else {
        FURI_LOG_E(TAG, "No regions found!");
    }

    // Detect and initialize radio chip
    if(!detect_radio_chip()) {
        FURI_LOG_W(TAG, "No supported FM chip detected (TEA5767/Si4703)");
    } else {
        FURI_LOG_I(TAG, "Detected FM chip: %s", radio_get_chip_name());
        radio_init();

        // Resume the last tuned frequency right away so it applies no matter
        // which view the user opens first
        if(app_settings.resume_station && app_settings.last_frequency >= 87.5f &&
           app_settings.last_frequency <= 108.0f) {
            FURI_LOG_I(TAG, "Resuming last station: %.1f MHz", (double)app_settings.last_frequency);
            radio_set_frequency_from_mhz(app_settings.last_frequency);
            tuner_tracked_freq = app_settings.last_frequency;
        }
    }

    cached_info_valid = false;
    
    // Initialize the view dispatcher
    app->view_dispatcher = view_dispatcher_alloc();
    if(!app->view_dispatcher) {
        furi_record_close(RECORD_GUI);
        furi_record_close(RECORD_STORAGE);
        free_stations();
        free(app);
        return NULL;
    }
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, my_app_custom_event_callback);
    view_dispatcher_set_tick_event_callback(app->view_dispatcher, my_app_tick_event_callback, 250);
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);

    // Initialize the submenu
    app->submenu = submenu_alloc();
    submenu_add_item(app->submenu, "Listen Now", MyAppSubmenuIndexFlipTheWorld, my_app_submenu_callback, app);
    submenu_add_item(app->submenu, "Tuner", MyAppSubmenuIndexTuner, my_app_submenu_callback, app);
    submenu_add_item(app->submenu, "Settings", MyAppSubmenuIndexSettings, my_app_submenu_callback, app);
    submenu_add_item(app->submenu, "Station Files", MyAppSubmenuIndexFileViewer, my_app_submenu_callback, app);
    //submenu_add_item(app->submenu, "Config", MyAppSubmenuIndexConfigure, my_app_submenu_callback, app);
    submenu_add_item(app->submenu, "About", MyAppSubmenuIndexAbout, my_app_submenu_callback, app);
    view_set_previous_callback(submenu_get_view(app->submenu), my_app_navigation_exit_callback);
    view_dispatcher_add_view(app->view_dispatcher, MyAppViewSubmenu, submenu_get_view(app->submenu));
    view_dispatcher_switch_to_view(app->view_dispatcher, MyAppViewSubmenu);

    // Initialize the variable item list for configuration
    app->variable_item_list_config = variable_item_list_alloc();
    variable_item_list_reset(app->variable_item_list_config);

    // Add frequency configuration
    VariableItem* frequency_item = variable_item_list_add(app->variable_item_list_config, "Freq (MHz)", num_stations, my_app_frequency_change, app);    
    
    uint32_t current_station_index = 0;
    variable_item_set_current_value_index(frequency_item, current_station_index);
    // Add volume configuration
    VariableItem* volume_item = variable_item_list_add(app->variable_item_list_config, "Volume", COUNT_OF(volume_values), my_app_volume_change, app);
    uint8_t volume_index = 0;
    variable_item_set_current_value_index(volume_item, volume_index);
    view_set_previous_callback(variable_item_list_get_view(app->variable_item_list_config),my_app_navigation_submenu_callback);
    view_dispatcher_add_view(app->view_dispatcher, MyAppViewConfigure, variable_item_list_get_view(app->variable_item_list_config));
    
    // Initialize the settings menu
    app->variable_item_list_settings = variable_item_list_alloc();
    variable_item_list_reset(app->variable_item_list_settings);
    
    FURI_LOG_I(TAG, "Building settings menu with %lu regions", NUM_REGIONS);
    
    // Add region selection - always add it with at least 1 region
    if(NUM_REGIONS > 0) {
        // Ensure region_index is valid
        if(app_settings.region_index >= NUM_REGIONS) {
            app_settings.region_index = 0;
        }
        
        VariableItem* region_item = variable_item_list_add(
            app->variable_item_list_settings, 
            "Region", 
            NUM_REGIONS, 
            my_app_region_change, 
            app);
        variable_item_set_current_value_index(region_item, app_settings.region_index);
        variable_item_set_current_value_text(region_item, regions[app_settings.region_index].name);
        FURI_LOG_I(TAG, "Region selector added: index=%lu, name=%s", app_settings.region_index, regions[app_settings.region_index].name);
    } else {
        FURI_LOG_E(TAG, "CRITICAL: No regions available!");
    }
    
    // Add mute on exit setting
    VariableItem* mute_exit_item = variable_item_list_add(
        app->variable_item_list_settings,
        "Mute On Exit",
        2,
        my_app_mute_on_exit_change,
        app);
    variable_item_set_current_value_index(mute_exit_item, app_settings.mute_on_exit ? 1 : 0);
    variable_item_set_current_value_text(mute_exit_item, app_settings.mute_on_exit ? "On" : "Off");
    
    // Add seek strength setting
    VariableItem* seek_item = variable_item_list_add(
        app->variable_item_list_settings,
        "Seek Strength",
        15,
        my_app_seek_strength_change,
        app);
    variable_item_set_current_value_index(seek_item, app_settings.seek_strength - 1);
    char seek_text[8];
    snprintf(seek_text, sizeof(seek_text), "%d", app_settings.seek_strength);
    variable_item_set_current_value_text(seek_item, seek_text);
    
    // Add audio mode setting
    VariableItem* audio_item = variable_item_list_add(
        app->variable_item_list_settings,
        "Audio Mode",
        NUM_AUDIO_MODES,
        my_app_audio_mode_change,
        app);
    variable_item_set_current_value_index(audio_item, app_settings.audio_mode);
    variable_item_set_current_value_text(audio_item, audio_mode_names[app_settings.audio_mode]);

    // Add start volume setting (Low/Med/High/Last)
    VariableItem* start_vol_item = variable_item_list_add(
        app->variable_item_list_settings,
        "Start Volume",
        NUM_START_VOLUME_OPTIONS,
        my_app_start_volume_change,
        app);
    variable_item_set_current_value_index(start_vol_item, app_settings.start_volume);
    variable_item_set_current_value_text(start_vol_item, start_volume_names[app_settings.start_volume]);

    // Add resume-last-station setting
    VariableItem* resume_item = variable_item_list_add(
        app->variable_item_list_settings,
        "Resume Station",
        2,
        my_app_resume_station_change,
        app);
    variable_item_set_current_value_index(resume_item, app_settings.resume_station ? 1 : 0);
    variable_item_set_current_value_text(resume_item, app_settings.resume_station ? "On" : "Off");

    // Add chip selection override (Auto / TEA5767 / Si4703)
    VariableItem* chip_item = variable_item_list_add(
        app->variable_item_list_settings,
        "Chip Select",
        NUM_CHIP_SELECT_OPTIONS,
        my_app_chip_select_change,
        app);
    variable_item_set_current_value_index(chip_item, (uint8_t)app_settings.chip_select);
    variable_item_set_current_value_text(chip_item, chip_select_names[app_settings.chip_select]);

    view_set_previous_callback(
        variable_item_list_get_view(app->variable_item_list_settings),
        my_app_navigation_submenu_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, 
        MyAppViewSettings, 
        variable_item_list_get_view(app->variable_item_list_settings));
    
    // Initialize the view for flipping the world
    app->view_flip_the_world = view_alloc();
    view_set_draw_callback(app->view_flip_the_world, my_app_view_draw_callback);
    view_set_input_callback(app->view_flip_the_world, my_app_view_input_callback);
    view_set_context(app->view_flip_the_world, app);
    view_set_enter_callback(app->view_flip_the_world, my_app_view_enter_callback);
    view_set_previous_callback(app->view_flip_the_world, my_app_navigation_submenu_callback);
    view_allocate_model(app->view_flip_the_world, ViewModelTypeLockFree, sizeof(MyModel));
    MyModel* model = view_get_model(app->view_flip_the_world);
    model->current_station_index = current_station_index;
    model->volume_index = volume_index;
    view_dispatcher_add_view(app->view_dispatcher, MyAppViewFlipTheWorld, app->view_flip_the_world);

    // Initialize the tuner view
    app->view_tuner = view_alloc();
    view_set_draw_callback(app->view_tuner, tuner_draw_callback);
    view_set_input_callback(app->view_tuner, tuner_input_callback);
    view_set_context(app->view_tuner, app);
    view_allocate_model(app->view_tuner, ViewModelTypeLockFree, sizeof(uint8_t));
    view_set_previous_callback(app->view_tuner, my_app_navigation_submenu_callback);
    view_dispatcher_add_view(app->view_dispatcher, MyAppViewTuner, app->view_tuner);

    // Initialize the file viewer
    app->view_file_viewer = view_alloc();
    view_set_draw_callback(app->view_file_viewer, file_viewer_draw_callback);
    view_set_input_callback(app->view_file_viewer, file_viewer_input_callback);
    view_set_context(app->view_file_viewer, app);
    view_set_previous_callback(app->view_file_viewer, my_app_navigation_submenu_callback);
    view_allocate_model(app->view_file_viewer, ViewModelTypeLockFree, sizeof(FileViewerModel));
    
    // Load first region
    if(NUM_REGIONS > 0) {
        load_file_content(app, 0);
    }
    
    view_dispatcher_add_view(app->view_dispatcher, MyAppViewFileViewer, app->view_file_viewer);
    
    // Initialize the widget for displaying information about the app
    app->widget_about = widget_alloc();
    
    char about_text[1152];
    snprintf(about_text, sizeof(about_text),
        "FM Radio v2.2\n"
        "By Coolshrimp\n"
        "CoolshrimpModz.com\n"
        "---\n"
        "Supports TEA5767 & Si4703\n"
        "Active chip: %s\n"
        "Regions loaded: %lu %s\n"
        "---\n"
        "LISTEN NOW\n"
        "Left/Right  = Seek\n"
        "Up/Down     = Preset\n"
        "OK = Volume/Mute\n"
        "  Si4703: Low/Med/Hi/Mute\n"
        "  Hold OK = Mute now\n"
        "  TEA5767: Mute toggle\n"
        "---\n"
        "TUNER VIEW\n"
        "Left/Right  = Fine tune\n"
        "             (0.1 MHz steps)\n"
        "Up/Down     = Auto seek\n"
        "OK = Volume/Mute\n"
        "---\n"
        "SETTINGS\n"
        "Chip Select\n"
        "  Auto / TEA5767 / Si4703\n"
        "Audio Mode\n"
        "  Stereo / Mono L / Mono R\n"
        "Seek Strength  1-15\n"
        "  Higher = stronger signal\n"
        "  required to stop seek\n"
        "Region  (loads preset list)\n"
        "Start Volume\n"
        "  Low/Med/High/Last\n"
        "Resume Station  On/Off\n"
        "  reopens last frequency\n"
        "Mute on Exit  On/Off\n"
        "---\n"
        "STATION FILES (SD Card)\n"
        "/ext/apps_data/fm_radio/\n"
        "One .txt per region.\n"
        "Format per line:\n"
        "  frequency|Station Name\n"
        "Example:\n"
        "  100.7|Classic Rock FM\n"
        "Up to 100 presets/region.\n"
        "---\n"
        "WIRING\n"
        "Both chips: SCL=Pin16\n"
        "            SDA=Pin15\n"
        "Si4703 also needs:\n"
        "  RST -> Pin 4 A4 (req)\n"
        "  SEN -> 3.3V  (req)",
        radio_get_chip_name(),
        NUM_REGIONS,
        regions_scanned_from_sd ? "(from SD)" : "(defaults)");
    
    widget_add_text_scroll_element(app->widget_about, 0, 0, 128, 64, about_text);    

    view_set_previous_callback(widget_get_view(app->widget_about), my_app_navigation_submenu_callback);
    view_dispatcher_add_view(app->view_dispatcher, MyAppViewAbout, widget_get_view(app->widget_about));
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
#ifdef BACKLIGHT_ALWAYS_ON
    notification_message(app->notifications, &sequence_display_backlight_enforce_on);
#endif
    return app;
}
// Free memory used by the application
void my_app_free(MyApp* app) {
#ifdef BACKLIGHT_ALWAYS_ON
    notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
#endif
    furi_record_close(RECORD_NOTIFICATION);
    view_dispatcher_remove_view(app->view_dispatcher, MyAppViewAbout);
    widget_free(app->widget_about);
    view_dispatcher_remove_view(app->view_dispatcher, MyAppViewFileViewer);
    view_free(app->view_file_viewer);
    view_dispatcher_remove_view(app->view_dispatcher, MyAppViewTuner);
    view_free(app->view_tuner);
    view_dispatcher_remove_view(app->view_dispatcher, MyAppViewFlipTheWorld);
    view_free(app->view_flip_the_world);
    view_dispatcher_remove_view(app->view_dispatcher, MyAppViewSettings);
    variable_item_list_free(app->variable_item_list_settings);
    view_dispatcher_remove_view(app->view_dispatcher, MyAppViewConfigure);
    variable_item_list_free(app->variable_item_list_config);
    view_dispatcher_remove_view(app->view_dispatcher, MyAppViewSubmenu);
    submenu_free(app->submenu);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);
    
    // Put radio to sleep (best-effort) if configured
    radio_sleep_or_unmute_on_exit();

    // Persist last volume and last tuned frequency for the next run.
    // cached_info retains the last good chip reading even when its valid
    // flag is false; frequency 0 means the chip never reported anything.
    app_settings.last_volume_step = (volume_step != 0) ? volume_step : last_volume_step;
    if(cached_info.frequency >= 87.5f && cached_info.frequency <= 108.0f) {
        app_settings.last_frequency = cached_info.frequency;
    }
    save_app_settings(app->storage);

    // Free station memory and close storage
    free_stations();
    furi_record_close(RECORD_STORAGE);
    
    free(app);
}
// Main function to start the application
int32_t fm_radio_app(void* p) {
    UNUSED(p);
    MyApp* app = my_app_alloc();
    if(!app) return -1;
    view_dispatcher_run(app->view_dispatcher);
    my_app_free(app);
    return 0;
}
