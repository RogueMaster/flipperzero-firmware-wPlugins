/**
 * @file reality_clock.c
 * @brief Reality Dimension Clock for Flipper Zero
 *
 * Multi-band electromagnetic ratio analyzer for dimensional stability detection.
 * Uses rolling 1000-sample buffer for ultra-stable readings.
 *
 * @author Eris Margeta (@Eris-Margeta)
 * @license MIT
 * @version 3.0
 *
 * SPDX-License-Identifier: MIT
 */

/* ============================================================================
 * DEBUG MODE - Enable logging to SD card
 * Real hardware sensors are always used (no simulated mode).
 * Uncomment DEBUG_LOG_TO_SD to enable CSV logging for analysis.
 * ============================================================================ */
#define DEBUG_MODE 1 /* Keep this - enables real sensors */
/* #define DEBUG_LOG_TO_SD 1 */ /* Disabled for production - no SD logging */

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_random.h>
#include <furi_hal_power.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <stdlib.h>
#include <math.h>

/* ============================================================================
 * INTERNAL NOTIFICATION STRUCTURES
 * ============================================================================
 * These structures are defined here to access internal notification settings
 * for flicker-free brightness control (same approach as NightStand Clock).
 * Based on flipperdevices/flipperzero-firmware notification_app.h
 */

typedef struct {
    uint8_t value_last[2];
    uint8_t value[2];
    uint8_t index;
    uint8_t light;
} NotificationLedLayer;

typedef struct {
    uint8_t version;
    float display_brightness;
    float led_brightness;
    float speaker_volume;
    uint32_t display_off_delay_ms;
    int8_t contrast;
    bool vibro_on;
} NotificationSettings;

typedef struct {
    FuriMessageQueue* queue;
    FuriPubSub* event_record;
    FuriTimer* display_timer;
    NotificationLedLayer display;
    NotificationLedLayer led[3];
    uint8_t display_led_lock;
    NotificationSettings settings;
} NotificationAppInternal;

#ifdef DEBUG_MODE
#include <furi_hal_subghz.h>
#include <furi_hal_adc.h>
#ifdef DEBUG_LOG_TO_SD
#include <storage/storage.h>
#endif
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64

#define INPUT_QUEUE_SIZE 8

/** Sample rates - production values */
#define SAMPLE_INTERVAL_CALIB_MS  200 /**< 5 samples/sec during calibration */
#define SAMPLE_INTERVAL_NORMAL_MS 1000 /**< 1 sample/sec during normal (battery friendly) */

/** Rolling buffer size */
#define BUFFER_SIZE         1000 /**< Rolling buffer for stability */
#define CALIBRATION_SAMPLES 100 /**< Samples needed before stable (20 sec at 5Hz) */

#ifdef DEBUG_MODE
/** Real sensor frequencies (Hz) */
#define FREQ_BAND_1 315000000 /**< 315 MHz - Path 2 */
#define FREQ_BAND_2 433920000 /**< 433.92 MHz - Path 1 */
#define FREQ_BAND_3 868350000 /**< 868.35 MHz - Path 3 */

/** Real sensor calibration values (from data collection)
 *  These are typical RSSI values in a normal environment */
#define REAL_BASE_315 (-99.4f) /**< Avg RSSI at 315 MHz */
#define REAL_BASE_433 (-96.1f) /**< Avg RSSI at 433 MHz */
#define REAL_BASE_868 (-112.8f) /**< Avg RSSI at 868 MHz */
#define REAL_VAR_315  (5.5f) /**< 2-sigma variation at 315 MHz */
#define REAL_VAR_433  (3.3f) /**< 2-sigma variation at 433 MHz */
#define REAL_VAR_868  (3.7f) /**< 2-sigma variation at 868 MHz */

/** RSSI offset for normalization (real RSSI is -90 to -120 dBm) */
#define RSSI_OFFSET 120.0f /**< Add to RSSI to get positive dB */

/** Log file path */
#define DEBUG_LOG_PATH EXT_PATH("apps_data/reality_clock/sensor_log.csv")
#define DEBUG_LOG_DIR  EXT_PATH("apps_data/reality_clock")
#endif

/** Stability thresholds - based on short-term variance, not fixed baseline */
#define HOME_THRESHOLD     98.0f /**< Very stable readings */
#define STABLE_THRESHOLD   95.0f /**< Mostly stable */
#define UNSTABLE_THRESHOLD 90.0f /**< Some fluctuation */

/** Adaptive baseline using Exponential Moving Average (EMA)
 *  Higher alpha = faster adaptation = baseline tracks "current reality" closely
 *  With alpha=0.05, baseline is 92% adapted after just 50 samples (~25 sec) */
#define EMA_ALPHA      0.05f /**< Fast adaptation to current reality */
#define EMA_ALPHA_FAST 0.15f /**< Very fast EMA for instant tracking */

/** Screen IDs */
#define SCREEN_HOME       0 /**< Main sci-fi display */
#define SCREEN_BANDS      1 /**< Band readings */
#define SCREEN_DETAILS    2 /**< Scrollable details */
#define SCREEN_INFO       3 /**< QR code / info screen */
#define SCREEN_MENU       4 /**< Settings menu */
#define SCREEN_BRIGHTNESS 5 /**< Brightness slider */
#define SCREEN_COUNT      6

/** Menu items */
#define MENU_ITEM_CALIBRATE  0
#define MENU_ITEM_BRIGHTNESS 1
#define MENU_ITEM_COUNT      2

/** Brightness settings */
#define BRIGHTNESS_MIN        0
#define BRIGHTNESS_MAX        100
#define BRIGHTNESS_STEP       5
#define BRIGHTNESS_REFRESH_MS 60000 /* Reapply brightness every 60 sec to prevent firmware reset */

/** Details screen */
#define DETAILS_LINES   12
#define DETAILS_VISIBLE 5
#define LINE_HEIGHT     10

/* ============================================================================
 * TYPES
 * ============================================================================ */

typedef enum {
    DimStatusHome,
    DimStatusStable,
    DimStatusUnstable,
    DimStatusForeign,
    DimStatusCalibrating,
} DimensionStatus;

/** Rolling buffer for a single band */
typedef struct {
    float values[BUFFER_SIZE];
    uint16_t write_idx;
    uint16_t count;
    float sum;
} RollingBuffer;

