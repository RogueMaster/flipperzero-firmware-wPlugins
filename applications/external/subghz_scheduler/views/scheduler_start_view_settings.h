#pragma once

typedef enum MenuIndices {
    MenuIndexInterval,
    MenuIndexTiming,
    MenuIndexTxCount,
    MenuIndexTxMode,
    MenuIndexTxDelay,
    MenuIndexTxRadio,
    MenuIndexSelectFile,
    MenuIndexSaveSchedule,
    MenuIndexStartSchedule
} MenuIndices;

typedef enum FileTxType {
    SchedulerFileTypeSingle,
    SchedulerFileTypePlaylist,
    SchedulerFileTypeNum
} FileTxType;
#define FILE_TYPE_COUNT SchedulerFileTypeNum
static const char* const file_type_text[SchedulerFileTypeNum] = {"Single", "Playlist"};

typedef enum SchedulerTxMode {
    SchedulerTxModeNormal,
    SchedulerTxModeImmediate,
    SchedulerTxModeOneShot,
    SchedulerTxModeSettingsNum,
} SchedulerTxMode;
#define TX_MODE_COUNT SchedulerTxModeSettingsNum
static const char* const tx_mode_text[SchedulerTxModeSettingsNum] = {
    "Normal",
    "Immed.",
    "1-Shot",
};

typedef enum SchedulerTimingMode {
    SchedulerTimingModeRelative,
    SchedulerTimingModePrecise,
    SchedulerTimingModeNum
} SchedulerTimingMode;
#define TIMING_MODE_COUNT SchedulerTimingModeNum
static const char* const timing_mode_text[TIMING_MODE_COUNT] = {"Relative", "Precise"};

#define TX_DELAY_STEP_MS 50
#define TX_DELAY_MAX_MS  1000
#define TX_DELAY_COUNT   ((TX_DELAY_MAX_MS / TX_DELAY_STEP_MS) + 1)

#define TX_COUNT 6
static const char* const tx_count_text[TX_COUNT] = {
    "x1",
    "x2",
    "x3",
    "x4",
    "x5",
    "x6",
};

#define RADIO_DEVICE_COUNT 2
static const char* const radio_device_text[RADIO_DEVICE_COUNT] = {
    "Internal",
    "External",
};
