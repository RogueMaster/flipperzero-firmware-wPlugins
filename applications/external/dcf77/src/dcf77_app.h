#ifndef __ARHA_FLIPPERAPP_DEMO
#define __ARHA_FLIPPERAPP_DEMO

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_speaker.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/elements.h>
#include <input/input.h>
#include <applications/services/gui/modules/number_input.h>
#include <applications/services/gui/modules/submenu.h>
#include <applications/services/gui/modules/variable_item_list.h>
#include <applications/services/gui/modules/widget.h>

#include <lib/subghz/receiver.h>
#include <lib/subghz/transmitter.h>
#include <lib/subghz/subghz_file_encoder_worker.h>
#include <lib/subghz/devices/cc1101_configs.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <datetime/datetime.h>
#include <furi_hal_resources.h>
#include <furi.h>

#include "dcf77_util.h"
#include "debug_settings.h"
#include "experimental_time.h"
#include "experimental_time_input.h"
#include "radio_clock.h"
#include "radio_clock_pulse.h"
#include "radio_clock_protocol.h"
#include "subghz_settings.h"
#include "tx_ratio.h"

// the TAG is used for displaying a relevant prefix in logs. update it.
#define TAG                        "__ARHA_FLIPPERAPP"
#define DCF77_SECONDS_PER_MINUTE   60
#define DCF77_LPTIM_HZ             32768U
#define DCF77_FULL_SECOND_TICKS    DCF77_LPTIM_HZ
#define DCF77_ZERO_HIGH_TICKS      ((DCF77_LPTIM_HZ * 9U) / 10U)
#define DCF77_ONE_HIGH_TICKS       ((DCF77_LPTIM_HZ * 8U) / 10U)
#define DCF77_MIN_TIMER_TICKS      8U
#define STARTUP_SCREEN_MS          1000U
#define SYNC_ON_START              0
#define DEBUG_SYNC_FRAME           0
#define SUBGHZ_OOK_INVERT          0
#define LF_FREQ_DEFAULT            77500U
#define LF_FREQ_MIN                20000U
#define LF_FREQ_MAX                200000U
#define LF_FREQ_STEP               500U
#define DCF77_LF_FREQ_ITEM_VALUES  255U
#define GPIO_BASEBAND_PIN_DEFAULT  7U
#define GPIO_RF_PIN_NONE           0U
#define GPIO_RF_DUTY_CYCLE_DEFAULT 50U
// #define TIME_ZERO 15
// #define TIME_ONE 5

typedef enum {
    AppScreenStartup,
    AppScreenMenu,
    AppScreenAdvancedMenu,
    AppScreenTx,
    AppScreenExperimentalTimeSettings,
    AppScreenPresetTimeInput,
    AppScreenLfSettings,
    AppScreenTxRatioSettings,
    AppScreenSubGhzSettings,
    AppScreenSubGhzFreqInput,
    AppScreenDebugSettings,
    AppScreenGpioRfWarning,
    AppScreenAbout,
    AppScreenEgg,
} AppScreen;

typedef enum {
    Dcf77ViewStartup,
    Dcf77ViewMenu,
    Dcf77ViewAdvancedMenu,
    Dcf77ViewTx,
    Dcf77ViewExperimentalTimeSettings,
    Dcf77ViewPresetTimeInput,
    Dcf77ViewLfSettings,
    Dcf77ViewTxRatioSettings,
    Dcf77ViewSubGhzSettings,
    Dcf77ViewSubGhzFreqInput,
    Dcf77ViewDebugSettings,
    Dcf77ViewGpioRfWarning,
    Dcf77ViewAbout,
    Dcf77ViewEgg,
} Dcf77View;

typedef enum {
    Dcf77MenuItemStart,
    Dcf77MenuItemLfSettings,
    Dcf77MenuItemAdvanced,
    Dcf77MenuItemAbout,
} Dcf77MenuItem;

typedef enum {
    Dcf77AdvancedMenuItemSubGhzSettings,
    Dcf77AdvancedMenuItemDebugSettings,
    Dcf77AdvancedMenuItemExperimentalTimeSettings,
    Dcf77AdvancedMenuItemTxRatioSettings,
} Dcf77AdvancedMenuItem;

typedef enum {
    SubGhzSignalModeDisabled,
    SubGhzSignalModeOok,
    SubGhzSignalModeFsk,
    SubGhzSignalModeFskFull,
} SubGhzSignalMode;

typedef enum {
    Dcf77LedColorNone,
    Dcf77LedColorRed,
    Dcf77LedColorOrange,
    Dcf77LedColorGreen,
    Dcf77LedColorBlue,
    Dcf77LedColorYellow,
    Dcf77LedColorCyan,
    Dcf77LedColorMagenta,
    Dcf77LedColorWhite,
    Dcf77LedColorDarkViolet,
    Dcf77LedColorPink,
    Dcf77LedColorCount,
} Dcf77LedColor;

