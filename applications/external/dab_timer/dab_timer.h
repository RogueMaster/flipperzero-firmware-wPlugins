#pragma once

#include <datetime/datetime.h>
#include <dolphin/dolphin.h>
#include <furi.h>
#include <furi_hal.h>
#include <gui/elements.h>
#include <gui/gui.h>
#include <input/input.h>
#include <locale/locale.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#if __has_include(<cfw/cfw.h>)
#include <cfw/cfw.h>
#endif

#define TIME_LEN     12
#define DATE_LEN     14
#define MERIDIAN_LEN 3

typedef struct {
    uint8_t x;
    uint8_t y;
} DabTimerClockPoint;

typedef enum {
    FaceStylePwn,
    FaceStyleOriginal,
    FaceStyleOriginalSmall,
    FaceStyleCircle,
    FaceStyleBinary,
    FaceStylePwnInverted,
    FaceStyleOriginalInverted,
    FaceStyleOriginalSmallInverted,
    FaceStyleCircleInverted,
    FaceStyleBinaryInverted,
    FaceStyleCount,
} FaceStyle;

typedef enum {
    SoundAlertOff,
    SoundAlertByMin,
    SoundAlertMario,
    SoundAlertGoGoPoRa,
    SoundAlertCont,
    SoundAlertCount,
} SoundAlert;

typedef enum {
    DabTimerReady,
    DabTimerRunning,
    DabTimerPaused,
} DabTimerRunState;

typedef enum {
    DabTimerCodeIdle,
    DabTimerCodeFirstUp,
    DabTimerCodeSecondUp,
    DabTimerCodeFirstDown,
    DabTimerCodeSecondDown,
    DabTimerCodeFirstLeft,
    DabTimerCodeFirstRight,
    DabTimerCodeSecondLeft,
    DabTimerCodeSecondRight,
    DabTimerCodeExitBack,
    DabTimerCodeBackPending,
} DabTimerCodeState;

typedef enum {
    DabTimerFaceAngry,
    DabTimerFaceAwake,
    DabTimerFaceBored,
    DabTimerFaceBroken,
    DabTimerFaceCool,
    DabTimerFaceDebug,
    DabTimerFaceDemotivated,
    DabTimerFaceExcited,
    DabTimerFaceFriend,
    DabTimerFaceGrateful,
    DabTimerFaceHappy,
    DabTimerFaceIntense,
    DabTimerFaceLonely,
    DabTimerFaceLookLeft,
    DabTimerFaceLookLeftHappy,
    DabTimerFaceLookRight,
    DabTimerFaceLookRightHappy,
    DabTimerFaceMotivated,
    DabTimerFaceSad,
    DabTimerFaceSleep2,
    DabTimerFaceSleep,
    DabTimerFaceSmart,
    DabTimerFaceUpload1,
    DabTimerFaceUpload2,
    DabTimerFaceUpload,
    DabTimerFaceCount,
} DabTimerEmotiveFace;

typedef enum {
    DabTimerAlarmWaiting,
    DabTimerAlarmFirstPlayed,
    DabTimerAlarmSecondPlayed,
    DabTimerAlarmFinished,
} DabTimerAlarmPhase;

typedef enum {
    DabTimerActionNone = 0,
    DabTimerActionStartStop = 1 << 0,
    DabTimerActionMinute = 1 << 1,
    DabTimerActionMarioFirst = 1 << 2,
    DabTimerActionMarioSecond = 1 << 3,
    DabTimerActionMarioThird = 1 << 4,
    DabTimerActionRangersFirst = 1 << 5,
    DabTimerActionRangersSecond = 1 << 6,
    DabTimerActionRangersThird = 1 << 7,
    DabTimerActionSilent = 1 << 8,
    DabTimerActionRainbow = 1 << 9,
    DabTimerActionSuccess = 1 << 10,
    DabTimerActionXp = 1 << 11,
    DabTimerActionSaveGameMode = 1 << 12,
} DabTimerAction;