typedef struct {
    bool is_running;
    bool is_calibrated;

    uint8_t current_screen;
    uint8_t previous_screen; /**< For returning from menu */
    int8_t scroll_offset;
    uint8_t menu_selection; /**< Current menu item */
    uint8_t brightness; /**< Current brightness 0-100 */
    uint32_t brightness_refresh_time; /**< Next time to reapply brightness */

    NotificationAppInternal* notification; /**< Notification service (internal cast) */
    float original_brightness; /**< Saved brightness to restore on exit */

    /** Rolling buffers for each band */
    RollingBuffer lf_buffer;
    RollingBuffer hf_buffer;
    RollingBuffer uhf_buffer;

    /** Averaged readings (from buffers) */
    float lf_avg;
    float hf_avg;
    float uhf_avg;

    /** Current raw readings (for display) */
    float lf_raw;
    float hf_raw;
    float uhf_raw;

    float phi_current; /**< Φ from averaged readings */
    float phi_baseline; /**< Adaptive baseline Φ (EMA-tracked) */
    float phi_short_term; /**< Short-term EMA for variance calc */
    float match_percent;
    float stability; /**< Current stability metric 0-100 */

    DimensionStatus status;

    uint32_t total_samples;
    float voltage;
    float current_ma;

#ifdef DEBUG_MODE
    /** Debug: Real sensor data */
    float temperature; /**< Internal die temperature in °C */
    float rssi_315; /**< Real RSSI at 315 MHz */
    float rssi_433; /**< Real RSSI at 433 MHz */
    float rssi_868; /**< Real RSSI at 868 MHz */
    uint32_t start_time; /**< Session start timestamp */

    /** Debug: Hardware handles */
    FuriHalAdcHandle* adc_handle;
#ifdef DEBUG_LOG_TO_SD
    Storage* storage;
    File* log_file;
    bool log_active;
#endif
#endif
} RealityClockState;

/* ============================================================================
 * ROLLING BUFFER
 * ============================================================================ */

static void buffer_init(RollingBuffer* buf) {
    memset(buf->values, 0, sizeof(buf->values));
    buf->write_idx = 0;
    buf->count = 0;
    buf->sum = 0.0f;
}

static void buffer_add(RollingBuffer* buf, float value) {
    /* Subtract old value from sum if buffer is full */
    if(buf->count >= BUFFER_SIZE) {
        buf->sum -= buf->values[buf->write_idx];
    } else {
        buf->count++;
    }

    /* Add new value */
    buf->values[buf->write_idx] = value;
    buf->sum += value;

    /* Advance write pointer (circular) */
    buf->write_idx = (buf->write_idx + 1) % BUFFER_SIZE;
}

static float buffer_average(RollingBuffer* buf) {
    if(buf->count == 0) return 0.0f;
    return buf->sum / (float)buf->count;
}

/* ============================================================================
 * HARDWARE ENTROPY (used only in simulated mode)
 * ============================================================================ */

#ifndef DEBUG_MODE
static float get_entropy_float(void) {
    uint32_t val;
    furi_hal_random_fill_buf((uint8_t*)&val, sizeof(val));
    return (float)(val & 0xFFFF) / 65536.0f;
}

static float get_timing_jitter(void) {
    uint32_t t1 = furi_get_tick();
    furi_delay_us(1);
    uint32_t t2 = furi_get_tick();
    return ((float)((t2 - t1) ^ (t1 & 0xFF)) / 1000.0f) - 0.5f;
}
#endif

/* ============================================================================
 * SENSOR READINGS - Raw with natural variation (SIMULATED - used when DEBUG_MODE is off)
 * ============================================================================ */

#ifndef DEBUG_MODE
static float read_lf_raw(void) {
    float base = -42.0f;
    float variation = get_entropy_float() * 8.0f - 4.0f;
    float jitter = get_timing_jitter() * 1.5f;
    return base + variation + jitter;
}

static float read_hf_raw(void) {
    float base = -58.0f;
    float variation = get_entropy_float() * 6.0f - 3.0f;
    float jitter = get_timing_jitter() * 1.2f;
    return base + variation + jitter;
}

static float read_uhf_raw(void) {
    float base = -70.0f;
    float variation = get_entropy_float() * 10.0f - 5.0f;
    float jitter = get_timing_jitter() * 2.0f;
    return base + variation + jitter;
}
#endif

/* ============================================================================
 * REAL SENSOR READINGS (DEBUG_MODE only)
 * Uses actual SubGHz radio RSSI and internal temperature sensor
 * ============================================================================ */

#ifdef DEBUG_MODE

/**
 * @brief Read real RSSI at specified frequency using SubGHz radio
 * @param frequency Frequency in Hz (e.g., 433920000 for 433.92 MHz)
 * @return RSSI value in dBm
 */
static float read_real_rssi(uint32_t frequency) {
    /* Set frequency and appropriate antenna path */
    furi_hal_subghz_set_frequency_and_path(frequency);

    /* Switch to RX mode */
    furi_hal_subghz_rx();

    /* Small delay for RSSI to stabilize */
    furi_delay_us(500);

    /* Read RSSI */
    float rssi = furi_hal_subghz_get_rssi();

    /* Return to idle */
    furi_hal_subghz_idle();

    return rssi;
}

/**
 * @brief Read internal die temperature from STM32 ADC
 * @param adc_handle ADC handle (must be acquired and configured)
 * @return Temperature in degrees Celsius
 */
static float read_real_temperature(FuriHalAdcHandle* adc_handle) {
    if(adc_handle == NULL) return 0.0f;

    /* Read temperature sensor channel */
    uint16_t raw_temp = furi_hal_adc_read(adc_handle, FuriHalAdcChannelTEMPSENSOR);

    /* Convert to temperature */
    return furi_hal_adc_convert_temp(adc_handle, raw_temp);
}

/**
 * @brief Read all real sensor bands
 * Band mapping for "dimensional" theme:
 *   - "LF" band  = 315 MHz RSSI (lower frequency)
 *   - "HF" band  = 433 MHz RSSI (mid frequency)
 *   - "UHF" band = 868 MHz RSSI (higher frequency)
 */
