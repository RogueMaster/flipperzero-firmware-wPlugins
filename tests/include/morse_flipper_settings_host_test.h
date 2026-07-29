#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "gui/modules/variable_item_list.h"
#include "plugins/settings/mf_settings_api.h"

#define APP_ASSETS_PATH(path) path
#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))
#define MORSE_FLIPPER_PREVIEW_TICKS 8U
#define MORSE_FLIPPER_TRAINER_TIMEOUT_MIN_S 3U
#define MORSE_FLIPPER_TRAINER_TIMEOUT_MAX_S 10U
#define MORSE_FLIPPER_TRAINER_GROUP_PAUSE_MIN_S 3U
#define MORSE_FLIPPER_TRAINER_GROUP_PAUSE_MAX_S 15U
#define MORSE_FLIPPER_STRAIGHT_TIMEOUT_MIN_S 3U
#define MORSE_FLIPPER_STRAIGHT_TIMEOUT_MAX_S 10U
#define MORSE_FLIPPER_STRAIGHT_NEXT_MIN_S 1U
#define MORSE_FLIPPER_STRAIGHT_NEXT_MAX_S 10U
#define MORSE_TRAINER_CUSTOM_SET_CAP 8U

typedef struct { int unused; } FuriMutex;
typedef enum {
    MorseFlipperGpioRuleOk = 0,
    MorseFlipperGpioRuleBadIndex = 1,
} MorseFlipperGpioRule;
typedef struct { int unused; } ViewDispatcher;
typedef struct { uint32_t state[64]; } SceneManager;
typedef struct { uint8_t type; } SceneManagerEvent;
#define SceneManagerEventTypeBack 1U
#define UNUSED(value) (void)(value)
typedef struct { uint8_t lesson; uint8_t group_size; uint8_t session_groups; uint8_t custom_set_idx; uint16_t local_dit_ms; } MorseTrainer;
typedef struct { uint16_t local_dit_ms; uint8_t lesson; uint8_t group_size; uint8_t session_groups; uint8_t custom_set_idx; uint8_t farnsworth_wpm; uint8_t answer_timeout_s; uint8_t group_pause_s; } MorseFlipperListeningSettings;
typedef struct { FuriMutex* mutex; const void* api; void* state; uint8_t owner; } MorseFlipperPluginSlot;
typedef struct MorseFlipperApp {
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    VariableItemList* settings_list;
    MorseFlipperPluginSlot plugin_slot;
    uint8_t input_source, keyer_mode, handedness, audio_path, tone_idx, p2_volume_pct, preview_ticks;
    uint8_t straight_answer_timeout_s, straight_next_delay_s, txg_difficulty, straight_wpm;
    uint8_t gpio_dit_idx, gpio_dah_idx, gpio_ground_idx, gpio_ptt_idx;
    uint8_t pc_mode_pref, pc_paddle_preset, pc_straight_preset;
    uint8_t scene;
    bool mouse_invert;
    MorseTrainer trainer;
    MorseFlipperListeningSettings listening_settings;
} MorseFlipperApp;

enum { MorseFlipperPluginOwnerSettings = 6, MorseFlipperHandednessNormal = 0, MorseFlipperHandednessSwapped = 1,
       MorseFlipperInputSourceStraight = 0, MorseFlipperInputSourcePaddle = 1, MorseFlipperInputSourceButtons = 2,
       MorseKeyerModeStraight = 1, MorseKeyerModeBug = 2, MorseKeyerModePlainIambic = 6, MorseKeyerModeIambicA = 7,
       MorseKeyerModeIambicB = 8, MorseKeyerModeUltimatic = 5, MorseKeyerModeKeyahead = 9,
       MorseFlipperAudioPathBuzzer = 0, MorseFlipperAudioPathP2 = 1, MorseFlipperAudioPathVibration = 2, MorseFlipperAudioPathSoftBuzz = 3,
       MorseFlipperPcModeOff = 0, MorseFlipperPcModeKeyboard = 1, MorseFlipperPcModeMouse = 2, MorseFlipperPcModeMidi = 3,
       MorseFlipperTxgDifficultyCount = 3,
       MorseFlipperSceneHome = 13, MorseFlipperSceneAudioCfg = 14, MorseFlipperSceneTrainer = 15,
       MorseFlipperSceneStraightCfg = 16, MorseFlipperScenePc = 17, MorseFlipperSceneGpio = 19,
       MorseFlipperSceneTxGroupsCfg = 34, MorseFlipperSceneRxCallsignsCfg = 35 };

uint32_t furi_get_tick(void);
void furi_mutex_acquire(FuriMutex*, uint32_t);
void furi_mutex_release(FuriMutex*);
#define FuriWaitForever 0U
uint8_t morse_flipper_current_wpm(const MorseFlipperApp*);
uint8_t morse_flipper_local_wpm(const MorseFlipperApp*);
size_t morse_trainer_lesson_count(void);
uint8_t morse_flipper_p2_volume_pct(const MorseFlipperApp*);
uint8_t morse_flipper_straight_wpm(const MorseFlipperApp*);
uint8_t morse_trainer_group_size(const MorseTrainer*);
uint8_t morse_trainer_session_groups(const MorseTrainer*);
void morse_flipper_set_local_wpm(MorseFlipperApp*, uint8_t);
void morse_flipper_set_straight_wpm(MorseFlipperApp*, uint8_t);
bool morse_flipper_gpio_try_apply(MorseFlipperApp*, uint8_t, uint8_t, uint8_t, uint8_t, MorseFlipperGpioRule*);
void morse_flipper_gpio_alert(MorseFlipperApp*, MorseFlipperGpioRule);
void morse_flipper_clear_button_keying(MorseFlipperApp*, uint32_t);
void morse_flipper_refresh_keyer(MorseFlipperApp*, uint32_t);
void morse_flipper_poll(MorseFlipperApp*);
void morse_flipper_resync_button_paddles(MorseFlipperApp*, uint32_t);
void morse_flipper_sync_audio_output(MorseFlipperApp*);
void morse_flipper_update_sidetone(MorseFlipperApp*);
bool morse_flipper_audio_output_is_pwm(const MorseFlipperApp*);
void morse_flipper_save_config(const MorseFlipperApp*);
uint8_t morse_pc_paddle_preset_count(void);
uint8_t morse_pc_straight_preset_count(void);
extern const int morse_flipper_tones[31];
void view_dispatcher_send_custom_event(ViewDispatcher*, uint32_t);
bool morse_flipper_plugin_runtime_open_mapped_locked(MorseFlipperApp*, uint8_t, uint8_t, const char*, uint32_t, uint32_t, uint32_t, const void*, MorseFlipperMappedFalResult*);
void morse_flipper_plugin_runtime_detach_locked(MorseFlipperApp*, uint8_t);
void scene_manager_set_scene_state(SceneManager*, uint32_t, uint32_t);
uint32_t scene_manager_get_scene_state(SceneManager*, uint32_t);
void morse_flipper_ensure_view(MorseFlipperApp*, uint8_t);
void morse_flipper_scene_enter_now(MorseFlipperApp*, uint32_t);
void morse_flipper_scene_back(MorseFlipperApp*);
#define MorseFlipperViewSettings 2U
