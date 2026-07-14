#pragma once

#include <stdio.h>
#include <string.h>

#include <dialogs/dialogs.h>
#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>

#include <flipper_format/flipper_format.h>
#include <storage/storage.h>

#define TAG "usb_hid_autofire"

// Uncomment to be able to make a screenshot
// #define USB_HID_AUTOFIRE_SCREENSHOT

#define AUTOFIRE_DELAY_MIN_MS            5U
#define AUTOFIRE_DELAY_MAX_MS            10000U
#define AUTOFIRE_DELAY_STEP_MS           10U
#define AUTOFIRE_DELAY_ACCEL_MEDIUM_MS   50U
#define AUTOFIRE_DELAY_ACCEL_FAST_MS     100U
#define AUTOFIRE_REPEAT_MEDIUM_THRESHOLD 3U
#define AUTOFIRE_REPEAT_FAST_THRESHOLD   8U
#define AUTOFIRE_DELAY_DEFAULT_MS        10U
#define AUTOFIRE_PRESET_SLOW_MS          250U
#define AUTOFIRE_PRESET_MEDIUM_MS        120U
#define AUTOFIRE_PRESET_FAST_MS          70U
#define HIGH_CPS_CONFIRM_THRESHOLD_X10   120U
#define UI_REFRESH_PERIOD_MS             250U
#define EVENT_DRAIN_MAX_COUNT            32U
#define SETTINGS_SAVE_DEBOUNCE_MS        500U

#define USB_HID_AUTOFIRE_SETTINGS_PATH      APP_DATA_PATH(".settings")
#define USB_HID_AUTOFIRE_SETTINGS_FILE_TYPE "USB HID Autofire Settings"
#define USB_HID_AUTOFIRE_SETTINGS_VERSION   1U

typedef enum {
    EventTypeInput,
    EventTypeTick,
    EventTypeUiRefresh,
    EventTypeSettingsSave,
} EventType;

typedef enum {
    ClickPhasePress,
    ClickPhaseRelease,
} ClickPhase;

typedef enum {
    AutofireModeMouseLeftClick,
    AutofireModeMouseRightClick,
    AutofireModeKeyboardEnter,
    AutofireModeKeyboardSpace,
} AutofireMode;

typedef enum {
    AutofirePresetCustom,
    AutofirePresetSlow,
    AutofirePresetMedium,
    AutofirePresetFast,
    AutofirePresetCount,
} AutofirePreset;

typedef enum {
    AutofireStartupPolicyPausedOnLaunch,
    AutofireStartupPolicyRestoreLastState,
} AutofireStartupPolicy;

typedef struct {
    union {
        InputEvent input;
    };
    EventType type;
} UsbMouseEvent;

typedef struct {
    FuriMessageQueue* event_queue;
    ViewPort* view_port;
    Gui* gui;
    DialogsApp* dialogs;
    FuriTimer* click_timer;
    FuriTimer* ui_refresh_timer;
    FuriTimer* settings_save_timer;
    FuriHalUsbInterface* usb_mode_prev;
    bool active;
    bool mouse_pressed;
    bool ui_dirty;
    bool settings_dirty;
    bool show_help;
    bool last_active_state;
    uint32_t autofire_delay_ms;
    uint32_t realtime_cps_x10;
    uint32_t last_click_release_tick_ms;
    uint32_t last_click_interval_ms;
    bool adjust_hold_active;
    InputKey adjust_hold_key;
    uint16_t adjust_repeat_count;
    bool mode_hold_active;
    InputKey mode_hold_key;
    bool ok_long_handled;
    bool back_long_handled;
    AutofireMode mode;
    AutofirePreset preset;
    AutofireStartupPolicy startup_policy;
    ClickPhase click_phase;
} UsbHidAutofireApp;

void usb_hid_autofire_input_callback(InputEvent* input_event, void* ctx);
void usb_hid_autofire_timer_callback(void* ctx);
void usb_hid_autofire_ui_timer_callback(void* ctx);
void usb_hid_autofire_settings_save_timer_callback(void* ctx);

uint32_t usb_hid_autofire_delay_clamp(uint32_t delay_ms);
uint32_t usb_hid_autofire_delay_decrease(uint32_t delay_ms, uint32_t step_ms);
uint32_t usb_hid_autofire_delay_increase(uint32_t delay_ms, uint32_t step_ms);
uint32_t usb_hid_autofire_accel_step_ms(uint16_t repeat_count);
uint32_t usb_hid_autofire_config_cps_x10_for_delay(uint32_t delay_ms);

const char* usb_hid_autofire_mode_label(AutofireMode mode);
AutofireMode usb_hid_autofire_next_mode(AutofireMode mode);
AutofireMode usb_hid_autofire_prev_mode(AutofireMode mode);
const char* usb_hid_autofire_preset_label(AutofirePreset preset);
uint32_t usb_hid_autofire_preset_delay_ms(AutofirePreset preset);
AutofirePreset usb_hid_autofire_next_preset(AutofirePreset preset);

bool usb_hid_autofire_mode_is_valid(uint32_t mode_value);
bool usb_hid_autofire_preset_is_valid(uint32_t preset_value);
bool usb_hid_autofire_startup_policy_is_valid(uint32_t startup_policy_value);

void usb_hid_autofire_format_cps(char* out, size_t out_size, uint32_t cps_x10);

void usb_hid_autofire_render_callback(Canvas* canvas, void* ctx);

uint32_t usb_hid_autofire_realtime_cps_x10(const UsbHidAutofireApp* app);
void usb_hid_autofire_reset_cps_tracking(UsbHidAutofireApp* app);
void usb_hid_autofire_drain_event_queue(UsbHidAutofireApp* app, uint8_t max_count);
void usb_hid_autofire_press_mode_control(UsbHidAutofireApp* app);
void usb_hid_autofire_release_mode_control(UsbHidAutofireApp* app);
void usb_hid_autofire_schedule_next_tick(UsbHidAutofireApp* app);
void usb_hid_autofire_start(UsbHidAutofireApp* app);
void usb_hid_autofire_stop(UsbHidAutofireApp* app);
void usb_hid_autofire_tick(UsbHidAutofireApp* app);

void usb_hid_autofire_mark_settings_dirty(UsbHidAutofireApp* app);
bool usb_hid_autofire_settings_load(UsbHidAutofireApp* app);
void usb_hid_autofire_settings_flush_if_dirty(UsbHidAutofireApp* app);

bool usb_hid_autofire_set_mode(UsbHidAutofireApp* app, AutofireMode new_mode);
bool usb_hid_autofire_set_delay(
    UsbHidAutofireApp* app,
    uint32_t new_delay_ms,
    AutofirePreset new_preset);
void usb_hid_autofire_adjust_delay(UsbHidAutofireApp* app, InputKey key, uint32_t step_ms);
void usb_hid_autofire_apply_preset_request(UsbHidAutofireApp* app, AutofirePreset preset);
void usb_hid_autofire_handle_delay_input(UsbHidAutofireApp* app, const InputEvent* input);
void usb_hid_autofire_handle_mode_input(UsbHidAutofireApp* app, const InputEvent* input);
bool usb_hid_autofire_handle_input_event(
    UsbHidAutofireApp* app,
    const InputEvent* input,
    bool* should_exit);