static void read_real_sensors(RealityClockState* state) {
    /* Read RSSI at each frequency band */
    state->rssi_315 = read_real_rssi(FREQ_BAND_1);
    state->rssi_433 = read_real_rssi(FREQ_BAND_2);
    state->rssi_868 = read_real_rssi(FREQ_BAND_3);

    /* Map to LF/HF/UHF for display consistency */
    state->lf_raw = state->rssi_315;
    state->hf_raw = state->rssi_433;
    state->uhf_raw = state->rssi_868;

    /* Read temperature */
    state->temperature = read_real_temperature(state->adc_handle);
}

#ifdef DEBUG_LOG_TO_SD
/**
 * @brief Initialize SD card logging
 */
static bool debug_log_init(RealityClockState* state) {
    state->storage = furi_record_open(RECORD_STORAGE);
    if(!state->storage) return false;

    /* Create directory if it doesn't exist */
    storage_common_mkdir(state->storage, DEBUG_LOG_DIR);

    /* Open log file for append */
    state->log_file = storage_file_alloc(state->storage);
    if(!storage_file_open(state->log_file, DEBUG_LOG_PATH, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        storage_file_free(state->log_file);
        state->log_file = NULL;
        furi_record_close(RECORD_STORAGE);
        state->storage = NULL;
        return false;
    }

    /* Always start fresh - truncate and write new header */
    storage_file_seek(state->log_file, 0, true);
    storage_file_truncate(state->log_file);
    const char* header =
        "timestamp_ms,sample_num,rssi_315,rssi_433,rssi_868,temperature,voltage,phi_current,phi_baseline,phi_short,stability,match_pct\n";
    storage_file_write(state->log_file, header, strlen(header));

    state->log_active = true;
    state->start_time = furi_get_tick();
    return true;
}

/**
 * @brief Write a log entry to SD card
 */
static void debug_log_write(RealityClockState* state) {
    if(!state->log_active || !state->log_file) return;

    char log_line[300];
    uint32_t elapsed_ms = furi_get_tick() - state->start_time;

    snprintf(
        log_line,
        sizeof(log_line),
        "%lu,%lu,%.2f,%.2f,%.2f,%.2f,%.3f,%.6f,%.6f,%.6f,%.2f,%.2f\n",
        (unsigned long)elapsed_ms,
        (unsigned long)state->total_samples,
        (double)state->rssi_315,
        (double)state->rssi_433,
        (double)state->rssi_868,
        (double)state->temperature,
        (double)state->voltage,
        (double)state->phi_current,
        (double)state->phi_baseline,
        (double)state->phi_short_term,
        (double)state->stability,
        (double)state->match_percent);

    storage_file_write(state->log_file, log_line, strlen(log_line));

    /* Sync to disk every 100 samples */
    if(state->total_samples % 100 == 0) {
        storage_file_sync(state->log_file);
    }
}

/**
 * @brief Close log file and cleanup
 */
static void debug_log_close(RealityClockState* state) {
    if(state->log_file) {
        storage_file_close(state->log_file);
        storage_file_free(state->log_file);
        state->log_file = NULL;
    }
    if(state->storage) {
        furi_record_close(RECORD_STORAGE);
        state->storage = NULL;
    }
    state->log_active = false;
}
#endif /* DEBUG_LOG_TO_SD */

#endif /* DEBUG_MODE */

/* ============================================================================
 * CALCULATIONS
 * ============================================================================ */

/**
 * @brief Convert dB to linear with offset normalization
 * Real RSSI values are -120 to -85 dBm, which produce tiny linear values
 * that cause underflow. We normalize by adding an offset first.
 */
static float db_to_linear_normalized(float db) {
    /* Normalize: real RSSI (-120 to -85) becomes (0 to 35) */
    float normalized = db + 120.0f;
    if(normalized < 0.0f) normalized = 0.1f; /* Floor at 0.1 to avoid log(0) */
    return powf(10.0f, normalized / 20.0f);
}

/**
 * @brief Calculate PHI (dimensional stability index)
 * PHI = (LF * UHF) / (HF^2)
 * This ratio should remain constant if physical constants are stable.
 * With normalized values, PHI typically ranges 0.05 to 0.2
 */
static float calculate_phi(float lf_db, float hf_db, float uhf_db) {
    float lf_lin = db_to_linear_normalized(lf_db);
    float hf_lin = db_to_linear_normalized(hf_db);
    float uhf_lin = db_to_linear_normalized(uhf_db);

    if(hf_lin < 0.001f) return 0.0f;
    return (lf_lin * uhf_lin) / (hf_lin * hf_lin);
}

/**
 * @brief Calculate stability based on baseline tracking quality
 *
 * KEY INSIGHT: Sensor noise is NOT dimensional instability.
 * With adaptive baseline (fast EMA), baseline tracks current PHI closely.
 * Stability = how well baseline matches current.
 *
 * Since baseline adapts quickly (α=0.05), it stays within ~5% of current.
 * This 5% tracking error should map to 99%+ stability.
 *
 * Only a SUDDEN JUMP that baseline can't track = instability.
 */
static float calculate_stability(float current, float short_term, float baseline) {
    UNUSED(short_term); /* Not used in new algorithm */

    if(baseline < 0.0001f) return 100.0f;

    /* Calculate how well baseline is tracking current */
    float tracking_error = fabsf(current - baseline) / baseline;

    /* With α=0.05 EMA, baseline tracks within ~5-10% normally.
     * Map this to stability:
     *   0% error  = 100% stable
     *   5% error  = 99% stable (normal tracking lag)
     *  10% error  = 98% stable (still normal)
     *  20% error  = 96% stable (faster change)
     *  50% error  = 90% stable (significant jump)
     * 100% error  = 80% stable (major discontinuity)
     *
     * Formula: stability = 100 - (error * 20), clamped to [80, 100]
     * This ensures normal sensor variation always shows as stable.
     */
    float stability = 100.0f - (tracking_error * 20.0f);

    if(stability < 80.0f) stability = 80.0f; /* Floor at 80% */
    if(stability > 100.0f) stability = 100.0f;
    return stability;
}

/**
 * @brief Calculate match percentage (for display compatibility)
 * This now represents stability relative to adaptive baseline.
 */
static float calculate_match(float current, float baseline) {
    if(baseline < 0.0001f) return 100.0f;
    float diff = fabsf(current - baseline) / baseline;
    /* More forgiving: 2% deviation = ~96% match */
    float match = 100.0f * (1.0f - diff * 2.0f);
    if(match < 0.0f) match = 0.0f;
    if(match > 100.0f) match = 100.0f;
    return match;
}

static DimensionStatus classify_status(float match_pct) {
    if(match_pct >= HOME_THRESHOLD) return DimStatusHome;
    if(match_pct >= STABLE_THRESHOLD) return DimStatusStable;
    if(match_pct >= UNSTABLE_THRESHOLD) return DimStatusUnstable;
    return DimStatusForeign;
}

static void update_readings(RealityClockState* state) {
#ifdef DEBUG_MODE
    /* Read REAL sensor values from hardware */
    read_real_sensors(state);
#else
    /* Read simulated sensor values */
    state->lf_raw = read_lf_raw();
    state->hf_raw = read_hf_raw();
    state->uhf_raw = read_uhf_raw();
#endif

    /* Add to rolling buffers */
    buffer_add(&state->lf_buffer, state->lf_raw);
    buffer_add(&state->hf_buffer, state->hf_raw);
    buffer_add(&state->uhf_buffer, state->uhf_raw);

    /* Calculate averages from buffers */
    state->lf_avg = buffer_average(&state->lf_buffer);
    state->hf_avg = buffer_average(&state->hf_buffer);
    state->uhf_avg = buffer_average(&state->uhf_buffer);

    /* Calculate Φ from AVERAGED readings (stable!) */
    state->phi_current = calculate_phi(state->lf_avg, state->hf_avg, state->uhf_avg);

    state->total_samples++;

    /* Read battery */
    state->voltage = furi_hal_power_get_battery_voltage(FuriHalPowerICFuelGauge);
    state->current_ma = furi_hal_power_get_battery_current(FuriHalPowerICFuelGauge);

    /* Calibration check */
    if(!state->is_calibrated) {
        state->status = DimStatusCalibrating;

        if(state->lf_buffer.count >= CALIBRATION_SAMPLES) {
            /* Initialize all baselines from current averaged Φ */
            state->phi_baseline = state->phi_current;
            state->phi_short_term = state->phi_current;
            state->is_calibrated = true;
        }
    } else {
        /* ADAPTIVE BASELINE: Continuously update using EMA
         * This makes "current reality" = wherever you ARE right now */

        /* Slow EMA for long-term baseline - adapts over ~200 samples */
        state->phi_baseline =
            EMA_ALPHA * state->phi_current + (1.0f - EMA_ALPHA) * state->phi_baseline;

        /* Fast EMA for short-term trend - adapts over ~50 samples */
        state->phi_short_term =
            EMA_ALPHA_FAST * state->phi_current + (1.0f - EMA_ALPHA_FAST) * state->phi_short_term;

        /* Calculate stability based on short-term consistency */
        state->stability =
            calculate_stability(state->phi_current, state->phi_short_term, state->phi_baseline);

        /* Match percentage: how close to the adaptive baseline
         * Since baseline tracks us, this stays near 100% when stable */
        state->match_percent = calculate_match(state->phi_current, state->phi_baseline);

        /* Status based on stability, not fixed-baseline distance */
        state->status = classify_status(state->stability);
    }

#ifdef DEBUG_MODE
#ifdef DEBUG_LOG_TO_SD
    /* Log data to SD card */
    debug_log_write(state);
#endif
#endif
}

/* ============================================================================
 * QR CODE DATA - https://github.com/Eris-Margeta/flipper-apps
 * ============================================================================
 * 29x29 QR code - displayed at 2x scale (58x58 pixels on screen)
 */

#define QR_SIZE          29
#define QR_BYTES_PER_ROW 4
#define QR_SCALE         2

static const uint8_t qr_code_data[] = {
    0xFE, 0x2A, 0x9B, 0xF8, /* Row 0 */
    0x82, 0xA1, 0x6A, 0x08, /* Row 1 */
    0xBA, 0x09, 0x02, 0xE8, /* Row 2 */
    0xBA, 0xEF, 0xF2, 0xE8, /* Row 3 */
    0xBA, 0x40, 0xFA, 0xE8, /* Row 4 */
    0x82, 0xDB, 0x82, 0x08, /* Row 5 */
    0xFE, 0xAA, 0xAB, 0xF8, /* Row 6 */
    0x00, 0x73, 0x10, 0x00, /* Row 7 */
    0xFB, 0xD6, 0x0D, 0x50, /* Row 8 */
    0x00, 0x2A, 0xFB, 0x88, /* Row 9 */
    0x3F, 0xA5, 0x08, 0x80, /* Row 10 */
    0xC9, 0x89, 0x88, 0x50, /* Row 11 */
    0x36, 0x6C, 0x40, 0x60, /* Row 12 */
    0xFD, 0xC4, 0xAF, 0x88, /* Row 13 */
    0x57, 0x7B, 0x2C, 0xE0, /* Row 14 */
    0x80, 0xF2, 0x9F, 0x90, /* Row 15 */
    0x36, 0xF7, 0x55, 0x60, /* Row 16 */
    0xE0, 0x68, 0x97, 0xA8, /* Row 17 */
    0xBA, 0xC1, 0xEB, 0xA0, /* Row 18 */
    0x85, 0xAA, 0xAE, 0x10, /* Row 19 */
    0x96, 0x0E, 0x5F, 0xB8, /* Row 20 */
    0x00, 0xE2, 0x28, 0xF8, /* Row 21 */
    0xFE, 0xD9, 0xDA, 0xE0, /* Row 22 */
    0x82, 0x31, 0x18, 0x90, /* Row 23 */
    0xBA, 0xFF, 0x4F, 0xA8, /* Row 24 */
    0xBA, 0xAA, 0xB8, 0x78, /* Row 25 */
    0xBA, 0xA1, 0x1F, 0xF0, /* Row 26 */
    0x82, 0xD9, 0x8D, 0x50, /* Row 27 */
    0xFE, 0xA7, 0xD3, 0xA0, /* Row 28 */
};

/* ============================================================================
 * SCI-FI UI DRAWING UTILITIES
 * ============================================================================ */

/** Draw sci-fi corner brackets */
static void draw_scifi_corners(Canvas* canvas) {
    /* Top-left corner */
    canvas_draw_line(canvas, 0, 0, 10, 0);
    canvas_draw_line(canvas, 0, 0, 0, 10);
    canvas_draw_line(canvas, 2, 2, 8, 2);
    canvas_draw_line(canvas, 2, 2, 2, 8);

    /* Top-right corner */
    canvas_draw_line(canvas, 117, 0, 127, 0);
    canvas_draw_line(canvas, 127, 0, 127, 10);
    canvas_draw_line(canvas, 119, 2, 125, 2);
    canvas_draw_line(canvas, 125, 2, 125, 8);

    /* Bottom-left corner */
    canvas_draw_line(canvas, 0, 53, 0, 63);
    canvas_draw_line(canvas, 0, 63, 10, 63);
    canvas_draw_line(canvas, 2, 55, 2, 61);
    canvas_draw_line(canvas, 2, 61, 8, 61);

    /* Bottom-right corner */
    canvas_draw_line(canvas, 127, 53, 127, 63);
    canvas_draw_line(canvas, 117, 63, 127, 63);
    canvas_draw_line(canvas, 125, 55, 125, 61);
    canvas_draw_line(canvas, 119, 61, 125, 61);
}

/** Draw decorative horizontal lines */
static void draw_scifi_lines(Canvas* canvas, int16_t y) {
    /* Left side decorative line */
    canvas_draw_line(canvas, 5, y, 25, y);
    canvas_draw_dot(canvas, 27, y);
    canvas_draw_dot(canvas, 29, y);

    /* Right side decorative line */
    canvas_draw_line(canvas, 102, y, 122, y);
    canvas_draw_dot(canvas, 100, y);
    canvas_draw_dot(canvas, 98, y);
}

/** Draw a large stylized character for E137 */
static void draw_large_e137(Canvas* canvas, int16_t center_x, int16_t center_y) {
    /* Draw "E-137" in a large, blocky sci-fi style */
    /* Each character is approximately 12 pixels wide, 16 pixels tall */
    int16_t x = center_x - 30; /* Start position */
    int16_t y = center_y - 8;

    /* 'E' */
    canvas_draw_box(canvas, x, y, 3, 16);
    canvas_draw_box(canvas, x, y, 10, 3);
    canvas_draw_box(canvas, x, y + 6, 8, 3);
    canvas_draw_box(canvas, x, y + 13, 10, 3);
    x += 14;

    /* '-' */
    canvas_draw_box(canvas, x, y + 6, 6, 3);
    x += 10;

    /* '1' */
    canvas_draw_box(canvas, x + 3, y, 3, 16);
    canvas_draw_box(canvas, x, y, 6, 3);
    canvas_draw_box(canvas, x, y + 13, 9, 3);
    x += 13;

    /* '3' */
    canvas_draw_box(canvas, x, y, 10, 3);
    canvas_draw_box(canvas, x + 7, y, 3, 16);
    canvas_draw_box(canvas, x + 2, y + 6, 8, 3);
    canvas_draw_box(canvas, x, y + 13, 10, 3);
    x += 14;

    /* '7' */
    canvas_draw_box(canvas, x, y, 10, 3);
    canvas_draw_box(canvas, x + 7, y, 3, 16);
}

/* ============================================================================
 * SCREEN DRAWING
 * ============================================================================ */

static void draw_screen_home(Canvas* canvas, RealityClockState* state) {
    /* Sci-fi corners */
    draw_scifi_corners(canvas);

    /* Title at top */
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 8, AlignCenter, AlignCenter, "REALITY DIMENSION CLOCK");

    /* Decorative line under title */
    draw_scifi_lines(canvas, 14);

    if(!state->is_calibrated) {
        /* Calibrating display */
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, "CALIBRATING...");

        char buf[32];
        float progress = (float)state->lf_buffer.count / (float)CALIBRATION_SAMPLES * 100.0f;
        snprintf(buf, sizeof(buf), "%d%%", (int)progress);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignCenter, buf);

        /* Progress bar */
        canvas_draw_frame(canvas, 24, 48, 80, 8);
        int16_t fill = (int16_t)(progress * 0.78f);
        if(fill > 0) canvas_draw_box(canvas, 25, 49, fill, 6);

        return;
    }

    /* Large dimension ID in center - only show E-137 for HOME dimension */
    if(state->status == DimStatusHome) {
        draw_large_e137(canvas, 64, 32);
    } else {
        /* Show "?-???" for unknown/other dimensions */
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "?-???");
    }

    /* Decorative line above status */
    draw_scifi_lines(canvas, 48);

    /* Status text */
    canvas_set_font(canvas, FontSecondary);
    const char* status_text;
    switch(state->status) {
    case DimStatusHome:
        status_text = "[ HOME ]";
        break;
    case DimStatusStable:
        status_text = "[ STABLE ]";
        break;
    case DimStatusUnstable:
        status_text = "[ DRIFT DETECTED ]";
        break;
    case DimStatusForeign:
        status_text = "[ FOREIGN DIMENSION ]";
        break;
    default:
        status_text = "[ SCANNING... ]";
    }
    canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignCenter, status_text);

    /* Navigation hint - subtle */
    canvas_draw_str(canvas, 122, 32, ">");
}

