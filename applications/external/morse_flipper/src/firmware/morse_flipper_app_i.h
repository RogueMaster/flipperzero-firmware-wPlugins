/*
 * Purpose: Share private Morse Flipper app state and internal declarations.
 * Owns: screen/scene enums, grouped runtime state, constants, and module glue.
 * Depends on: Flipper GUI/HAL services plus all host-tested feature headers.
 * Tests: firmware build and feature-specific host tests.
 */

#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_rtc.h>

#include <dialogs/dialogs.h>
#include <gui/elements.h>
#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/variable_item_list.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <input/input.h>
#include <m-array.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <string.h>

#include "cw_markdown_widget.h"
#include "keyer.h"
#include "morse_flipper_audio_pwm.h"
#include "morse_flipper_cw_decoder.h"
#include "morse_flipper_gpio.h"
#include "morse_flipper_gpio_probe.h"
#include "morse_flipper_ham_keyer.h"
#include "morse_flipper_paths.h"
#include "morse_flipper_radio.h"
#include "morse_flipper_run_history.h"
#include "morse_flipper_rf.h"
#include "morse_flipper_straight_filter.h"
#include "morse_flipper_straight_trainer.h"
#include "morse_flipper_tx_groups.h"
#include "pc_keys.h"
#include "trainer.h"
#include "trainer_files.h"
#include "usb/morse_usb_midi.h"

#define MORSE_FLIPPER_VOLUME                        0.7f
#define MORSE_FLIPPER_POLL_MS                       5
#define MORSE_FLIPPER_PREVIEW_TICKS                 8
#define MORSE_FLIPPER_CONFIG_PATH                   APP_DATA_PATH("config.bin")
#define MORSE_FLIPPER_SETTINGS_VERSION              1U
#define MORSE_FLIPPER_DEFAULT_DIT_MS                100U
#define MORSE_FLIPPER_SESSION_ANSWER_GRACE_MS       250U
#define MORSE_FLIPPER_SESSION_RESULT_MS             160U
#define MORSE_FLIPPER_TXG_RESULT_DELAY_MS           500U
#define MORSE_FLIPPER_STRAIGHT_SETTLE_MS            700U
#define MORSE_FLIPPER_STRAIGHT_RELEASE_DEBOUNCE_MS  15U
#define MORSE_FLIPPER_AUDIO_WAIT_DRAW_MS            30U
#define MORSE_FLIPPER_RF_TX_TAIL_DITS               2U
#define MORSE_FLIPPER_RF_RSSI_WINDOW_MS             160U
#define MORSE_FLIPPER_RF_RSSI_PEAK_DECAY_MS         240U
#define MORSE_FLIPPER_RF_LIVE_DECODERS              1U
#define MORSE_FLIPPER_HAM_WPM_HOLD_NONE             0xFFU
#define MORSE_FLIPPER_HAM_WPM_HOLD_REPEAT_MS        500U
#define MORSE_FLIPPER_TRAINER_TIMEOUT_DEFAULT_S     6U
#define MORSE_FLIPPER_TRAINER_TIMEOUT_MIN_S         3U
#define MORSE_FLIPPER_TRAINER_TIMEOUT_MAX_S         10U
#define MORSE_FLIPPER_TRAINER_GROUP_PAUSE_MIN_S     3U
#define MORSE_FLIPPER_TRAINER_GROUP_PAUSE_MAX_S     15U
#define MORSE_FLIPPER_TRAINER_GROUP_PAUSE_DEFAULT_S 3U
#define MORSE_FLIPPER_STRAIGHT_TIMEOUT_DEFAULT_S    6U
#define MORSE_FLIPPER_STRAIGHT_TIMEOUT_MIN_S        3U
#define MORSE_FLIPPER_STRAIGHT_TIMEOUT_MAX_S        10U
#define MORSE_FLIPPER_STRAIGHT_NEXT_MIN_S           3U
#define MORSE_FLIPPER_STRAIGHT_NEXT_MAX_S           15U
#define MORSE_FLIPPER_STRAIGHT_NEXT_DEFAULT_S       3U
#define MORSE_FLIPPER_STRAIGHT_CHARSET              "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
#define MORSE_FLIPPER_RF_FREQ_DIGITS                6U
#define MORSE_FLIPPER_TONE_OFF_IDX                  0xFFU
#define MORSE_FLIPPER_DEFAULT_TONE_IDX              20U

#define MORSE_SOURCE_STRAIGHT_GPIO (1UL << 0)
#define MORSE_SOURCE_STRAIGHT_BTN  (1UL << 1)
#define MORSE_SOURCE_KEYER_DIT     (1UL << 2)
#define MORSE_SOURCE_KEYER_DAH     (1UL << 3)
#define MORSE_SOURCE_HAM_MACRO     (1UL << 4)

#define MORSE_PADDLE_SOURCE_GPIO_DIT (1UL << 0)
#define MORSE_PADDLE_SOURCE_GPIO_DAH (1UL << 1)
#define MORSE_PADDLE_SOURCE_BTN_OK   (1UL << 2)
#define MORSE_PADDLE_SOURCE_BTN_BACK (1UL << 3)

typedef struct {
    const char* name;
    float hz;
    uint8_t midi_note;
} MorseFlipperTone;

typedef enum {
    MorseFlipperHandednessNormal = 0,
    MorseFlipperHandednessSwapped = 1,
} MorseFlipperHandedness;

typedef enum {
    MorseFlipperInputSourceStraight = 0,
    MorseFlipperInputSourcePaddle = 1,
    MorseFlipperInputSourceButtons = 2,
} MorseFlipperInputSource;

