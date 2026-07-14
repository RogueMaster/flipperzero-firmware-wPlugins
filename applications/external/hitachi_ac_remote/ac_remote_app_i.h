#pragma once

#include <flipper_format/flipper_format.h>
#include <gui/gui.h>
#include <gui/scene_manager.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/view_stack.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/variable_item_list.h>
#include <hvac_hitachi.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>

#include "ac_remote_app.h"
#include "ac_remote_custom_event.h"
#include "hitachi_ac_remote_icons.h"
#include "scenes/ac_remote_scene.h"
#include "views/ac_remote_panel.h"

#define AC_REMOTE_APP_SETTINGS APP_DATA_PATH("settings.txt")

typedef enum {
    PowerButtonOff,
    PowerButtonOn,
    POWER_STATE_MAX,
} PowerButtonState;

typedef enum {
    ModeButtonAuto,
    ModeButtonHeating,
    ModeButtonCooling,
    ModeButtonDehumidifying,
    ModeButtonFan,
    MODE_BUTTON_STATE_MAX,
} ModeButtonState;

typedef enum {
    FanSpeedButtonLow,
    FanSpeedButtonMedium,
    FanSpeedButtonHigh,
    FAN_SPEED_BUTTON_STATE_MAX,
} FanSpeedButtonState;

typedef enum {
    VaneButtonPos0,
    VaneButtonPos1,
    VaneButtonPos2,
    VaneButtonPos3,
    VaneButtonPos4,
    VaneButtonPos5,
    VaneButtonPos6,
    VaneButtonAuto,
    VANE_BUTTON_STATE_MAX,
} VaneButtonState;

typedef enum {
    LabelStringTemp,
    LabelStringTimerOnHour,
    LabelStringTimerOnMinute,
    LabelStringTimerOffHour,
    LabelStringTimerOffMinute,
    LABEL_STRING_POOL_SIZE,
} LabelStringPoolIndex;

typedef enum {
    TimerStateStopped,
    TimerStatePaused,
    TimerStateRunning,
    TIMER_STATE_COUNT,
} TimerState;

typedef enum {
    SettingsSideA,
    SettingsSideB,
    SETTINGS_SIDE_COUNT,
} SettingsSide;

typedef enum {
    SettingsTimerStep1min,
    SettingsTimerStep2min,
    SettingsTimerStep3min,
    SettingsTimerStep5min,
    SettingsTimerStep10min,
    SettingsTimerStep15min,
    SettingsTimerStep30min,
    SETTINGS_TIMER_STEP_COUNT,
} SettingsTimerStep;

typedef struct {
    uint32_t on;
    uint32_t off;
} TimerOnOffState;

typedef struct {
    uint32_t power;
    uint32_t mode;
    uint32_t temperature;
    uint32_t fan;
    uint32_t vane;
    uint32_t timer_state;
    TimerOnOffState timer_preset;
    TimerOnOffState timer_pause;
    uint32_t timer_on_expires_at;
    uint32_t timer_off_expires_at;
    uint32_t side;
    uint32_t timer_step;
    bool allow_auto;
} ACRemoteAppSettings;

typedef struct {
    uint8_t hours;
    uint8_t minutes;
    uint16_t minutes_only;
    char hours_display[4];
    char minutes_display[4];
} ACRemoteTimerState;

typedef struct {
    ACRemoteTimerState timer_on;
    ACRemoteTimerState timer_off;
    char temp_display[4];
} ACRemoteTransientState;

struct AC_RemoteApp {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    ACRemotePanel* panel_main;
    ACRemotePanel* panel_sub;
    VariableItemList* vil_settings;
    DialogEx* dex_reset_confirm;
    ACRemoteAppSettings app_state;
    ACRemoteTransientState transient_state;
    HvacHitachiContext* protocol;
};

typedef enum {
    AC_RemoteAppViewMain,
    AC_RemoteAppViewSub,
    AC_RemoteAppViewSettings,
    AC_RemoteAppViewResetConfirm,
} AC_RemoteAppView;

void ac_remote_reset_settings(AC_RemoteApp* const app);

#define LABEL_STRING_SIZE sizeof(ac_remote->label_string_pool[0])
