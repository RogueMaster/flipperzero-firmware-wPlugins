#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <bt/bt_service/bt.h>
#include <storage/storage.h>
#include <extra_profiles/hid_profile.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#define MAX_KEYFRAMES           128
#define MOUSE_MOVE_SHORT        5
#define MOUSE_MOVE_LONG         20
#define COUNTDOWN_SECS          3
#define TIMER_TICK_MS           10
#define CLICK_HOLD_MS           50
#define DURATION_MIN            1
#define DURATION_MAX            60
#define DURATION_DEFAULT        5
#define SHUFFLER_CLICKS_DEFAULT 25
#define SHUFFLER_CLICKS_MIN     1
#define SHUFFLER_CLICKS_MAX     MAX_KEYFRAMES
#define SPAM_CLICKS_DEFAULT     10
#define SPAM_CLICKS_MIN         1
#define SPAM_CLICKS_MAX         255
#define SPAM_INTERVAL_MS        100

typedef enum {
    TransportUsb,
    TransportBle,
} Transport;

typedef enum {
    ModeRecorder,
    ModeShuffler,
    ModeSpamClick,
    ModeComputerMouse,
    ModeCount,
} AppMode;

typedef enum {
    StateModeSelect,
    StateTemplateSelect,
    StateShufflerClickCount,
    StateCountdown,
    StateRecording,
    StateTuneTrack,
    StatePositioning,
    StatePlayModeSelect,
    StatePlayback,
    StatePaused,
    StateSpamClickConfig,
    StateSpamClicking,
    StateComputerMouse,
    StateQuitConfirm,
    StateAbout,
} AppStateEnum;

typedef enum {
    LoopOnce,
    LoopForever,
} LoopMode;

typedef enum {
    PauseMenuContinue,
    PauseMenuReset,
    PauseMenuQuit,
    PauseMenuCount,
} PauseMenuItem;

typedef enum {
    ClickLeft,
    ClickRight,
    ClickBoth,
} ClickType;

typedef enum {
    SpamBtnLeft,
    SpamBtnMiddle,
    SpamBtnRight,
    SpamBtnCount,
} SpamButton;

typedef struct {
    uint32_t timestamp_ms;
    ClickType click_type;
} Keyframe;

typedef struct {
    uint8_t duration_sec;
    uint8_t num_keyframes;
    Keyframe keyframes[MAX_KEYFRAMES];
} Recording;

typedef struct {
    // State
    AppStateEnum state;
    uint8_t menu_selection; // 0..ModeCount-1
    Transport transport;
    LoopMode loop_mode;

    // Template selection
    uint8_t selected_duration;

    // Shuffler
    uint8_t shuffler_click_count;

    // Countdown
    uint8_t countdown_value;

    // Recording
    Recording recording;
    uint32_t record_start_tick;
    uint32_t elapsed_ms;

    // Tune track (original keyframes for restoring)
    Keyframe original_keyframes[MAX_KEYFRAMES];
    uint8_t original_duration_sec;
    uint8_t tuned_duration_sec;

    // Playback
    uint32_t playback_start_tick;
    uint32_t playback_ms;
    uint8_t playback_keyframe_idx;
    bool playback_click_pending;
    ClickType playback_click_type;
    uint32_t click_press_tick;

    // Pause menu
    PauseMenuItem pause_selection;

    // Menu (quit/about)
    uint8_t quit_selection; // 0=Resume 1=About 2=Quit
    AppStateEnum state_before_quit;

    // Direction indicators
    bool dir_up;
    bool dir_down;
    bool dir_left;
    bool dir_right;

    // Play mode selection
    uint8_t play_mode_selection; // 0=Once, 1=Loop

    // Spam click
    uint8_t spam_click_count;
    SpamButton spam_button;
    uint8_t spam_clicks_done;
    bool spam_paused;

    // Computer mouse indicators
    bool mouse_left_pressed;
    bool mouse_right_pressed;

    // Misc
    bool ble_connected;
    bool running;

    // OS resources
    ViewPort* view_port;
    Gui* gui;
    FuriMessageQueue* event_queue;
    FuriTimer* timer;
    FuriHalUsbInterface* usb_mode_prev;
    Bt* bt;
    FuriHalBleProfileBase* ble_profile;
    NotificationApp* notifications;
} App;

// Entry point
int32_t mouse_click_recorder_app(void* p);