typedef enum {
    Dcf77ScreenModeDebug,
    Dcf77ScreenModeUser,
    Dcf77ScreenModeCount,
} Dcf77ScreenMode;

typedef struct AppFSM {
    uint32_t lf_freq;
    uint8_t current_signal;
    uint32_t signal_freqs[RadioClockSignalCount];

    uint8_t bit_number; // 0 - 59
    uint8_t bit_value; // 0/1/marker for byte-debug screens
    uint8_t current_pulse;
    volatile bool output_state;
    volatile bool output_dirty;
    uint8_t dcf77_message[8];
    uint8_t next_message[8];
    RadioClockPulse pulse_frame[DCF77_SECONDS_PER_MINUTE];
    RadioClockPulse next_pulse_frame[DCF77_SECONDS_PER_MINUTE];
    RadioClockSecondWaveform waveform_frame[DCF77_SECONDS_PER_MINUTE];
    RadioClockSecondWaveform next_waveform_frame[DCF77_SECONDS_PER_MINUTE];
    DateTime current_minute_dt;
    DateTime next_minute_dt;
    Dcf77ExperimentalTimeSettings experimental_time_settings;
    Dcf77ExperimentalTimeRuntime experimental_time_runtime;
    uint8_t experimental_time_speed_index;
    volatile uint8_t scheduler_second;
    volatile uint8_t scheduler_segment;
    volatile bool scheduler_ready;
    volatile bool scheduler_synced;
    volatile bool startup_marker_wrap_pending;
    volatile bool next_minute_prepare_pending;
    volatile bool next_minute_ready;
    FuriTimer* test_timer;
    uint8_t test_phase;

    bool debug_flag;
    bool lf_ready;
    bool subghz_ready;
    bool subghz_timeout_elapsed;
    bool gpio_rf_running;
    volatile bool subghz_async_tx;
    volatile bool subghz_output;
    volatile bool subghz_tone_phase;
    uint8_t subghz_signal_mode;
    uint8_t subghz_fsk_tone_index;
    uint8_t subghz_tx_timeout_index;
    uint8_t subghz_band_count;
    uint8_t subghz_band_index;
    uint8_t speaker_value_index;
    uint8_t gpio_baseband_pin_number;
    uint8_t gpio_rf_pin_number;
    uint8_t gpio_rf_pending_pin_number;
    uint8_t tx_ratio_x;
    uint8_t tx_ratio_y_index;
    uint8_t tx_progress_second;
    uint8_t gpio_rf_duty_cycle;
    uint8_t led_color_index;
    uint8_t screen_mode;
    volatile bool tx_frame_enabled;
    volatile bool next_tx_frame_enabled;
    uint32_t subghz_band_starts[DCF77_SUBGHZ_MAX_BANDS];
    uint32_t subghz_band_ends[DCF77_SUBGHZ_MAX_BANDS];
    uint32_t subghz_band_freqs[DCF77_SUBGHZ_MAX_BANDS];
    bool tx_active;
    uint8_t screen;
    uint32_t startup_deadline_tick;
    uint32_t last_tx_refresh_tick;
    uint32_t tx_second_start_tick;
    uint32_t tx_session_start_tick;

    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    Submenu* advanced_menu;
    VariableItemList* lf_settings;
    VariableItemList* tx_ratio_settings;
    VariableItemList* experimental_time_settings_view;
    VariableItemList* subghz_settings;
    VariableItemList* debug_settings;
    NumberInput* subghz_freq_input;
    Dcf77ExperimentalTimeInput* preset_time_input;
    Widget* startup_widget;
    View* about_view;
    Widget* egg_widget;
    View* gpio_rf_warning_view;
    View* tx_view;
    VariableItem* lf_signal_item;
    VariableItem* lf_tx_enabled_item;
    VariableItem* lf_freq_item;
    VariableItem* lf_default_freq_item;
    VariableItem* lf_tx_ratio_x_item;
    VariableItem* lf_tx_ratio_y_item;
    VariableItem* experimental_time_enabled_item;
    VariableItem* experimental_time_source_item;
    VariableItem* experimental_preset_item;
    VariableItem* experimental_direction_item;
    VariableItem* experimental_speed_item;
    VariableItem* subghz_tx_item;
    VariableItem* subghz_tone_item;
    VariableItem* subghz_band_item;
    VariableItem* subghz_frequency_item;
    VariableItem* subghz_manual_item;
    VariableItem* subghz_timeout_item;
    VariableItem* debug_gpio_baseband_item;
    VariableItem* debug_gpio_rf_item;
    VariableItem* debug_gpio_duty_item;
    VariableItem* debug_led_item;
    VariableItem* debug_screen_item;
    VariableItem* debug_speaker_item;
    NotificationApp* notification;
    bool lf_transmit_enabled;
    bool speaker_active;
    char lf_freq_text[16];
    char tx_ratio_x_text[4];
    char tx_ratio_y_text[4];
    char experimental_preset_text[24];
    char experimental_speed_text[8];
    char subghz_tone_text[16];
    char subghz_band_text[16];
    char subghz_frequency_text[16];
    char subghz_frequency_step_text[24];
    char subghz_manual_text[16];
    char subghz_timeout_text[8];
    char gpio_baseband_text[8];
    char gpio_rf_text[8];
    char gpio_rf_duty_text[8];
    char led_text[12];
    char screen_text[8];
    char speaker_text[16];

    uint8_t tx_minute;
    uint8_t tx_hour;
    uint8_t tx_day;
    uint8_t tx_month;
    uint8_t tx_year;
    uint8_t tx_dow;
    uint16_t tx_day_of_year;
    uint8_t tx_start_second_offset;
} AppFSM;