typedef enum {
    DabTimerInputIgnored,
    DabTimerInputChanged,
    DabTimerInputExit,
} DabTimerInputResult;

typedef enum {
    DabTimerFlagInput = 1 << 0,
    DabTimerFlagTick = 1 << 1,
    DabTimerFlagRedraw = 1 << 2,
    DabTimerFlagExit = 1 << 3,
} DabTimerThreadFlag;

typedef struct {
    DateTime datetime;
    LocaleDateFormat date_format;
    LocaleTimeFormat time_format;
    DabTimerRunState run_state;
    FaceStyle face_style;
    SoundAlert sound_alert;
    DabTimerEmotiveFace emotive_face;
    uint32_t elapsed_seconds;
    uint32_t alert_time;
    bool easter_egg;
    bool game_mode;
    char time_string[TIME_LEN];
    char date_string[DATE_LEN];
    char circle_date_string[DATE_LEN];
    char meridian_string[MERIDIAN_LEN];
    char timer_string[20];
    char alert_string[11];
} DabTimerModel;

/* Owned by the app thread; only the separately published model is shared with GUI. */
typedef struct {
    DabTimerModel model;
    DabTimerCodeState code_state;
    DabTimerAlarmPhase alarm_phase;
    uint32_t last_tick;
    uint32_t tick_frequency;
    uint32_t fractional_ticks;
    uint32_t last_minute;
    uint32_t last_continuous_second;
    uint32_t last_xp_tick;
    bool xp_tick_valid;
} DabTimerState;

typedef struct {
    FuriMutex* mutex;
    FuriMessageQueue* event_queue;
    FuriThreadId thread_id;
    DabTimerModel model;
} DabTimerApp;