typedef enum {
    MorseFlipperAudioPathBuzzer = 0,
    MorseFlipperAudioPathGpioP2Hd = 1,
    MorseFlipperAudioPathVibration = 2,
} MorseFlipperAudioPath;

typedef enum {
    MorseFlipperTxgDifficultyEasy = 0,
    MorseFlipperTxgDifficultyMedium,
    MorseFlipperTxgDifficultyCompetition,
    MorseFlipperTxgDifficultyCount,
} MorseFlipperTxgDifficulty;

typedef enum {
    MorseFlipperAboutModeLanding = 0,
    MorseFlipperAboutModeText,
} MorseFlipperAboutMode;

typedef enum {
    MorseFlipperScreenHome = 0,
    MorseFlipperScreenRun = 1,
    MorseFlipperScreenTrace = 2,
    MorseFlipperScreenTrainer = 3,
    MorseFlipperScreenSession = 4,
    MorseFlipperScreenRf = 5,
    MorseFlipperScreenRfRx = 6,
    MorseFlipperScreenRfFreq = 7,
    MorseFlipperScreenStraight = 8,
    MorseFlipperScreenSessionEnd = 9,
    MorseFlipperScreenMenu = 10,
    MorseFlipperScreenHelp = 11,
    MorseFlipperScreenAbout = 12,
    MorseFlipperScreenStartupProbe = 13,
    MorseFlipperScreenHamRun = 14,
    MorseFlipperScreenHamStartRefusal = 15,
    MorseFlipperScreenHamAssign = 16,
    MorseFlipperScreenHamAssignments = 17,
    MorseFlipperScreenHamCopyNotice = 18,
    MorseFlipperScreenHamDeleteConfirm = 19,
    MorseFlipperScreenTxGroups = 20,
    MorseFlipperScreenTxGroupsResult = 21,
    MorseFlipperScreenTxGroupsFinal = 22,
} MorseFlipperScreen;

typedef enum {
    MorseFlipperViewMenu = 0,
    MorseFlipperViewLive,
    MorseFlipperViewSettings,
    MorseFlipperViewTextInput,
} MorseFlipperView;

typedef enum {
    MorseFlipperSceneMenuMain = 0,
    MorseFlipperSceneMenuTraining,
    MorseFlipperSceneMenuSettings,
    MorseFlipperSceneMenuHelp,
    MorseFlipperSceneMenuRf,
    MorseFlipperSceneMenuHam,
    MorseFlipperSceneRun,
    MorseFlipperSceneRf,
    MorseFlipperSceneRfRx,
    MorseFlipperSceneRfFreq,
    MorseFlipperSceneSession,
    MorseFlipperSceneStraight,
    MorseFlipperSceneSessionEnd,
    MorseFlipperSceneHome,
    MorseFlipperSceneAudioCfg,
    MorseFlipperSceneTrainer,
    MorseFlipperSceneStraightCfg,
    MorseFlipperScenePc,
    MorseFlipperSceneTrace,
    MorseFlipperSceneGpio,
    MorseFlipperSceneHelp,
    MorseFlipperSceneAbout,
    MorseFlipperSceneStartupProbe,
    MorseFlipperSceneHamRun,
    MorseFlipperSceneHamStartRefusal,
    MorseFlipperSceneHamConfigure,
    MorseFlipperSceneHamMessageActions,
    MorseFlipperSceneHamTextInput,
    MorseFlipperSceneHamAssign,
    MorseFlipperSceneHamAssignments,
    MorseFlipperSceneHamCopyNotice,
    MorseFlipperSceneHamDeleteConfirm,
    MorseFlipperSceneTxGroups,
    MorseFlipperSceneTxGroupsResult,
    MorseFlipperSceneTxGroupsFinal,
    MorseFlipperSceneTxGroupsCfg,
    MorseFlipperSceneNum,
} MorseFlipperScene;

typedef enum {
    MorseFlipperHamMenuStart = 1,
    MorseFlipperHamMenuLogging,
    MorseFlipperHamMenuConfigure,
    MorseFlipperHamMenuAssignments,
} MorseFlipperHamMenuItem;

typedef enum {
    MorseFlipperHamConfigureAdd = 1,
    MorseFlipperHamConfigureMessageBase = 32,
} MorseFlipperHamConfigureItem;

typedef enum {
    MorseFlipperHamActionAssign = 1,
    MorseFlipperHamActionEdit,
    MorseFlipperHamActionCopy,
    MorseFlipperHamActionDelete,
} MorseFlipperHamActionItem;

typedef enum {
    MorseFlipperHamTextModeNone = 0,
    MorseFlipperHamTextModeAdd,
    MorseFlipperHamTextModeEdit,
} MorseFlipperHamTextMode;

typedef enum {
    MorseFlipperCustomHamTextDone = 0x1A00,
} MorseFlipperCustomEvent;

typedef enum {
    MorseFlipperCustomHelpPrev = 0x1900,
    MorseFlipperCustomHelpNext,
} MorseFlipperHelpCustomEvent;

typedef enum {
    MorseFlipperHelpFirstSteps = 0,
    MorseFlipperHelpInputKeys,
    MorseFlipperHelpConnectingPaddle,
    MorseFlipperHelpPractice,
    MorseFlipperHelpPrepping,
    MorseFlipperHelpContact,
    MorseFlipperHelpContesting,
    MorseFlipperHelpUsbLive,
    MorseFlipperHelpHamUsage,
    MorseFlipperHelpTroubleshooting,
    MorseFlipperHelpMovingForward,
    MorseFlipperHelpCount,
} MorseFlipperHelpTopic;

