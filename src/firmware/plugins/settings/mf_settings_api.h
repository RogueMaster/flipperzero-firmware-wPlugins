#pragma once

#include "../../morse_flipper_mapped_fal.h"

typedef struct VariableItemList VariableItemList;

#define MF_SETTINGS_API_MAGIC 0x4D465345UL
#define MF_SETTINGS_API_VERSION 1U

typedef enum {
    MfSettingsEntryKeying = 0,
    MfSettingsEntryAudio,
    MfSettingsEntryListening,
    MfSettingsEntryStraight,
    MfSettingsEntryTxGroups,
    MfSettingsEntryGpio,
    MfSettingsEntryUsb,
} MfSettingsEntry;

typedef enum {
    MfSettingsNavigateAudio = 1,
    MfSettingsNavigateGpio = 2,
} MfSettingsNavigateEvent;

typedef struct {
    uint16_t local_wpm;
    uint8_t input_source;
    uint8_t keyer_mode;
    bool handedness;
    uint8_t audio_path;
    uint8_t tone_index;
    uint8_t p2_volume;
    uint8_t audio_waveform;
    uint8_t lesson;
    uint8_t farnsworth_wpm;
    uint8_t answer_timeout_s;
    uint8_t group_pause_s;
    uint8_t group_size;
    uint8_t group_count;
    uint8_t custom_set_idx;
    uint8_t straight_wpm;
    uint8_t straight_answer_timeout_s;
    uint8_t straight_next_delay_s;
    uint8_t tx_groups_difficulty;
    uint8_t gpio_dit_pin;
    uint8_t gpio_dah_pin;
    uint8_t gpio_ground_pin;
    uint8_t gpio_ptt_pin;
    uint8_t usb_mode;
    uint8_t usb_paddle_preset;
    uint8_t usb_straight_preset;
    bool usb_mouse_invert;
} MfSettingsSnapshot;

typedef enum {
    MfSettingsSetLocalWpm = 0,
    MfSettingsSetInputSource,
    MfSettingsSetKeyerMode,
    MfSettingsSetHandedness,
    MfSettingsSetAudioPath,
    MfSettingsSetTone,
    MfSettingsSetP2Volume,
    MfSettingsSetAudioWaveform,
    MfSettingsSetListeningLesson,
    MfSettingsSetListeningFarnsworth,
    MfSettingsSetListeningAnswerTimeout,
    MfSettingsSetListeningGroupPause,
    MfSettingsSetListeningGroupSize,
    MfSettingsSetListeningGroupCount,
    MfSettingsSetListeningCustomSet,
    MfSettingsSetStraightWpm,
    MfSettingsSetStraightAnswerTimeout,
    MfSettingsSetStraightNextDelay,
    MfSettingsSetTxGroupsDifficulty,
    MfSettingsApplyGpioDraft,
    MfSettingsSetUsbMode,
    MfSettingsSetUsbPaddlePreset,
    MfSettingsSetUsbStraightPreset,
    MfSettingsSetUsbMouseInvert,
} MfSettingsRequestKind;

typedef struct {
    uint8_t kind;
    uint32_t value;
    uint8_t gpio_dit_pin;
    uint8_t gpio_dah_pin;
    uint8_t gpio_ground_pin;
    uint8_t gpio_ptt_pin;
} MfSettingsRequest;

typedef struct {
    bool accepted;
    uint8_t error;
    MfSettingsSnapshot snapshot;
} MfSettingsResponse;

typedef bool (*MfSettingsApplyFn)(void* context, const MfSettingsRequest*, MfSettingsResponse*);
typedef void (*MfSettingsPostNavigateFn)(void* context, uint32_t event);

typedef struct {
    uint32_t struct_size;
    MfSettingsApplyFn apply;
    MfSettingsPostNavigateFn post_navigate;
} MfSettingsHostServices;

typedef struct {
    uint32_t struct_size;
    uint8_t entry;
    uint32_t selected_state;
    VariableItemList* list;
    MfSettingsSnapshot snapshot;
    const MfSettingsHostServices* services;
    void* service_context;
} MfSettingsEnterArgs;

typedef struct {
    MorseFlipperMappedFalApi mapped;
    bool (*request_close)(void* state, MorseFlipperMappedFalResult* result);
    uint32_t (*selected_state)(const void* state);
} MfSettingsApi;
