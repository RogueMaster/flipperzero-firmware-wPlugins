#pragma once

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/view.h>
#include <core/timer.h>
#include <gui/modules/variable_item_list.h>

#define LTAG "FujitsuRemote"

typedef enum {
    FujiAirMode_Auto,
    FujiAirMode_Cool,
    FujiAirMode_Dry,
    FujiAirMode_Fan,
    FujiAirMode_Heat
} FujiAirMode;

typedef enum {
    FujiAirTimerModes_NOT_SET,
    FujiAirTimerModes_ON,
    FujiAirTimerModes_OFF,
    FujiAirTimerModes_ON_OFF,
    FujiAirTimerModes_OFF_ON
} FujiAirTimerMode;

extern const uint8_t FujiAir_C_MIN_TEMP;
extern const uint8_t FujiAir_C_MAX_TEMP;
extern const uint8_t FujiAir_C_MAX_FAN_SPEED; /* 0 means AUTO */

/**
 * @brief Holds the current state of the air conditioner
 */
typedef struct {
    FujiAirMode mode;
    uint8_t temperature;
    uint8_t fan_speed;
    uint32_t sleep_timer;
    uint32_t on_timer;
    uint32_t off_timer;
    FujiAirTimerMode on_off_timer_mode;
    bool is_powered;
    bool swing;
    bool has_alarm;
    bool economy;
} FujitsuAirState;

typedef enum {
    FujitsuViewId_Page1_MainScreen,
    FujitsuViewId_Page2,
    FujitsuViewId_Page3,
    FujitsuViewId_Page_Sleep,
    FujitsuViewId_Page_Timer,
    FujitsuView_Count
} FujitsuViewId;

typedef struct {
    uint32_t gui_event_delay;
    FuriTimer* view_page2_timer1;
    FuriTimer* view_page3_timer1;
    FuriTimer* view_page3_timer2;
} FujitsuRemoteAppTimers;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    FujitsuAirState* air_conditioner_state;
    VariableItemList* variable_item_list_sleep;
    VariableItemList* variable_item_list_timer;

    View* view_page1;
    View* view_page2;
    View* view_page3;
    View* view_page_timer;
    View* view_page_sleep;
    FujitsuRemoteAppTimers* timers;
} FujitsuRemoteApp;

typedef enum {
    FujitsuRemoteAppEvent_OpenPage1,
    FujitsuRemoteAppEvent_OpenPage2,
    FujitsuRemoteAppEvent_OpenPage3,
    FujitsuRemoteAppEvent_OpenPageSleep,
    FujitsuRemoteAppEvent_OpenPageTimer,

    FujitsuRemoteAppEvent_TempUp,
    FujitsuRemoteAppEvent_TempDown,
    FujitsuRemoteAppEvent_TempChangeEnd,
    FujitsuRemoteAppEvent_PowerToggle,
    FujitsuRemoteAppEvent_FanSpeedNext,
    FujitsuRemoteAppEvent_AutoSwingToggle,
    FujitsuRemoteAppEvent_ModeNext,
    FujitsuRemoteAppEvent_EconomyModeToggle,
    FujitsuRemoteAppEvent_PowerModeToggle,
    FujitsuRemoteAppEvent_SetSwingChanged,
    FujitsuRemoteAppEvent_CancelTimer,
    FujitsuRemoteAppEvent_TimerChanged,
    FujitsuRemoteAppEvent_SleepChanged
} FujitsuRemoteAppEvent;