typedef enum {
    MorseFlipperSettingWpm = 0,
    MorseFlipperSettingInput,
    MorseFlipperSettingKeyer,
    MorseFlipperSettingSwap,
    MorseFlipperSettingAudio,
    MorseFlipperSettingGpio,
} MorseFlipperSettingIndex;

typedef enum {
    MorseFlipperAudioSettingPath = 0,
    MorseFlipperAudioSettingTone,
    MorseFlipperAudioSettingP2Volume,
} MorseFlipperAudioSettingIndex;

typedef enum {
    MorseFlipperGpioSettingDit = 0,
    MorseFlipperGpioSettingDah,
    MorseFlipperGpioSettingGround,
    MorseFlipperGpioSettingPtt,
} MorseFlipperGpioSettingIndex;

typedef enum {
    MorseFlipperTrainerSettingLesson = 0,
    MorseFlipperTrainerSettingWpm,
    MorseFlipperTrainerSettingFarnsworth,
    MorseFlipperTrainerSettingAnswerTimeout,
    MorseFlipperTrainerSettingGroupPause,
    MorseFlipperTrainerSettingGroupSize,
    MorseFlipperTrainerSettingGroups,
    MorseFlipperTrainerSettingChars,
} MorseFlipperTrainerSettingIndex;

typedef enum {
    MorseFlipperPcModeOff = 0,
    MorseFlipperPcModeKeyboard = 1,
    MorseFlipperPcModeMouse = 2,
    MorseFlipperPcModeMidi = 3,
} MorseFlipperPcMode;

typedef enum {
    MorseFlipperBacklightAuto = 0,
    MorseFlipperBacklightHold,
} MorseFlipperBacklightMode;

typedef enum {
    MorseFlipperUsbSettingConnection = 0,
    MorseFlipperUsbSettingPaddle,
    MorseFlipperUsbSettingStraight,
    MorseFlipperUsbSettingMouseSwap,
} MorseFlipperUsbSettingIndex;

typedef struct {
    uint8_t selected_message;
    uint8_t text_mode;
    bool key_level;
    bool macro_active;
    bool macro_mark;
    uint8_t macro_char_idx;
    uint8_t macro_mark_idx;
    uint8_t macro_dir;
    uint8_t wpm_hold_key;
    uint32_t macro_next_at;
    uint32_t notice_until;
    uint32_t wpm_hold_next_at;
    char text_buffer[MORSE_FLIPPER_HAM_KEYER_MESSAGE_LEN + 1U];
    char macro_text[MORSE_FLIPPER_HAM_KEYER_MESSAGE_LEN + 1U];
    char notice[16];
} MorseFlipperHamRuntimeState;

