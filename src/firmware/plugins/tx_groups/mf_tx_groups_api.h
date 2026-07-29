#pragma once

#include "../../morse_flipper_mapped_fal.h"
#include "../../morse_flipper_tx_groups.h"

#define MF_TX_GROUPS_API_MAGIC   0x4D465458UL
#define MF_TX_GROUPS_API_VERSION 1U

typedef enum {
    MfTxGroupsDrawPractice = 0,
    MfTxGroupsDrawResult,
    MfTxGroupsDrawFinal,
    MfTxGroupsDrawStartButtons,
    MfTxGroupsDrawStartKey,
} MfTxGroupsDrawMode;

typedef struct {
    void* context;
    const MorseFlipperTxGroup* group;
    const uint32_t* sum_speed;
    const uint32_t* sum_lgap;
    const uint32_t* sum_ratio;
    const uint32_t* sum_accuracy;
    const uint32_t* sum_dgap;
    const uint32_t* sum_variance;
    const uint16_t* session_total;
    const uint16_t* session_good;
    const uint16_t* session_sk;
    const uint32_t* result_until;
    const uint8_t* screen;
    const uint8_t* input_source;
    const bool* started;
    const bool* txg_sk;
    uint8_t screen_practice;
    uint8_t screen_result;
    uint8_t input_buttons;
    uint8_t prompt_width;
    void (*draw_prompt)(Canvas* canvas, void* context, int32_t cx, int32_t cy, char ch);
    void (*draw_history_divider)(Canvas* canvas, bool left_hint);
    void (*draw_left_exit_hint)(Canvas* canvas);
    char (*answer_preview)(void* context);
} MfTxGroupsDrawServices;

typedef struct {
    char target[MORSE_FLIPPER_TX_GROUP_LEN + 1U];
    char answer[MORSE_FLIPPER_TX_GROUP_LEN + 1U];
    char fault[24];
    MorseFlipperTxGroupResult result;
    uint32_t sum_speed;
    uint32_t sum_lgap;
    uint32_t sum_ratio;
    uint32_t sum_accuracy;
    uint32_t sum_dgap;
    uint32_t sum_variance;
    uint16_t session_total;
    uint16_t session_good;
    uint16_t session_sk;
    uint8_t mode;
    uint8_t countdown_s;
    bool sk;
    bool left_hint;
    bool show_left_exit_hint;
} MfTxGroupsDrawSnapshot;

typedef struct {
    MfTxGroupsDrawServices draw_services;
} MfTxGroupsEnterArgs;

typedef struct {
    MfTxGroupsDrawServices draw_services;
} MfTxGroupsState;

typedef struct {
    MorseFlipperMappedFalApi mapped;
    void (*init)(MorseFlipperTxGroup* group);
    void (*set_seed)(MorseFlipperTxGroup* group, uint32_t seed);
    void (*start)(MorseFlipperTxGroup* group, bool sk);
    void (*feed_mark)(MorseFlipperTxGroup* group, uint16_t ms);
    void (*feed_space)(MorseFlipperTxGroup* group, uint16_t ms);
    void (*feed_text)(MorseFlipperTxGroup* group, const char* text);
    bool (*finalize_answer_from_raw)(MorseFlipperTxGroup* group, uint16_t dit_ms);
    void (*set_range)(MorseFlipperTxGroup* group, uint8_t pass_min, uint8_t pass_max);
    void (*score)(MorseFlipperTxGroup* group, uint16_t dit_ms, bool timed_out);
    bool (*complete)(const MorseFlipperTxGroup* group);
    bool (*marks_complete)(const MorseFlipperTxGroup* group);
} MfTxGroupsApi;