static void draw_bar(Canvas* canvas, int16_t x, int16_t y, int16_t w, int16_t h, float percent) {
    if(percent < 0.0f) percent = 0.0f;
    if(percent > 100.0f) percent = 100.0f;
    int16_t fill_w = (int16_t)((w * percent) / 100.0f);
    if(fill_w > 0) {
        canvas_draw_box(canvas, x, y, fill_w, h);
    }
    for(int16_t i = fill_w; i < w; i += 2) {
        canvas_draw_dot(canvas, x + i, y + h / 2);
    }
}

static float db_to_percent(float db) {
    /* Real RSSI range: -120 dBm (weak) to -85 dBm (strong)
     * Map this to 0-100% for bar display */
    float p = ((db + 120.0f) / 35.0f) * 100.0f;
    if(p < 0.0f) p = 0.0f;
    if(p > 100.0f) p = 100.0f;
    return p;
}

static void draw_screen_bands(Canvas* canvas, RealityClockState* state) {
    char buf[32];

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, "BAND ANALYSIS");
    canvas_draw_line(canvas, 0, 10, 127, 10);

    int16_t y = 14;
    int16_t bar_x = 35;
    int16_t bar_w = 55;

    /* LF - show both raw and averaged */
    snprintf(buf, sizeof(buf), "LF");
    canvas_draw_str(canvas, 2, y + 5, buf);
    draw_bar(canvas, bar_x, y, bar_w, 5, db_to_percent(state->lf_avg));
    snprintf(buf, sizeof(buf), "%.1f", (double)state->lf_avg);
    canvas_draw_str(canvas, 95, y + 5, buf);
    y += 10;

    /* HF */
    canvas_draw_str(canvas, 2, y + 5, "HF");
    draw_bar(canvas, bar_x, y, bar_w, 5, db_to_percent(state->hf_avg));
    snprintf(buf, sizeof(buf), "%.1f", (double)state->hf_avg);
    canvas_draw_str(canvas, 95, y + 5, buf);
    y += 10;

    /* UHF */
    canvas_draw_str(canvas, 2, y + 5, "UHF");
    draw_bar(canvas, bar_x, y, bar_w, 5, db_to_percent(state->uhf_avg));
    snprintf(buf, sizeof(buf), "%.1f", (double)state->uhf_avg);
    canvas_draw_str(canvas, 95, y + 5, buf);

    /* Separator */
    canvas_draw_line(canvas, 0, 44, 127, 44);

    /* Phi and stability */
    snprintf(buf, sizeof(buf), "PHI: %.4f", (double)state->phi_current);
    canvas_draw_str(canvas, 2, 54, buf);

    snprintf(buf, sizeof(buf), "Stab: %.1f%%", (double)state->stability);
    canvas_draw_str(canvas, 70, 54, buf);

    /* Buffer status */
    snprintf(buf, sizeof(buf), "Buffer: %d/%d", state->lf_buffer.count, BUFFER_SIZE);
    canvas_draw_str(canvas, 2, 62, buf);

    /* Navigation */
    canvas_draw_str(canvas, 2, 8, "<");
    canvas_draw_str(canvas, 120, 8, ">");
}