const NotificationSequence dab_timer_alert_silent = {
    &message_vibro_on,
    &message_red_255,
    &message_green_255,
    &message_blue_255,
    &message_display_backlight_on,
    &message_vibro_off,
    &message_display_backlight_off,
    &message_delay_50,
    &message_display_backlight_on,
    NULL,
};
const NotificationSequence dab_timer_alert_pr1 = {
    &message_vibro_on,
    &message_red_255,
    &message_green_255,
    &message_blue_255,
    &message_display_backlight_on,
    &message_note_g5,
    &message_delay_100,
    &message_delay_100,
    &message_delay_50,
    &message_sound_off,
    &message_vibro_off,
    &message_display_backlight_off,
    &message_delay_50,
    &message_display_backlight_on,
    &message_note_g5,
    &message_delay_100,
    &message_delay_100,
    &message_delay_50,
    &message_sound_off,
    NULL,
};
const NotificationSequence dab_timer_alert_pr2 = {
    &message_vibro_on,
    &message_note_fs5,
    &message_delay_100,
    &message_delay_100,
    &message_sound_off,
    &message_display_backlight_off,
    &message_vibro_off,
    &message_delay_50,
    &message_note_g5,
    &message_delay_100,
    &message_delay_100,
    &message_sound_off,
    &message_display_backlight_on,
    &message_delay_50,
    &message_note_a5,
    &message_delay_100,
    &message_delay_100,
    &message_sound_off,
    NULL,
};
const NotificationSequence dab_timer_alert_pr3 = {
    &message_display_backlight_off,
    &message_note_g5,
    &message_delay_100,
    &message_delay_100,
    &message_sound_off,
    &message_delay_50,
    &message_red_255,
    &message_green_255,
    &message_blue_255,
    &message_display_backlight_on,
    &message_delay_100,
    NULL,
};
const NotificationSequence dab_timer_alert_mario1 = {
    &message_vibro_on,
    &message_red_255,
    &message_green_255,
    &message_blue_255,
    &message_display_backlight_on,
    &message_note_e5,
    &message_delay_100,
    &message_delay_100,
    &message_delay_50,
    &message_sound_off,
    &message_note_e5,
    &message_delay_100,
    &message_delay_100,
    &message_delay_50,
    &message_sound_off,
    &message_vibro_off,
    &message_display_backlight_off,
    &message_delay_100,
    &message_display_backlight_on,
    &message_delay_100,
    &message_note_e5,
    &message_delay_100,
    &message_delay_100,
    &message_delay_50,
    &message_sound_off,
    NULL,
};
const NotificationSequence dab_timer_alert_mario2 = {
    &message_vibro_on,
    &message_display_backlight_off,
    &message_delay_100,
    &message_display_backlight_on,
    &message_delay_100,
    &message_note_c5,
    &message_delay_100,
    &message_delay_100,
    &message_sound_off,
    &message_display_backlight_off,
    &message_vibro_off,
    &message_delay_50,
    &message_note_e5,
    &message_delay_100,
    &message_delay_100,
    &message_sound_off,
    &message_display_backlight_on,
    NULL,
};
const NotificationSequence dab_timer_alert_mario3 = {
    &message_display_backlight_off,
    &message_note_g5,
    &message_delay_100,
    &message_delay_100,
    &message_delay_100,
    &message_delay_100,
    &message_sound_off,
    &message_delay_50,
    &message_red_255,
    &message_green_255,
    &message_blue_255,
    &message_display_backlight_on,
    &message_delay_100,
    &message_note_g4,
    &message_delay_100,
    &message_delay_100,
    &message_delay_100,
    &message_delay_100,
    &message_sound_off,
    NULL,
};
const NotificationSequence dab_timer_alert_perMin = {
    &message_note_g5,
    &message_delay_100,
    &message_delay_50,
    &message_sound_off,
    &message_delay_10,
    &message_note_g4,
    &message_delay_50,
    &message_delay_10,
    &message_delay_10,
    &message_sound_off,
    NULL,
};
const NotificationSequence dab_timer_alert_startStop = {
    &message_red_255,
    &message_green_255,
    &message_blue_255,
    &message_note_d6,
    &message_delay_100,
    &message_delay_10,
    &message_delay_10,
    &message_sound_off,
    NULL,
};

const NotificationMessage message_red_127 = {
    .type = NotificationMessageTypeLedRed,
    .data.led.value = 0x7F,
};

const NotificationMessage message_green_127 = {
    .type = NotificationMessageTypeLedGreen,
    .data.led.value = 0x7F,
};

const NotificationMessage message_blue_127 = {
    .type = NotificationMessageTypeLedBlue,
    .data.led.value = 0x7F,
};

const NotificationSequence sequence_rainbow = {
    &message_red_255,   &message_green_0,   &message_blue_0,
    &message_delay_250, &message_red_255,   &message_green_127,
    &message_blue_0,    &message_delay_250, &message_red_255,
    &message_green_255, &message_blue_0,    &message_delay_250,
    &message_red_127,   &message_green_255, &message_blue_0,
    &message_delay_250, &message_red_0,     &message_green_255,
    &message_blue_0,    &message_delay_250, &message_red_0,
    &message_green_255, &message_blue_127,  &message_delay_250,
    &message_red_0,     &message_green_255, &message_blue_255,
    &message_delay_250, &message_red_0,     &message_green_127,
    &message_blue_255,  &message_delay_250, &message_red_0,
    &message_green_0,   &message_blue_255,  &message_delay_250,
    &message_red_127,   &message_green_0,   &message_blue_255,
    &message_delay_250, &message_red_255,   &message_green_0,
    &message_blue_255,  &message_delay_250, &message_red_255,
    &message_green_0,   &message_blue_127,  &message_delay_250,
    &message_red_127,   &message_green_127, &message_blue_127,
    &message_delay_250, &message_red_255,   &message_green_255,
    &message_blue_255,  &message_delay_250, NULL,
};
