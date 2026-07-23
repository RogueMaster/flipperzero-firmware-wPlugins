#pragma once

#include <furi.h>

/* ── Monitored frequencies ── */
#define MONITOR_FREQ_COUNT 4

static const uint32_t MONITOR_FREQS[MONITOR_FREQ_COUNT] = {
    315000000,
    433920000,
    868350000,
    915000000,
};

static const char* const MONITOR_NAMES[MONITOR_FREQ_COUNT] = {
    "315MHz",
    "433MHz",
    "868MHz",
    "915MHz",
};

/* ── Detection thresholds (dBm) ── */
#define RSSI_THRESHOLD_SUSPICIOUS (-60.0f)
#define RSSI_THRESHOLD_JAMMER     (-40.0f)

/* Rolling RSSI window depth */
#define RSSI_WINDOW_SIZE 8

/* How many consecutive scan cycles above threshold before alerting */
#define ALERT_CONSECUTIVE_COUNT 3

/* Worker dwell time per frequency in milliseconds */
#define DWELL_TIME_MS 200

/* UI refresh period in milliseconds */
#define UI_REFRESH_MS 300

/* ── Alert mode ── */
typedef enum {
    AlertModeSilent,
    AlertModeBlink,
    AlertModeVibrate,
} AlertMode;

/* ── Per-frequency status ── */
typedef enum {
    FreqStatusOk,
    FreqStatusSuspicious,
    FreqStatusJammer,
} FreqStatus;

/* ── Shared state (guarded by mutex in JammerApp) ── */
typedef struct {
    /* Last RSSI reading per frequency */
    float rssi[MONITOR_FREQ_COUNT];
    /* Rolling max RSSI window per frequency */
    float rssi_window[MONITOR_FREQ_COUNT][RSSI_WINDOW_SIZE];
    uint8_t window_pos[MONITOR_FREQ_COUNT];
    float window_max[MONITOR_FREQ_COUNT];
    /* Consecutive alert cycle counter per frequency */
    uint8_t consecutive[MONITOR_FREQ_COUNT];
    /* Derived status */
    FreqStatus status[MONITOR_FREQ_COUNT];
    /* Index of worst frequency currently alerting (-1 = none) */
    int8_t alert_freq_idx;
    /* Settings */
    float threshold_suspicious;
    float threshold_jammer;
    AlertMode alert_mode;
    /* Hardware error — set by worker if CC1101 init fails */
    bool hw_error;
} JammerState;

/* ── View model (copied into view for draw callback) ── */
typedef struct {
    float rssi[MONITOR_FREQ_COUNT];
    float window_max[MONITOR_FREQ_COUNT];
    FreqStatus status[MONITOR_FREQ_COUNT];
    int8_t alert_freq_idx;
    float threshold_suspicious;
    float threshold_jammer;
    bool hw_error;
} JammerViewModel;