static void draw_screen_details(Canvas* canvas, RealityClockState* state) {
#ifdef DEBUG_MODE
#define DETAILS_LINES_ACTUAL 17 /* Extra lines for debug info */
#else
#define DETAILS_LINES_ACTUAL 14 /* Extra lines for stability info */
#endif
    char lines[DETAILS_LINES_ACTUAL][32];
    int line_count = 0;

    snprintf(lines[line_count++], 32, "Current PHI:  %.4f", (double)state->phi_current);
    snprintf(lines[line_count++], 32, "Short-term:   %.4f", (double)state->phi_short_term);
    snprintf(lines[line_count++], 32, "Baseline:     %.4f", (double)state->phi_baseline);
    snprintf(lines[line_count++], 32, "Stability:    %.1f%%", (double)state->stability);
    snprintf(lines[line_count++], 32, "Match:        %.1f%%", (double)state->match_percent);
    snprintf(lines[line_count++], 32, "Buffer Size:  %d", state->lf_buffer.count);
    snprintf(lines[line_count++], 32, "Total Samples:%lu", (unsigned long)state->total_samples);
#ifdef DEBUG_MODE
    snprintf(lines[line_count++], 32, "315MHz RSSI:  %.2f dBm", (double)state->rssi_315);
    snprintf(lines[line_count++], 32, "433MHz RSSI:  %.2f dBm", (double)state->rssi_433);
    snprintf(lines[line_count++], 32, "868MHz RSSI:  %.2f dBm", (double)state->rssi_868);
    snprintf(lines[line_count++], 32, "Temperature:  %.1f C", (double)state->temperature);
#else
    snprintf(lines[line_count++], 32, "LF Raw:       %.2f dB", (double)state->lf_raw);
    snprintf(lines[line_count++], 32, "HF Raw:       %.2f dB", (double)state->hf_raw);
    snprintf(lines[line_count++], 32, "UHF Raw:      %.2f dB", (double)state->uhf_raw);
#endif
    snprintf(lines[line_count++], 32, "LF Avg:       %.2f dB", (double)state->lf_avg);
    snprintf(lines[line_count++], 32, "HF Avg:       %.2f dB", (double)state->hf_avg);
    snprintf(lines[line_count++], 32, "UHF Avg:      %.2f dB", (double)state->uhf_avg);
    snprintf(lines[line_count++], 32, "Battery:      %.2fV", (double)state->voltage);
#ifdef DEBUG_MODE
#ifdef DEBUG_LOG_TO_SD
    snprintf(lines[line_count++], 32, "Logging:      %s", state->log_active ? "ACTIVE" : "OFF");
#endif
#endif

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, "DETAILS");

    char scroll_buf[16];
    snprintf(
        scroll_buf,
        sizeof(scroll_buf),
        "[%d-%d/%d]",
        state->scroll_offset + 1,
        state->scroll_offset + DETAILS_VISIBLE,
        line_count);
    canvas_draw_str(canvas, 80, 8, scroll_buf);

    canvas_draw_line(canvas, 0, 10, 127, 10);

    int16_t y = 20;
    for(int i = 0; i < DETAILS_VISIBLE && (state->scroll_offset + i) < line_count; i++) {
        canvas_draw_str(canvas, 4, y, lines[state->scroll_offset + i]);
        y += LINE_HEIGHT;
    }

    /* Scroll hints */
    if(state->scroll_offset > 0) {
        canvas_draw_str(canvas, 118, 20, "^");
    }
    if(state->scroll_offset + DETAILS_VISIBLE < line_count) {
        canvas_draw_str(canvas, 118, 58, "v");
    }

    canvas_draw_str(canvas, 2, 62, "<");
}