typedef struct MorseFlipperApp {
    /* Flipper objects owned by the app shell; allocation and teardown live in app.c. */
    FuriMessageQueue* q;
    ViewPort* view_port;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    Submenu* submenu;
    TextInput* text_input;
    VariableItemList* settings_list;
    VariableItem* audio_cfg_items[MorseFlipperAudioSettingP2Volume + 1U];
    VariableItem* trainer_items[MorseFlipperTrainerSettingChars + 1U];
    VariableItem* straight_cfg_items[3];
    View* live_view;
    Gui* gui;
    DialogsApp* dialogs;
    NotificationApp* notifications;
    FuriString* help_text;
    volatile bool exit_requested;

    /*
     * Hardware and transport mirrors. These track what we last asked the outside
     * world to do, so the poll loop can avoid noisy repeat writes.
     */
    FuriHalUsbInterface* previous_usb_config;
    FuriHalUsbHidConfig hid_cfg;
    bool tone_on;
    bool signal_led_on;
    bool signal_led_red;
    bool signal_led_green;
    bool speaker_owned;
    bool speaker_busy;
    float speaker_hz;
    bool transport_connected;
    bool mouse_invert;

    /* Button latch state for modes where Flipper buttons become a straight key or paddle. */
    bool left_down;
    bool ok_down;
    bool back_down;

    /* LCWO playback/session flags. Runtime transitions live in morse_flipper_session.c. */
    bool trainer_playback_active;
    bool trainer_playback_mark;
    bool session_started;
    bool session_round_pending;
    bool session_result_hold;
    bool session_result_tone;
    bool session_result_good;
    bool session_start_holdoff;

    /* Help/About UI state; the hidden trace entry uses the OK tap counter. */
    bool about_show_next;
    volatile bool midi_rx_pending;
    uint8_t screen;

    /* Persistent user choices, normalised on load before hardware is touched. */
    uint8_t pc_mode;
    uint8_t pc_mode_pref;
    uint8_t pc_paddle_preset;
    uint8_t pc_straight_preset;
    uint8_t scene;
    uint8_t handedness;
    uint8_t input_source;
    uint8_t audio_path;
    uint8_t keyer_mode;
    uint8_t gpio_straight_idx;
    uint8_t gpio_dit_idx;
    uint8_t gpio_dah_idx;
    uint8_t gpio_ground_idx;
    uint8_t gpio_ptt_idx;
    uint8_t trainer_row;
    uint8_t help_topic;
    uint8_t help_page;
    uint8_t help_card_count;
    uint8_t about_mode;
    uint8_t about_ok_count;
    uint8_t about_social_idx;
    uint8_t about_footer_seq_i;
    uint32_t about_last_ok_ms;
    uint32_t about_social_next_ms;
    CwmdState help_md;
    CwmdState about_md;
    MorseFlipperHamRuntimeState ham;

    /* Feature-local settings and cursors. Small integers are deliberate; this is a FAP. */
    uint8_t rf_freq_focus;
    uint8_t trainer_farnsworth_wpm;
    uint8_t trainer_answer_timeout_s;
    uint8_t trainer_group_pause_s;
    uint8_t straight_answer_timeout_s;
    uint8_t straight_next_delay_s;
    uint8_t trainer_char_idx;
    uint8_t trainer_mark_idx;
    uint8_t session_wait_draw_s;
    uint8_t gpio_edit_dit_idx;
    uint8_t gpio_edit_dah_idx;
    uint8_t gpio_edit_ground_idx;
    uint8_t gpio_edit_ptt_idx;
    uint8_t gpio_probe_state;
    uint8_t startup_gpio_probe_state;
    bool vail_mode_active;
    bool vail_speed_active;
    bool vail_tone_active;
    uint8_t vail_keyer_mode;
    uint16_t vail_dit_ms;
    uint8_t vail_tone_idx;
    MorseKeyer keyer;
    uint8_t tone_idx;
    uint8_t p2_volume_pct;
    uint8_t preview_ticks;
    uint8_t input_mask;

    /* Millisecond deadlines. Zero means idle unless the owning module says otherwise. */
    uint32_t trainer_next_at;
    uint32_t straight_next_at;
    uint32_t straight_wait_started_at;
    uint32_t straight_answer_started_at;
    uint32_t straight_last_input_at;
    uint32_t straight_mark_started_at;
    uint32_t txg_wait_started_at;
    uint32_t txg_last_input_at;
    uint32_t txg_result_open_at;
    uint32_t txg_result_until;
    uint8_t txg_result_draw_s;
    uint16_t run_dit_ms;
    uint16_t straight_dit_ms;
    uint16_t straight_session_total;
    uint16_t straight_session_good;
    uint16_t txg_session_total;
    uint16_t txg_session_good;
    uint16_t txg_session_sk;
    uint32_t txg_sum_speed;
    uint32_t txg_sum_lgap;
    uint32_t txg_sum_ratio;
    uint32_t txg_sum_accuracy;
    uint32_t txg_sum_dgap;
    uint32_t txg_sum_variance;
    uint32_t session_last_input_at;
    uint32_t session_answer_complete_at;
    uint32_t session_result_until;
    uint32_t session_next_group_at;
    uint32_t session_complete_at;
    uint32_t rf_tx_tail_until;
    uint32_t rf_tx_edge_at;
    uint32_t rf_rx_edge_at;
    uint32_t rf_rx_sample_next_at;
    uint32_t rf_rx_view_next_at;
    uint32_t rf_rssi_next_at;
    uint32_t rf_rssi_peak_decay_at;
    uint32_t gpio_edge_at;
    uint32_t gpio_probe_notice_until;
    uint32_t ptt_tail_until;
    uint32_t rf_edit_khz;
    int32_t rf_rssi_sum_dbm;
    uint32_t paddle_sources[MorseKeyerPaddleCount];
    uint32_t note_sources[3];

    /* Host-testable models: keep policy here, hardware glue around the edges. */
    MorseTrainer trainer;
    MorseFlipperHamKeyer ham_keyer;
    MorseTrainerCustomSets custom_sets;
    bool custom_sets_loaded;

    /* Live feature flags; each block is owned by its matching runtime module. */
    bool straight_playback_active;
    bool straight_playback_mark;
    bool straight_started;
    bool straight_wait_answer;
    bool straight_done;
    bool straight_key_down;
    bool straight_cutoff_wait_release;
    bool txg_started;
    bool txg_wait_answer;
    bool txg_done;
    bool txg_sk;
    bool txg_start_holdoff;
    bool rf_live_active;
    bool rf_tx_level;
    bool rf_rx_level;
    bool rf_rx_candidate_level;
    bool rf_tx_gap_flushed;
    bool rf_rx_gap_flushed;
    bool rf_rssi_valid;
    bool rf_carrier_present;
    bool rf_monitor_tone;
    bool rf_rx_audio_enabled;
    bool audio_wait_active;
    bool ptt_level;
    bool gpio_level;
    bool gpio_gap_flushed;
    uint8_t straight_mark_idx;
    uint8_t straight_next_draw_s;
    uint8_t txg_repeated_timeouts;
    uint8_t txg_difficulty;
    uint8_t straight_return_screen;
    uint8_t backlight_mode;
    uint8_t session_end_flash_phase;
    int8_t rf_rssi_dbm;
    int8_t rf_monitor_threshold_dbm;
    int8_t rf_rssi_peak_dbm;
    uint16_t rf_rssi_samples;
    uint16_t rf_rx_edges_window;
    uint16_t rf_rx_activity;
    uint8_t rf_rx_candidate_samples;
    uint8_t rf_rx_wpm_hint;
    char rf_rx_text[64];
    char rf_tx_text[64];
    char gpio_text[64];

    /* Larger submodules held by value to keep lifetime boring and failure modes fewer. */
    MorseFlipperRunHistory run_history;
    MorseFlipperAudioPwm audio_pwm;
    MorseFlipperStraightFilter straight_filter;
    MorseFlipperRf rf;
    MorseFlipperRfTicker rf_rx_ticker;
    MorseFlipperRadio radio;
    MorseFlipperCwDecoder rf_decoder;
    MorseFlipperCwDecoder tx_decoder;
    MorseFlipperCwDecoder gpio_decoder;
    MorseFlipperStraightTrainer straight_trainer;
    MorseFlipperTxGroup tx_group;
} MorseFlipperApp;

typedef struct {
    MorseFlipperApp* app;
    uint32_t bump;
} MorseFlipperLiveModel;

