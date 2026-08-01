#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MF_ARDF_CUSTOM_CAPACITY 5U
#define MF_ARDF_SETTING_COUNT   9U
#define MF_ARDF_INTERVAL_COUNT  28U
#define MF_ARDF_CUSTOM_ROW      4U

typedef enum {
    MfArdfViewSettings = 0,
    MfArdfViewClock,
    MfArdfViewRun,
} MfArdfView;

typedef enum {
    MfArdfModeCustom = 0,
    MfArdfModeSprint,
    MfArdfModeStandard,
} MfArdfMode;

typedef enum {
    MfArdfModulationCw = 0,
    MfArdfModulationCwfm,
} MfArdfModulation;

typedef enum {
    MfArdfMessage1 = 0,
    MfArdfMessage2,
    MfArdfMessage3,
    MfArdfMessage4,
    MfArdfMessage5,
    MfArdfMessageS,
    MfArdfMessageMo,
    MfArdfMessageCount,
} MfArdfMessage;

typedef enum {
    MfArdfClockConfirm = 0,
    MfArdfClockSelect,
    MfArdfClockEdit,
} MfArdfClockState;

typedef enum {
    MfArdfClockHours = 0,
    MfArdfClockMinutes,
    MfArdfClockSeconds,
} MfArdfClockField;

typedef enum {
    MfArdfHostActionNone = 0,
    MfArdfHostActionOpenTextInput,
    MfArdfHostActionShowStopConfirmation,
    MfArdfHostActionShowError,
    MfArdfHostActionCloseToRadio,
} MfArdfHostAction;

typedef enum {
    MfArdfErrorNone = 0,
    MfArdfErrorFrequency,
    MfArdfErrorHardware,
    MfArdfErrorConfig,
} MfArdfError;

typedef struct {
    uint8_t mode;
    uint8_t modulation;
    uint8_t message;
    uint8_t interval_index;
    uint8_t light_assistance;
    uint8_t audio_output;
    uint8_t wpm;
    uint8_t selected_row;
    char custom[MF_ARDF_CUSTOM_CAPACITY + 1U];
} MfArdfSettings;

typedef struct {
    uint32_t struct_size;
    uint32_t now_ms;
    uint32_t frequency_hz;
} MfArdfEnterArgs;

typedef struct {
    uint32_t struct_size;
    uint8_t view;
    uint8_t clock_state;
    uint8_t clock_field;
    MfArdfSettings settings;
    bool running;
    bool transmitting;
    bool mark;
    bool ptt;
    bool gpio_owned;
    bool playback_active;
    bool playback_mark;
    bool run_pending;
    uint32_t next_deadline_ms;
    uint32_t segment_start_ms;
    uint8_t host_action;
    uint8_t error;
} MfArdfSnapshot;

typedef struct {
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} MfArdfClockTime;