static void draw_screen_info(Canvas* canvas, RealityClockState* state) {
    UNUSED(state);

    /* Draw QR code at 2x scale - 58x58 pixels, centered vertically */
    int16_t qr_x = 2;
    int16_t qr_y = (SCREEN_HEIGHT - QR_SIZE * QR_SCALE) / 2;

    for(int row = 0; row < QR_SIZE; row++) {
        for(int col = 0; col < QR_SIZE; col++) {
            int byte_idx = row * QR_BYTES_PER_ROW + col / 8;
            int bit_idx = 7 - (col % 8);
            if(qr_code_data[byte_idx] & (1 << bit_idx)) {
                /* Draw 2x2 pixel block for each QR module */
                canvas_draw_box(
                    canvas, qr_x + col * QR_SCALE, qr_y + row * QR_SCALE, QR_SCALE, QR_SCALE);
            }
        }
    }

    /* Minimal text on right side */
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 66, 28, "Scan for");
    canvas_draw_str(canvas, 66, 40, "source");

    /* Navigation hint */
    canvas_draw_str(canvas, 120, 8, "<");
}

static void draw_screen_menu(Canvas* canvas, RealityClockState* state) {
    /* Draw sci-fi corners */
    draw_scifi_corners(canvas);

    /* Title */
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 8, AlignCenter, AlignCenter, "SETTINGS");
    draw_scifi_lines(canvas, 14);

    /* Menu items */
    canvas_set_font(canvas, FontPrimary);

    const char* items[] = {"CALIBRATE", "BRIGHTNESS"};

    for(int i = 0; i < MENU_ITEM_COUNT; i++) {
        int16_t y = 28 + i * 16;

        if(i == state->menu_selection) {
            /* Selected item - draw highlight box */
            canvas_draw_box(canvas, 20, y - 8, 88, 14);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str_aligned(canvas, 64, y, AlignCenter, AlignCenter, items[i]);
            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_str_aligned(canvas, 64, y, AlignCenter, AlignCenter, items[i]);
        }
    }

    /* Hint at bottom */
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignCenter, "OK=Select  Back=Exit");
}