typedef struct {
    const char* label;
    uint8_t current_value_index;
    FuriString* current_value_text;
    uint8_t values_count;
    VariableItemChangeCallback change_callback;
    void* context;
} MorseFlipperVilItem;

ARRAY_DEF(MorseFlipperVilArray, MorseFlipperVilItem, M_POD_OPLIST);

typedef struct {
    MorseFlipperVilArray_t items;
    uint8_t position;
    uint8_t window_position;
} MorseFlipperVilModel;

typedef struct {
    bool live;
    bool btn;
    bool btn_str;
    bool btn_pad;
    bool back_key;
    bool back_exit;
    bool left_hint;
} MorseFlipperInputGate;

extern const char* const morse_flipper_usb_mode_names[4];
extern const GpioPin* const morse_flipper_gpio_pins[MORSE_FLIPPER_GPIO_PIN_COUNT];
extern const GpioPin* morse_flipper_straight_pin;
extern const GpioPin* morse_flipper_dit_pin;
extern const GpioPin* morse_flipper_dah_pin;
extern const GpioPin* morse_flipper_ground_pin;
extern const GpioPin* morse_flipper_ptt_pin;
extern const char* const morse_flipper_input_names[3];
extern const char* const morse_flipper_audio_path_names[3];
extern const uint8_t morse_flipper_input_values[3];
extern const uint8_t morse_flipper_keyer_values[7];
extern const MorseFlipperTone morse_flipper_tones[31];
extern const SceneManagerHandlers morse_flipper_scene_handlers;

void morse_flipper_set_paddle_source(
    MorseFlipperApp* app,
    uint8_t paddle,
    uint32_t source_mask,
    bool active,
    uint32_t now_ms);
void morse_flipper_set_note_source(
    MorseFlipperApp* app,
    uint8_t note,
    uint32_t source_mask,
    bool active);