extern const NotificationSequence seq_c_minor;
extern const char* const dcf77_bitnames[];

void dcf77_app_sound_set(AppFSM* app_fsm, bool enabled);
bool dcf77_app_settings_save(const AppFSM* app_fsm);
void dcf77_app_apply_rf_settings(AppFSM* app_fsm);
uint8_t dcf77_app_get_lf_freq_index(uint32_t freq);
void dcf77_app_update_lf_freq_text(AppFSM* app_fsm);
void dcf77_app_update_tx_ratio_texts(AppFSM* app_fsm);
void dcf77_app_set_signal(AppFSM* app_fsm, RadioClockSignal signal);
void dcf77_app_set_signal_frequency(AppFSM* app_fsm, uint32_t freq);
void dcf77_app_restore_default_frequency(AppFSM* app_fsm);
bool dcf77_app_signal_can_run(const AppFSM* app_fsm);
void dcf77_app_set_tx_ratio_x(AppFSM* app_fsm, uint8_t x);
void dcf77_app_set_tx_ratio_y_index(AppFSM* app_fsm, uint8_t y_index);
uint8_t dcf77_app_get_tx_ratio_y(const AppFSM* app_fsm);
bool dcf77_app_should_send_frame(const AppFSM* app_fsm, uint32_t frame_index);
void dcf77_app_update_subghz_texts(AppFSM* app_fsm);
void dcf77_app_set_subghz_signal_mode(AppFSM* app_fsm, SubGhzSignalMode mode);
void dcf77_app_set_subghz_fsk_tone(AppFSM* app_fsm, uint8_t tone_index);
void dcf77_app_set_subghz_tx_timeout_index(AppFSM* app_fsm, uint8_t timeout_index);
void dcf77_app_set_subghz_band(AppFSM* app_fsm, uint8_t band_index);
void dcf77_app_set_subghz_frequency(AppFSM* app_fsm, uint32_t freq_hz);
void dcf77_app_step_subghz_frequency(AppFSM* app_fsm, int8_t direction);
uint32_t dcf77_app_get_subghz_frequency(const AppFSM* app_fsm);
uint32_t dcf77_app_get_subghz_tx_timeout_seconds(const AppFSM* app_fsm);
bool dcf77_app_subghz_runtime_enabled(const AppFSM* app_fsm);
void dcf77_app_update_experimental_time_texts(AppFSM* app_fsm);
void dcf77_app_set_experimental_time_enabled(AppFSM* app_fsm, bool enabled);
void dcf77_app_set_experimental_time_source(AppFSM* app_fsm, Dcf77ExperimentalTimeSource source);
void dcf77_app_set_experimental_preset_datetime(AppFSM* app_fsm, const DateTime* datetime);
void dcf77_app_set_experimental_time_direction(
    AppFSM* app_fsm,
    Dcf77ExperimentalTimeDirection direction);
void dcf77_app_set_experimental_time_speed_index(AppFSM* app_fsm, uint8_t speed_index);
void dcf77_app_set_experimental_speedup(AppFSM* app_fsm, uint8_t speedup);
void dcf77_app_set_experimental_slowdown(AppFSM* app_fsm, uint8_t slowdown);
void dcf77_app_seed_experimental_preset_from_rtc(AppFSM* app_fsm);
void dcf77_app_set_speaker_value_index(AppFSM* app_fsm, uint8_t value_index);
void dcf77_app_update_debug_texts(AppFSM* app_fsm);
void dcf77_app_set_gpio_baseband_pin(AppFSM* app_fsm, uint8_t pin_number);
void dcf77_app_set_gpio_rf_pin(AppFSM* app_fsm, uint8_t pin_number);
void dcf77_app_set_gpio_rf_duty_cycle(AppFSM* app_fsm, uint8_t duty_cycle);
void dcf77_app_set_led_color(AppFSM* app_fsm, Dcf77LedColor color);
void dcf77_app_set_screen_mode(AppFSM* app_fsm, Dcf77ScreenMode mode);
bool dcf77_app_gpio_rf_enabled(const AppFSM* app_fsm);

#endif