static void draw_screen_brightness(Canvas* canvas, RealityClockState* state) {
    /* Draw sci-fi corners */
    draw_scifi_corners(canvas);

    /* Title */
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 8, AlignCenter, AlignCenter, "BRIGHTNESS");
    draw_scifi_lines(canvas, 14);

    /* Brightness value */
    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", state->brightness);
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, buf);

    /* Slider bar */
    int16_t bar_x = 14;
    int16_t bar_y = 44;
    int16_t bar_w = 100;
    int16_t bar_h = 8;

    /* Bar outline */
    canvas_draw_frame(canvas, bar_x, bar_y, bar_w, bar_h);

    /* Fill based on brightness */
    int16_t fill_w = (bar_w - 2) * state->brightness / 100;
    if(fill_w > 0) {
        canvas_draw_box(canvas, bar_x + 1, bar_y + 1, fill_w, bar_h - 2);
    }

    /* Navigation hint */
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignCenter, "L/R=Adjust  Back=Done");
}

/**
 * @brief Apply brightness via notification settings (NightStand Clock approach)
 *
 * Directly modifies the notification app's internal brightness setting,
 * then triggers a backlight update. This avoids flickering because the
 * system uses our value instead of overriding it.
 *
 * @param state      Application state (contains notification handle)
 * @param brightness Brightness percentage (0-100)
 */
static void apply_brightness(RealityClockState* state, uint8_t brightness) {
    if(state->notification == NULL) return;

    /* Set brightness in notification settings (0.0 - 1.0) */
    state->notification->settings.display_brightness = (float)brightness / 100.0f;

    /* Trigger backlight update with new brightness value */
    notification_message((NotificationApp*)state->notification, &sequence_display_backlight_on);
}

static void render_callback(Canvas* canvas, void* ctx) {
    RealityClockState* state = (RealityClockState*)ctx;
    canvas_clear(canvas);

    switch(state->current_screen) {
    case SCREEN_HOME:
        draw_screen_home(canvas, state);
        break;
    case SCREEN_BANDS:
        draw_screen_bands(canvas, state);
        break;
    case SCREEN_DETAILS:
        draw_screen_details(canvas, state);
        break;
    case SCREEN_INFO:
        draw_screen_info(canvas, state);
        break;
    case SCREEN_MENU:
        draw_screen_menu(canvas, state);
        break;
    case SCREEN_BRIGHTNESS:
        draw_screen_brightness(canvas, state);
        break;
    default:
        draw_screen_home(canvas, state);
    }
}

/* ============================================================================
 * INPUT
 * ============================================================================ */

static void input_callback(InputEvent* event, void* ctx) {
    FuriMessageQueue* queue = (FuriMessageQueue*)ctx;
    furi_message_queue_put(queue, event, FuriWaitForever);
}

static void do_calibrate(RealityClockState* state) {
    state->is_calibrated = false;
    buffer_init(&state->lf_buffer);
    buffer_init(&state->hf_buffer);
    buffer_init(&state->uhf_buffer);
    state->total_samples = 0;
    state->phi_baseline = 0;
    state->phi_short_term = 0;
    state->match_percent = 0;
    state->stability = 0;
}

static void process_input(RealityClockState* state, InputEvent* event) {
    /* Only process actions for Press and Repeat events */
    if(event->type != InputTypePress && event->type != InputTypeRepeat) {
        return;
    }

    /* Handle brightness screen */
    if(state->current_screen == SCREEN_BRIGHTNESS) {
        switch(event->key) {
        case InputKeyLeft:
            if(state->brightness >= BRIGHTNESS_STEP) {
                state->brightness -= BRIGHTNESS_STEP;
                apply_brightness(state, state->brightness);
                state->brightness_refresh_time = furi_get_tick() + BRIGHTNESS_REFRESH_MS;
            }
            break;
        case InputKeyRight:
            if(state->brightness <= BRIGHTNESS_MAX - BRIGHTNESS_STEP) {
                state->brightness += BRIGHTNESS_STEP;
                apply_brightness(state, state->brightness);
                state->brightness_refresh_time = furi_get_tick() + BRIGHTNESS_REFRESH_MS;
            }
            break;
        case InputKeyBack:
        case InputKeyOk:
            /* Return to menu */
            state->current_screen = SCREEN_MENU;
            break;
        default:
            break;
        }
        return;
    }

    /* Handle menu screen */
    if(state->current_screen == SCREEN_MENU) {
        switch(event->key) {
        case InputKeyUp:
            if(state->menu_selection > 0) {
                state->menu_selection--;
            }
            break;
        case InputKeyDown:
            if(state->menu_selection < MENU_ITEM_COUNT - 1) {
                state->menu_selection++;
            }
            break;
        case InputKeyOk:
            if(state->menu_selection == MENU_ITEM_CALIBRATE) {
                do_calibrate(state);
                state->current_screen = state->previous_screen;
            } else if(state->menu_selection == MENU_ITEM_BRIGHTNESS) {
                state->current_screen = SCREEN_BRIGHTNESS;
            }
            break;
        case InputKeyBack:
            state->current_screen = state->previous_screen;
            break;
        default:
            break;
        }
        return;
    }

    /* Handle normal screens (HOME, BANDS, DETAILS, INFO) */
    switch(event->key) {
    case InputKeyLeft:
        if(state->current_screen > 0) {
            state->current_screen--;
            state->scroll_offset = 0;
        }
        break;

    case InputKeyRight:
        if(state->current_screen < SCREEN_INFO) {
            state->current_screen++;
            state->scroll_offset = 0;
        }
        break;

    case InputKeyUp:
        /* Only scroll on details screen */
        if(state->current_screen == SCREEN_DETAILS && state->scroll_offset > 0) {
            state->scroll_offset--;
        }
        break;

    case InputKeyDown:
        /* Only scroll on details screen */
        if(state->current_screen == SCREEN_DETAILS) {
            if(state->scroll_offset + DETAILS_VISIBLE < DETAILS_LINES) {
                state->scroll_offset++;
            }
        }
        break;

    case InputKeyOk:
        /* Open menu */
        state->previous_screen = state->current_screen;
        state->current_screen = SCREEN_MENU;
        state->menu_selection = 0;
        break;

    case InputKeyBack:
        state->is_running = false;
        break;

    default:
        break;
    }
}