void morse_flipper_resync_button_paddles(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_clear_button_keying(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_drain_keyer_events(MorseFlipperApp* app);
void morse_flipper_decoder_drain_into(MorseFlipperCwDecoder* decoder, char* out, size_t out_sz);
void morse_flipper_tone_stop(MorseFlipperApp* app);
void morse_flipper_refresh_keyer(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_poll(MorseFlipperApp* app);
void morse_flipper_release_all_notes(MorseFlipperApp* app);
void morse_flipper_load_config(MorseFlipperApp* app);
void morse_flipper_save_config(const MorseFlipperApp* app);
uint8_t morse_flipper_local_wpm(const MorseFlipperApp* app);
void morse_flipper_set_local_wpm(MorseFlipperApp* app, uint8_t wpm);
void morse_flipper_set_run_wpm(MorseFlipperApp* app, uint8_t wpm);
void morse_flipper_clear_run_wpm(MorseFlipperApp* app, uint32_t now_ms);
uint8_t morse_flipper_straight_wpm(const MorseFlipperApp* app);
void morse_flipper_set_straight_wpm(MorseFlipperApp* app, uint8_t wpm);
void morse_flipper_clamp_trainer_settings(MorseFlipperApp* app);
void morse_flipper_clamp_straight_settings(MorseFlipperApp* app);
void morse_flipper_set_pc_mode(MorseFlipperApp* app, uint8_t mode);
void morse_flipper_handle_midi_rx(MorseFlipperApp* app);
void morse_flipper_sync_signal_led(MorseFlipperApp* app, bool on);
void morse_flipper_update_sidetone(MorseFlipperApp* app);
void morse_flipper_midi_rx_ready(void* context);
void morse_flipper_handle_active_keying_event(MorseFlipperApp* app, const InputEvent* event);
const char* morse_flipper_pc_state_name(const MorseFlipperApp* app);
uint8_t morse_flipper_nearest_tone_idx_for_midi(uint8_t midi_note);
bool morse_flipper_transport_connected(const MorseFlipperApp* app);
void morse_flipper_release_mouse_buttons(void);
void morse_flipper_send_transport_note(MorseFlipperApp* app, uint8_t note, bool active);
void morse_flipper_resync_transport_notes(MorseFlipperApp* app);
void morse_flipper_gpio_init(MorseFlipperApp* app);
void morse_flipper_gpio_deinit(void);
void morse_flipper_toggle_source(MorseFlipperApp* app);
bool morse_flipper_training_playback_active(const MorseFlipperApp* app);
bool morse_flipper_straight_answer_down(const MorseFlipperApp* app);
void morse_flipper_cycle_mode(MorseFlipperApp* app);
void morse_flipper_tone_nudge(MorseFlipperApp* app, int dir);
bool morse_flipper_local_buzzer_enabled(const MorseFlipperApp* app);
bool morse_flipper_audio_output_is_pwm(const MorseFlipperApp* app);
bool morse_flipper_use_pwm_buzzer(const MorseFlipperApp* app);
bool morse_flipper_scene_supports_audio_pwm(uint8_t scene);
bool morse_flipper_audio_wait_transition(
    const MorseFlipperApp* app,
    uint8_t old_scene,
    uint8_t new_scene);
bool morse_flipper_any_active_notes(const MorseFlipperApp* app);
uint8_t morse_flipper_p2_volume_pct(const MorseFlipperApp* app);
void morse_flipper_sync_audio_output(MorseFlipperApp* app);
const char* morse_flipper_current_tone_name(const MorseFlipperApp* app);
float morse_flipper_active_tone_hz(const MorseFlipperApp* app);
uint8_t morse_flipper_current_keyer_mode(const MorseFlipperApp* app);
uint8_t morse_flipper_ok_button_paddle(const MorseFlipperApp* app);
uint8_t morse_flipper_back_button_paddle(const MorseFlipperApp* app);
bool morse_flipper_straight_like_mode(const MorseFlipperApp* app);
void morse_flipper_toggle_handedness(MorseFlipperApp* app);
void morse_flipper_tick_trainer_playback(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_help_open(MorseFlipperApp* app);
void morse_flipper_about_open(MorseFlipperApp* app);
void morse_flipper_cycle_trainer_value(MorseFlipperApp* app, int dir);
void morse_flipper_ensure_custom_sets_loaded(MorseFlipperApp* app);
void morse_flipper_apply_trainer_charset_choice(MorseFlipperApp* app);
void morse_flipper_drop_live_keying_for_playback(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_begin_group_playback(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_start_session(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_reset_session_runtime(MorseFlipperApp* app);
void morse_flipper_reset_session_state(MorseFlipperApp* app, uint32_t now_ms);
bool morse_flipper_session_repeat_active(const MorseFlipperApp* app);
bool morse_flipper_session_idle_view(const MorseFlipperApp* app);
void morse_flipper_tick_session(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_leave_session(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_leave_live_screen(MorseFlipperApp* app, uint32_t now_ms);
bool morse_flipper_session_wait_key_down(const MorseFlipperApp* app);
bool morse_flipper_session_hurry(MorseFlipperApp* app, uint32_t now_ms);
bool morse_flipper_session_end_flash(const MorseFlipperApp* app);
bool morse_flipper_session_running_view(const MorseFlipperApp* app);
bool morse_flipper_session_left_exit_active(const MorseFlipperApp* app);
MorseFlipperInputGate morse_flipper_input_gate(const MorseFlipperApp* app);
bool morse_flipper_straight_down(void);
bool morse_flipper_logical_dit_down(const MorseFlipperApp* app);
bool morse_flipper_logical_dah_down(const MorseFlipperApp* app);
void morse_flipper_gpio_probe_reset(MorseFlipperApp* app);
void morse_flipper_gpio_probe_prepare(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_gpio_probe_tick(MorseFlipperApp* app, uint32_t now_ms);
uint8_t morse_flipper_gpio_probe_sample_raw(MorseFlipperApp* app);
bool morse_flipper_gpio_probe_screen(const MorseFlipperApp* app);
bool morse_flipper_gpio_probe_keep_state(uint8_t screen);
bool morse_flipper_gpio_probe_notice_active(const MorseFlipperApp* app);
bool morse_flipper_gpio_probe_blocks_start(const MorseFlipperApp* app);
bool morse_flipper_gpio_probe_use_straight(const MorseFlipperApp* app);
void morse_flipper_feed_straight_mark(MorseFlipperApp* app, uint16_t mark_ms, uint32_t now_ms);
void morse_flipper_reset_straight_state(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_reset_straight_session(MorseFlipperApp* app);
void morse_flipper_note_straight_session(MorseFlipperApp* app);
bool morse_flipper_straight_countdown_active(const MorseFlipperApp* app);
void morse_flipper_start_straight_countdown(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_start_straight_round(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_finish_straight_round(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_tick_straight(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_reset_tx_groups_state(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_start_tx_groups_round(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_leave_tx_groups(MorseFlipperApp* app, uint32_t now_ms);
const char* morse_flipper_txg_difficulty_name(uint8_t difficulty);
uint8_t morse_flipper_txg_range_low(uint8_t difficulty);
uint8_t morse_flipper_txg_range_high(uint8_t difficulty);
void morse_flipper_feedback_pass(MorseFlipperApp* app);
void morse_flipper_feedback_fail(MorseFlipperApp* app);
void morse_flipper_feedback_timeout(MorseFlipperApp* app);
void morse_flipper_tick_ham_macro(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_ham_start_macro(MorseFlipperApp* app, const char* text, uint32_t now_ms);
void morse_flipper_ham_stop_macro(MorseFlipperApp* app);
void morse_flipper_ham_gpio_apply(MorseFlipperApp* app);
void morse_flipper_ham_gpio_release(MorseFlipperApp* app);
void morse_flipper_ham_log_append_text(MorseFlipperApp* app, const char* text, uint32_t now_ms);
void morse_flipper_ham_log_append_marker(MorseFlipperApp* app, const char* marker, uint32_t now_ms);
void morse_flipper_ham_log_flush(MorseFlipperApp* app);
void morse_flipper_ham_log_flush_if_idle(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_tick_live_rf(MorseFlipperApp* app, uint32_t now_ms);
int8_t morse_flipper_rf_clamp_dbm(int8_t dbm);
int8_t morse_flipper_rssi_dbm_round(float rssi);
uint32_t morse_flipper_rf_default_frequency_hz(void);
bool morse_flipper_rf_frequency_valid_hz(uint32_t hz);
bool morse_flipper_rf_frequency_valid_khz(uint32_t khz);
bool morse_flipper_rf_tx_allowed_khz(uint32_t khz);
void morse_flipper_rf_reset_rx_runtime(MorseFlipperApp* app);
void morse_flipper_rf_rx_bump_wpm(MorseFlipperApp* app, int dir);
void morse_flipper_rf_reset_edit(MorseFlipperApp* app);
void morse_flipper_rf_bump_focus(MorseFlipperApp* app, int dir);
void morse_flipper_rf_bump_digit(MorseFlipperApp* app, int dir);
void morse_flipper_rf_commit_edit(MorseFlipperApp* app);
void morse_flipper_rf_rx_edge(void* ctx, bool level, uint16_t duration_ms);
uint8_t morse_flipper_live_upper_char(uint8_t ch);
void morse_flipper_draw_left_exit_hint(Canvas* canvas);
void morse_flipper_draw_tx_history_divider(Canvas* canvas, bool left_hint);
void morse_flipper_draw_run_text(Canvas* canvas, int32_t x, int32_t y, const char* text);
void morse_flipper_draw_straight_prompt(Canvas* canvas, int32_t cx, int32_t cy, uint8_t ch);
void morse_flipper_about_reset(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_tick_about(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_draw_about(Canvas* canvas, MorseFlipperApp* app);
int16_t morse_flipper_about_max_scroll(Canvas* canvas);
void morse_flipper_draw_help(Canvas* canvas, MorseFlipperApp* app);
int16_t morse_flipper_help_max_scroll(Canvas* canvas, const MorseFlipperApp* app);
void morse_flipper_draw_gpio_probe_overlay(Canvas* canvas, const MorseFlipperApp* app);
void morse_flipper_draw_startup_gpio_probe(Canvas* canvas, const MorseFlipperApp* app);
void morse_flipper_draw_tx_history_screen(
    Canvas* canvas,
    MorseFlipperApp* app,
    const char* second_line);
void morse_flipper_draw_tx_history_screen_custom(
    Canvas* canvas,
    MorseFlipperApp* app,
    const char* second_line,
    const char* hint_override);
void morse_flipper_draw_trainer_setup(Canvas* canvas, MorseFlipperApp* app);
void morse_flipper_draw_straight_screen(Canvas* canvas, MorseFlipperApp* app);
void morse_flipper_draw_tx_groups_screen(Canvas* canvas, MorseFlipperApp* app);
void morse_flipper_draw_rf_tx_blocked(Canvas* canvas, const MorseFlipperApp* app);
void morse_flipper_draw_rf_freq_picker(Canvas* canvas, const MorseFlipperApp* app);
void morse_flipper_draw_rf_rx_screen(Canvas* canvas, MorseFlipperApp* app);
void morse_flipper_draw_ham_start_refusal(Canvas* canvas);
void morse_flipper_draw_ham_assign(Canvas* canvas);
void morse_flipper_draw_ham_assignments(Canvas* canvas, MorseFlipperApp* app);
void morse_flipper_draw_ham_copy_notice(Canvas* canvas, MorseFlipperApp* app);
void morse_flipper_draw_ham_delete_confirm(Canvas* canvas);
void morse_flipper_draw_ham_run(Canvas* canvas, MorseFlipperApp* app);
void morse_flipper_draw(Canvas* canvas, void* ctx);
void morse_flipper_view_dirty(MorseFlipperApp* app);
void morse_flipper_scene_open(MorseFlipperApp* app, uint32_t scene);
void morse_flipper_scene_back(MorseFlipperApp* app);
void morse_flipper_live_draw(Canvas* canvas, void* model);
bool morse_flipper_live_input(InputEvent* event, void* ctx);
bool morse_flipper_custom_event_callback(void* context, uint32_t event);
bool morse_flipper_back_event_callback(void* context);
uint8_t morse_flipper_backlight_mode(const MorseFlipperApp* app);
void morse_flipper_sync_backlight(MorseFlipperApp* app, uint32_t now_ms);
void morse_flipper_tick_callback(void* context);
const char* morse_flipper_input_line(const MorseFlipperApp* app, char* buf, size_t buf_sz);
uint32_t morse_flipper_straight_answer_settle_ms(const MorseFlipperApp* app);
void morse_flipper_draw_session_rows(Canvas* canvas, const MorseFlipperApp* app);
void morse_flipper_draw_session_bottom(Canvas* canvas, const MorseFlipperApp* app);
void morse_flipper_draw_session_end(Canvas* canvas, const MorseFlipperApp* app);
void morse_flipper_scene_enter_now(MorseFlipperApp* app, uint32_t scene);
void morse_flipper_enter_screen(
    MorseFlipperApp* app,
    uint8_t screen,
    uint8_t scene,
    uint32_t now_ms);
uint32_t morse_flipper_settings_list_state(VariableItemList* list);
void morse_flipper_settings_list_restore(VariableItemList* list, uint32_t state);
uint8_t morse_flipper_input_value_index(uint8_t source);
uint8_t morse_flipper_keyer_value_index(uint8_t mode);
uint16_t morse_flipper_wpm_to_dit_ms(uint8_t wpm);
uint16_t morse_flipper_current_dit_ms(const MorseFlipperApp* app);
uint16_t morse_flipper_current_straight_dit_ms(const MorseFlipperApp* app);
uint8_t morse_flipper_current_wpm(const MorseFlipperApp* app);
uint32_t morse_flipper_note_source_for_paddle(uint8_t paddle);
uint8_t morse_flipper_note_for_paddle(uint8_t paddle);
uint8_t morse_flipper_gpio_straight_idx(const MorseFlipperApp* app);
void morse_flipper_gpio_sync_straight_idx(MorseFlipperApp* app);
const GpioPin* morse_flipper_gpio_pin_ptr(uint8_t pin_idx);
void morse_flipper_gpio_bind_from_app(const MorseFlipperApp* app);
void morse_flipper_gpio_reset_candidates(void);
void morse_flipper_gpio_apply(MorseFlipperApp* app);
void morse_flipper_gpio_alert(MorseFlipperApp* app, MorseFlipperGpioRule rule);
bool morse_flipper_gpio_try_apply(
    MorseFlipperApp* app,
    uint8_t dit,
    uint8_t dah,
    uint8_t ground,
    uint8_t ptt,
    MorseFlipperGpioRule* out_rule);
void morse_flipper_sync_ptt(MorseFlipperApp* app, uint32_t now_ms);
const char* morse_flipper_status_line(const MorseFlipperApp* app, char* buf, size_t buf_sz);
bool morse_flipper_live_back_is_key(const MorseFlipperApp* app);
bool morse_flipper_live_left_hint(const MorseFlipperApp* app);
const char* morse_flipper_hand_name(const MorseFlipperApp* app);
const char* morse_flipper_run_hint(const MorseFlipperApp* app, char* buf, size_t buf_sz);
const char* morse_flipper_run_mode_line(const MorseFlipperApp* app, char* buf, size_t buf_sz);
const char* morse_flipper_run_input_name(const MorseFlipperApp* app);
const char* morse_flipper_run_keyer_name(const MorseFlipperApp* app);
const char* morse_flipper_run_usb_name(const MorseFlipperApp* app);
const char* morse_flipper_rf_khz_line(const MorseFlipperApp* app, char* buf, size_t buf_sz);
const char* morse_flipper_rf_rssi_line(const MorseFlipperApp* app, char* buf, size_t buf_sz);
const char* morse_flipper_rf_rx_wpm_line(const MorseFlipperApp* app, char* buf, size_t buf_sz);
const char* morse_flipper_source_short_name(const MorseFlipperApp* app, char* buf, size_t buf_sz);
void morse_flipper_reset_run_state(MorseFlipperApp* app);
const char* morse_flipper_trace_hint(const MorseFlipperApp* app, char* buf, size_t buf_sz);
void morse_flipper_settings_noop_enter(void* context, uint32_t index);
void morse_flipper_settings_enter_callback(void* context, uint32_t index);
void morse_flipper_settings_wpm_changed(VariableItem* item);
void morse_flipper_settings_input_changed(VariableItem* item);
void morse_flipper_settings_keyer_changed(VariableItem* item);
void morse_flipper_settings_swap_changed(VariableItem* item);
void morse_flipper_settings_tone_changed(VariableItem* item);
void morse_flipper_settings_gpio_dit_changed(VariableItem* item);
void morse_flipper_settings_gpio_dah_changed(VariableItem* item);
void morse_flipper_settings_gpio_ground_changed(VariableItem* item);
void morse_flipper_settings_gpio_ptt_changed(VariableItem* item);
void morse_flipper_settings_usb_mode_changed(VariableItem* item);
void morse_flipper_settings_usb_paddle_changed(VariableItem* item);
void morse_flipper_settings_usb_straight_changed(VariableItem* item);
void morse_flipper_settings_usb_mouse_swap_changed(VariableItem* item);
void morse_flipper_scene_menu_pick(void* ctx, uint32_t idx);
uint8_t morse_flipper_help_card_count(const MorseFlipperApp* app);
void morse_flipper_scene_home_on_enter(void* context);
bool morse_flipper_scene_home_on_event(void* context, SceneManagerEvent event);
void morse_flipper_scene_home_on_exit(void* context);
void morse_flipper_scene_audio_cfg_on_enter(void* context);
bool morse_flipper_scene_audio_cfg_on_event(void* context, SceneManagerEvent event);
void morse_flipper_scene_audio_cfg_on_exit(void* context);
void morse_flipper_scene_gpio_on_enter(void* context);
bool morse_flipper_scene_gpio_on_event(void* context, SceneManagerEvent event);
void morse_flipper_scene_gpio_on_exit(void* context);
void morse_flipper_scene_trainer_on_enter(void* context);
void morse_flipper_scene_trainer_on_exit(void* context);
void morse_flipper_scene_straight_cfg_on_enter(void* context);
void morse_flipper_scene_straight_cfg_on_exit(void* context);
void morse_flipper_scene_tx_groups_cfg_on_enter(void* context);
bool morse_flipper_scene_tx_groups_cfg_on_event(void* context, SceneManagerEvent event);
void morse_flipper_scene_tx_groups_cfg_on_exit(void* context);
void morse_flipper_scene_pc_on_enter(void* context);
bool morse_flipper_scene_pc_on_event(void* context, SceneManagerEvent event);
void morse_flipper_scene_pc_on_exit(void* context);
void morse_flipper_trainer_lesson_changed(VariableItem* item);
void morse_flipper_trainer_wpm_changed(VariableItem* item);
void morse_flipper_trainer_farnsworth_changed(VariableItem* item);
void morse_flipper_trainer_answer_timeout_changed(VariableItem* item);
void morse_flipper_trainer_group_pause_changed(VariableItem* item);
void morse_flipper_trainer_group_size_changed(VariableItem* item);
void morse_flipper_trainer_groups_changed(VariableItem* item);
void morse_flipper_trainer_chars_changed(VariableItem* item);
void morse_flipper_trainer_sync_farn_item(MorseFlipperApp* app);
void morse_flipper_trainer_menu_refresh(MorseFlipperApp* app);
bool morse_flipper_input_chunk_a(MorseFlipperApp* app, InputEvent* event);
bool morse_flipper_input_chunk_b(MorseFlipperApp* app, InputEvent* event, uint32_t now_ms);
void morse_flipper_active_mode_tick(MorseFlipperApp* app, uint32_t now_ms);
bool morse_flipper_active_mode_input(MorseFlipperApp* app, InputEvent* event, uint32_t now_ms);

MorseFlipperApp* morse_flipper_boot(void);
ViewDispatcher* morse_flipper_view_dispatcher_get(MorseFlipperApp* app);
void morse_flipper_ensure_view(MorseFlipperApp* app, uint8_t view);
void morse_flipper_shutdown(MorseFlipperApp* app);