/* ============================================================================
 * LIFECYCLE
 * ============================================================================ */

static RealityClockState* state_alloc(void) {
    RealityClockState* state = malloc(sizeof(RealityClockState));
    furi_check(state != NULL); /* Abort if allocation fails */
    memset(state, 0, sizeof(RealityClockState));

    state->is_running = true;
    state->status = DimStatusCalibrating;
    state->brightness = BRIGHTNESS_MAX; /**< Will be updated to system brightness in main */
    state->brightness_refresh_time = 0;
    state->notification = NULL;
    state->original_brightness = 1.0f;

    buffer_init(&state->lf_buffer);
    buffer_init(&state->hf_buffer);
    buffer_init(&state->uhf_buffer);

    return state;
}

static void state_free(RealityClockState* state) {
    free(state);
}

int32_t reality_clock_app(void* p) {
    UNUSED(p);

    RealityClockState* state = state_alloc();
    FuriMessageQueue* event_queue = furi_message_queue_alloc(INPUT_QUEUE_SIZE, sizeof(InputEvent));

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, render_callback, state);
    view_port_input_callback_set(view_port, input_callback, event_queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    /* Open notification service and cast to internal type for brightness access */
    state->notification = (NotificationAppInternal*)furi_record_open(RECORD_NOTIFICATION);

    /* Save original brightness and use it as starting point (don't override system setting) */
    state->original_brightness = state->notification->settings.display_brightness;

    /* Convert system brightness (0.0-1.0) to our percentage (0-100), rounded to nearest 5% step */
    uint8_t system_brightness_pct = (uint8_t)(state->original_brightness * 100.0f);
    state->brightness = (system_brightness_pct / BRIGHTNESS_STEP) * BRIGHTNESS_STEP;
    if(state->brightness > BRIGHTNESS_MAX) state->brightness = BRIGHTNESS_MAX;

    /* Enable always-on backlight (keeps current brightness, just prevents auto-off) */
    notification_message(
        (NotificationApp*)state->notification, &sequence_display_backlight_enforce_on);

    /* Set up periodic brightness refresh to prevent firmware from reverting after ~1 hour */
    state->brightness_refresh_time = furi_get_tick() + BRIGHTNESS_REFRESH_MS;

#ifdef DEBUG_MODE
    /* SubGHz radio is already initialized by the system
     * We just need to wake it from sleep mode before use */
    furi_hal_subghz_reset();
    furi_hal_subghz_idle();

    /* Initialize ADC for temperature sensor */
    state->adc_handle = furi_hal_adc_acquire();
    if(state->adc_handle) {
        /* Configure for internal temperature sensor
         * Requires slower sampling time for accurate readings */
        furi_hal_adc_configure_ex(
            state->adc_handle,
            FuriHalAdcScale2048,
            FuriHalAdcClockSync64,
            FuriHalAdcOversample64,
            FuriHalAdcSamplingtime247_5);
    }

#ifdef DEBUG_LOG_TO_SD
    /* Initialize SD card logging */
    debug_log_init(state);
#endif
#endif

    InputEvent event;

    while(state->is_running) {
        /* Update readings */
        update_readings(state);
        view_port_update(view_port);

        /*
         * Periodically reapply brightness to prevent firmware from reverting it.
         * The notification system has an internal timer that can reset brightness
         * to system defaults after a timeout period (~1 hour).
         */
        if(furi_get_tick() >= state->brightness_refresh_time) {
            apply_brightness(state, state->brightness);
            state->brightness_refresh_time = furi_get_tick() + BRIGHTNESS_REFRESH_MS;
        }

        /* Dynamic sample rate: faster during calibration */
        uint32_t interval = state->is_calibrated ? SAMPLE_INTERVAL_NORMAL_MS :
                                                   SAMPLE_INTERVAL_CALIB_MS;

        if(furi_message_queue_get(event_queue, &event, interval) == FuriStatusOk) {
            process_input(state, &event);
        }
    }

#ifdef DEBUG_MODE
#ifdef DEBUG_LOG_TO_SD
    /* Close SD card logging */
    debug_log_close(state);
#endif

    /* Release ADC handle */
    if(state->adc_handle) {
        furi_hal_adc_release(state->adc_handle);
        state->adc_handle = NULL;
    }

    /* Put SubGHz radio to sleep */
    furi_hal_subghz_sleep();
#endif

    /* Restore original brightness and default backlight behavior on exit */
    state->notification->settings.display_brightness = state->original_brightness;
    notification_message(
        (NotificationApp*)state->notification, &sequence_display_backlight_enforce_auto);
    furi_record_close(RECORD_NOTIFICATION);

    gui_remove_view_port(gui, view_port);
    furi_record_close(RECORD_GUI);

    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    state_free(state);

    return 0;
}
