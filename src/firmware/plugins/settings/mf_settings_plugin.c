#include "mf_settings_api.h"

#include "mf_settings_model.h"
#include "../../trainer_lesson.h"
#include "../../morse_flipper_paths.h"
#include "../../pc_keys.h"

#include <flipper_application/flipper_application.h>
#include <furi.h>
#include <gui/modules/variable_item_list.h>
#include <storage/storage.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    MfSettingsEnterArgs args;
    MfSettingsSnapshot snapshot;
    char custom_parse_scratch[512];
    MfSettingsCustomNames custom_names;
    VariableItem* items[8];
    uint8_t gpio_dit_pin;
    uint8_t gpio_dah_pin;
    uint8_t gpio_ground_pin;
    uint8_t gpio_ptt_pin;
    bool entered;
} MfSettingsState;

_Static_assert(sizeof(MfSettingsState) <= 2048U, "settings FAL state regressed");

enum {
    MfSettingsRowWpm = 0,
    MfSettingsRowInput,
    MfSettingsRowKeyer,
    MfSettingsRowSwap,
    MfSettingsRowAudioOrTone,
    MfSettingsRowGpioOrVolume,
    MfSettingsRowWaveformOrTimeout,
    MfSettingsRowCount = 8U,
};

static const char* mf_settings_audio_path_name(uint8_t value) {
    static const char* const names[] = {"Buzzer", "P2", "Vibration"};
    return value < 3U ? names[value] : names[0];
}

typedef struct {
    uint8_t source;
    const char* name;
} MfSettingsInputChoice;

/* Display order is a UI contract; it is intentionally not the core enum order. */
static const MfSettingsInputChoice mf_settings_input_choices[] = {
    {.source = 2U, .name = "buttons"},
    {.source = 0U, .name = "straight"},
    {.source = 1U, .name = "paddle"},
};

static uint8_t mf_settings_input_index_from_source(uint8_t source) {
    for(uint8_t index = 0U; index < sizeof(mf_settings_input_choices) / sizeof(mf_settings_input_choices[0]); index++) {
        if(mf_settings_input_choices[index].source == source) return index;
    }
    return 0U;
}

static uint8_t mf_settings_input_source_from_index(uint8_t index) {
    return index < sizeof(mf_settings_input_choices) / sizeof(mf_settings_input_choices[0]) ?
               mf_settings_input_choices[index].source :
               mf_settings_input_choices[0].source;
}

static const char* mf_settings_input_name_from_index(uint8_t index) {
    return index < sizeof(mf_settings_input_choices) / sizeof(mf_settings_input_choices[0]) ?
               mf_settings_input_choices[index].name :
               mf_settings_input_choices[0].name;
}

typedef struct {
    uint8_t mode;
    const char* name;
} MfSettingsKeyerChoice;

/* These are persisted core enum values, not variable-item indices. */
static const MfSettingsKeyerChoice mf_settings_keyer_choices[] = {
    {.mode = 1U, .name = "Straight"},
    {.mode = 2U, .name = "Bug"},
    {.mode = 6U, .name = "Plain Iambic"},
    {.mode = 7U, .name = "Iambic A"},
    {.mode = 8U, .name = "Iambic B"},
    {.mode = 5U, .name = "Ultimatic"},
    {.mode = 9U, .name = "Keyahead"},
};

static uint8_t mf_settings_keyer_index_from_mode(uint8_t mode) {
    for(uint8_t index = 0U;
        index < sizeof(mf_settings_keyer_choices) / sizeof(mf_settings_keyer_choices[0]);
        index++) {
        if(mf_settings_keyer_choices[index].mode == mode) return index;
    }
    return 0U;
}

static uint8_t mf_settings_keyer_mode_from_index(uint8_t index) {
    return index < sizeof(mf_settings_keyer_choices) / sizeof(mf_settings_keyer_choices[0]) ?
               mf_settings_keyer_choices[index].mode :
               mf_settings_keyer_choices[0].mode;
}

static const char* mf_settings_keyer_name_from_index(uint8_t index) {
    return index < sizeof(mf_settings_keyer_choices) / sizeof(mf_settings_keyer_choices[0]) ?
               mf_settings_keyer_choices[index].name :
               mf_settings_keyer_choices[0].name;
}

static const char* mf_settings_tone_name(uint8_t value) {
    static const char* const names[] = {
        "G2", "A2", "B2", "C3", "D3", "E3", "F3", "G3", "A3", "B3", "C4",
        "D4", "E4", "F4", "G4", "A4", "B4", "C5", "D5", "E5", "F5", "G5",
        "A5", "B5", "C6", "D6", "E6", "F6", "G6", "A6", "B6"};
    return value < 31U ? names[value] : names[0];
}

static const char* mf_settings_rx_length_name(uint8_t value) {
    static const char* const names[] = {"4", "5", "6", "4-5", "5-6", "4-6"};
    return value < 6U ? names[value] : names[5];
}

typedef struct {
    uint8_t pin;
    const char* name;
} MfSettingsGpioChoice;

enum {
    MfSettingsGpioPinP3 = 1U,
    MfSettingsGpioPinP5 = 3U,
    MfSettingsGpioPinP7 = 5U,
    MfSettingsGpioPinP16 = 7U,
    MfSettingsGpioPinNone = 0xffU,
};

static const MfSettingsGpioChoice mf_settings_gpio_choices[] = {
    {.pin = 1U, .name = "P3"},
    {.pin = 2U, .name = "P4"},
    {.pin = 3U, .name = "P5"},
    {.pin = 4U, .name = "P6"},
    {.pin = 5U, .name = "P7"},
    {.pin = 7U, .name = "P16"},
};

static uint8_t mf_settings_gpio_pin_from_index(uint8_t index) {
    return index < sizeof(mf_settings_gpio_choices) / sizeof(mf_settings_gpio_choices[0]) ?
               mf_settings_gpio_choices[index].pin :
               MfSettingsGpioPinP3;
}

static uint8_t mf_settings_gpio_index_from_pin(uint8_t pin, bool permit_off) {
    if(permit_off && pin == MfSettingsGpioPinNone) return 0U;
    for(uint8_t i = 0U;
        i < sizeof(mf_settings_gpio_choices) / sizeof(mf_settings_gpio_choices[0]);
        i++) {
        if(pin == mf_settings_gpio_choices[i].pin) return permit_off ? i + 1U : i;
    }
    return 0U;
}

static const char* mf_settings_gpio_name(uint8_t pin) {
    if(pin == MfSettingsGpioPinNone) return "off";
    for(uint8_t i = 0U;
        i < sizeof(mf_settings_gpio_choices) / sizeof(mf_settings_gpio_choices[0]);
        i++) {
        if(pin == mf_settings_gpio_choices[i].pin) return mf_settings_gpio_choices[i].name;
    }
    return "?";
}

static bool mf_settings_gpio_selectable(uint8_t pin) {
    for(uint8_t i = 0U;
        i < sizeof(mf_settings_gpio_choices) / sizeof(mf_settings_gpio_choices[0]);
        i++) {
        if(pin == mf_settings_gpio_choices[i].pin) return true;
    }
    return false;
}

static void mf_settings_set_number(VariableItem* item, uint8_t index, uint8_t base, const char* suffix) {
    char value[12];

    variable_item_set_current_value_index(item, index);
    snprintf(value, sizeof(value), "%u%s", (unsigned)(base + index), suffix);
    variable_item_set_current_value_text(item, value);
}

static void mf_settings_set_percent(VariableItem* item, uint8_t percent) {
    char value[12];
    uint8_t index = (uint8_t)((percent - 10U) / 5U);

    variable_item_set_current_value_index(item, index);
    snprintf(value, sizeof(value), "%u%%", (unsigned)(10U + index * 5U));
    variable_item_set_current_value_text(item, value);
}

static void mf_settings_refresh(MfSettingsState* state) {
    VariableItem* item;
    char label[24];

    if(state == NULL) return;
    mf_settings_snapshot_normalize(&state->snapshot);
    item = state->items[MfSettingsRowWpm];
    if(item != NULL) mf_settings_set_number(item, (uint8_t)(state->snapshot.local_wpm - 10U), 10U, "");
    if(state->args.entry == MfSettingsEntryKeying) {
        item = state->items[MfSettingsRowInput];
        if(item != NULL) {
            uint8_t input_index = mf_settings_input_index_from_source(state->snapshot.input_source);
            variable_item_set_current_value_index(item, input_index);
            variable_item_set_current_value_text(item, mf_settings_input_name_from_index(input_index));
        }
        item = state->items[MfSettingsRowKeyer];
        if(item != NULL) {
            uint8_t keyer_index =
                mf_settings_keyer_index_from_mode(state->snapshot.keyer_mode);
            variable_item_set_current_value_index(item, keyer_index);
            variable_item_set_current_value_text(
                item, mf_settings_keyer_name_from_index(keyer_index));
        }
        item = state->items[MfSettingsRowSwap];
        if(item != NULL) {
            variable_item_set_current_value_index(item, state->snapshot.handedness);
            variable_item_set_current_value_text(item, state->snapshot.handedness ? "Yes" : "No");
        }
    } else if(state->args.entry == MfSettingsEntryAudio) {
        item = state->items[MfSettingsRowWpm];
        if(item != NULL) {
            variable_item_set_current_value_index(item, state->snapshot.audio_path);
            variable_item_set_current_value_text(item, mf_settings_audio_path_name(state->snapshot.audio_path));
        }
        item = state->items[MfSettingsRowInput];
        if(item != NULL) {
            if(state->snapshot.audio_path == 2U) {
                variable_item_set_values_count(item, 1U);
                variable_item_set_current_value_index(item, 0U);
                variable_item_set_current_value_text(item, "n/a");
            } else {
                variable_item_set_values_count(item, 31U);
                variable_item_set_current_value_index(item, state->snapshot.tone_index);
                variable_item_set_current_value_text(item, mf_settings_tone_name(state->snapshot.tone_index));
            }
        }
        item = state->items[MfSettingsRowKeyer];
        if(item != NULL) mf_settings_set_percent(item, state->snapshot.p2_volume);
        item = state->items[MfSettingsRowSwap];
        if(item != NULL) {
            variable_item_set_current_value_index(item, state->snapshot.audio_waveform);
            variable_item_set_current_value_text(item, state->snapshot.audio_waveform ? "Sine" : "Square");
        }
    } else if(state->args.entry == MfSettingsEntryListening) {
        item = state->items[MfSettingsRowWpm];
        if(item != NULL) {
            uint8_t lesson = state->snapshot.lesson - 1U;
            variable_item_set_current_value_index(item, lesson);
            morse_trainer_lesson_label((uint8_t)(lesson + 1U), label, sizeof(label));
            variable_item_set_current_value_text(item, label);
        }
        item = state->items[MfSettingsRowInput];
        if(item != NULL) mf_settings_set_number(item, (uint8_t)(state->snapshot.local_wpm - 10U), 10U, "");
        item = state->items[MfSettingsRowKeyer];
        if(item != NULL) mf_settings_set_number(item, (uint8_t)(state->snapshot.farnsworth_wpm - 1U), 1U, "");
        item = state->items[MfSettingsRowSwap];
        if(item != NULL) mf_settings_set_number(item, (uint8_t)(state->snapshot.answer_timeout_s - 3U), 3U, "");
        item = state->items[MfSettingsRowAudioOrTone];
        if(item != NULL) mf_settings_set_number(item, (uint8_t)(state->snapshot.group_pause_s - 3U), 3U, "");
        item = state->items[MfSettingsRowGpioOrVolume];
        if(item != NULL) mf_settings_set_number(item, (uint8_t)(state->snapshot.group_size - 1U), 1U, "");
        item = state->items[MfSettingsRowWaveformOrTimeout];
        if(item != NULL) mf_settings_set_number(item, (uint8_t)(state->snapshot.group_count - 3U), 3U, "");
        item = state->items[7];
        if(item != NULL) {
            uint8_t selected = state->snapshot.custom_set_idx;
            if(selected > state->custom_names.count) selected = 0U;
            variable_item_set_values_count(item, state->custom_names.count + 1U);
            variable_item_set_current_value_index(item, selected);
            variable_item_set_current_value_text(
                item, selected == 0U ? "lesson" : state->custom_names.names[selected - 1U]);
        }
    } else if(state->args.entry == MfSettingsEntryStraight) {
        item = state->items[0];
        if(item != NULL) mf_settings_set_number(item, (uint8_t)(state->snapshot.straight_wpm - 10U), 10U, "");
        item = state->items[1];
        if(item != NULL) mf_settings_set_number(item, (uint8_t)(state->snapshot.straight_answer_timeout_s - 1U), 1U, "");
        item = state->items[2];
        if(item != NULL) mf_settings_set_number(item, (uint8_t)(state->snapshot.straight_next_delay_s - 1U), 1U, "");
    } else if(state->args.entry == MfSettingsEntryTxGroups) {
        item = state->items[0];
        if(item != NULL) {
            variable_item_set_current_value_index(item, state->snapshot.tx_groups_difficulty);
            variable_item_set_current_value_text(
                item,
                state->snapshot.tx_groups_difficulty == 0U ? "Easy" :
                state->snapshot.tx_groups_difficulty == 1U ? "Medium" : "Competition");
        }
    } else if(state->args.entry == MfSettingsEntryRxCallsigns) {
        item = state->items[0];
        if(item != NULL) {
            variable_item_set_current_value_index(item, state->snapshot.rx_callsigns_length);
            variable_item_set_current_value_text(
                item, mf_settings_rx_length_name(state->snapshot.rx_callsigns_length));
        }
        item = state->items[1];
        if(item != NULL)
            mf_settings_set_number(
                item, (uint8_t)(state->snapshot.rx_callsigns_wpm - 10U), 10U, "");
        item = state->items[2];
        if(item != NULL) {
            variable_item_set_values_count(item, state->snapshot.rx_callsigns_wpm);
            mf_settings_set_number(
                item,
                (uint8_t)(state->snapshot.rx_callsigns_farnsworth_wpm - 1U),
                1U,
                "");
        }
    } else if(state->args.entry == MfSettingsEntryGpio) {
        uint8_t values[] = {state->gpio_dit_pin, state->gpio_dah_pin, state->gpio_ground_pin, state->gpio_ptt_pin};
        for(uint8_t i = 0U; i < 4U; i++) {
            item = state->items[i];
            if(item == NULL) continue;
            variable_item_set_current_value_index(
                item, mf_settings_gpio_index_from_pin(values[i], i >= 2U));
            variable_item_set_current_value_text(item, mf_settings_gpio_name(values[i]));
        }
    } else if(state->args.entry == MfSettingsEntryUsb) {
        item = state->items[0];
        if(item != NULL) {
            variable_item_set_current_value_index(item, state->snapshot.usb_mode);
            static const char* const modes[] = {"None", "Keyboard", "Mouse", "MIDI"};
            variable_item_set_current_value_text(
                item, state->snapshot.usb_mode < 4U ? modes[state->snapshot.usb_mode] : modes[0]);
        }
        item = state->items[1];
        if(item != NULL) {
            variable_item_set_current_value_index(item, state->snapshot.usb_paddle_preset);
            variable_item_set_current_value_text(
                item, morse_pc_paddle_preset_name(state->snapshot.usb_paddle_preset));
        }
        item = state->items[2];
        if(item != NULL) {
            variable_item_set_current_value_index(item, state->snapshot.usb_straight_preset);
            variable_item_set_current_value_text(
                item, morse_pc_straight_preset_name(state->snapshot.usb_straight_preset));
        }
        item = state->items[3];
        if(item != NULL) {
            variable_item_set_current_value_index(item, state->snapshot.usb_mouse_invert);
            variable_item_set_current_value_text(item, state->snapshot.usb_mouse_invert ? "Yes" : "No");
        }
    }
}

static bool mf_settings_apply(MfSettingsState* state, uint8_t kind, uint32_t value) {
    MfSettingsRequest request = {.kind = kind, .value = value};
    MfSettingsResponse response = {0};

    if(state == NULL || state->args.services == NULL || state->args.services->apply == NULL) return false;
    if(!state->args.services->apply(state->args.service_context, &request, &response) || !response.accepted)
        return false;
    state->snapshot = response.snapshot;
    mf_settings_refresh(state);
    return true;
}

static void mf_settings_changed(VariableItem* item) {
    MfSettingsState* state = variable_item_get_context(item);
    uint8_t row = 0U;
    uint8_t index;

    if(state == NULL) return;
    while(row < MfSettingsRowCount && state->items[row] != item) row++;
    if(row == MfSettingsRowCount) return;
    index = variable_item_get_current_value_index(item);
    if(state->args.entry == MfSettingsEntryKeying) {
        static const uint8_t kinds[] = {
            MfSettingsSetLocalWpm, MfSettingsSetInputSource, MfSettingsSetKeyerMode, MfSettingsSetHandedness};
        if(row < 4U)
            (void)mf_settings_apply(
                state,
                kinds[row],
                row == MfSettingsRowInput ? mf_settings_input_source_from_index(index) :
                row == MfSettingsRowKeyer ? mf_settings_keyer_mode_from_index(index) :
                                           (row == MfSettingsRowWpm ? 10U + index : index));
    } else if(state->args.entry == MfSettingsEntryAudio) {
        static const uint8_t kinds[] = {
            MfSettingsSetAudioPath, MfSettingsSetTone, MfSettingsSetP2Volume, MfSettingsSetAudioWaveform};
        if(row < 4U) (void)mf_settings_apply(
            state, kinds[row], row == 2U ? 10U + index * 5U : index);
    } else if(state->args.entry == MfSettingsEntryListening) {
        static const uint8_t kinds[] = {
            MfSettingsSetListeningLesson, MfSettingsSetLocalWpm, MfSettingsSetListeningFarnsworth,
            MfSettingsSetListeningAnswerTimeout, MfSettingsSetListeningGroupPause,
            MfSettingsSetListeningGroupSize, MfSettingsSetListeningGroupCount};
        static const uint8_t bases[] = {1U, 10U, 1U, 3U, 3U, 1U, 3U};
        if(row < 7U) (void)mf_settings_apply(state, kinds[row], bases[row] + index);
        if(row == 7U) (void)mf_settings_apply(state, MfSettingsSetListeningCustomSet, index);
    } else if(state->args.entry == MfSettingsEntryStraight) {
        static const uint8_t kinds[] = {
            MfSettingsSetStraightWpm, MfSettingsSetStraightAnswerTimeout, MfSettingsSetStraightNextDelay};
        static const uint8_t bases[] = {10U, 1U, 1U};
        if(row < 3U) (void)mf_settings_apply(state, kinds[row], bases[row] + index);
    } else if(state->args.entry == MfSettingsEntryTxGroups && row == 0U) {
        (void)mf_settings_apply(state, MfSettingsSetTxGroupsDifficulty, index);
    } else if(state->args.entry == MfSettingsEntryRxCallsigns) {
        static const uint8_t kinds[] = {
            MfSettingsSetRxCallsignsLength,
            MfSettingsSetRxCallsignsWpm,
            MfSettingsSetRxCallsignsFarnsworth,
        };
        static const uint8_t bases[] = {0U, 10U, 1U};
        if(row < 3U) (void)mf_settings_apply(state, kinds[row], bases[row] + index);
    } else if(state->args.entry == MfSettingsEntryUsb) {
        static const uint8_t kinds[] = {
            MfSettingsSetUsbMode, MfSettingsSetUsbPaddlePreset,
            MfSettingsSetUsbStraightPreset, MfSettingsSetUsbMouseInvert};
        if(row < 4U) (void)mf_settings_apply(state, kinds[row], index);
    } else if(state->args.entry == MfSettingsEntryGpio) {
        uint8_t pin = row >= 2U && index == 0U ? MfSettingsGpioPinNone :
                      row == 3U ? MfSettingsGpioPinP16 :
                                 mf_settings_gpio_pin_from_index(
                                     index - (row >= 2U ? 1U : 0U));
        if(row == 0U) state->gpio_dit_pin = pin;
        if(row == 1U) state->gpio_dah_pin = pin;
        if(row == 2U) state->gpio_ground_pin = pin;
        if(row == 3U) state->gpio_ptt_pin = pin;
        mf_settings_refresh(state);
    }
}

static void mf_settings_enter_row(void* context, uint32_t index) {
    UNUSED(context);
    UNUSED(index);
}

static void mf_settings_build_rows(MfSettingsState* state) {
    VariableItemList* list = state->args.list;
    uint8_t rows = mf_settings_row_count(state->args.entry, &state->snapshot);

    if(state->args.entry == MfSettingsEntryKeying) {
        state->items[0] = variable_item_list_add(list, "WPM", 21U, mf_settings_changed, state);
        state->items[1] = variable_item_list_add(list, "Input", 3U, mf_settings_changed, state);
        state->items[2] = variable_item_list_add(list, "Keyer", 7U, mf_settings_changed, state);
        state->items[3] = variable_item_list_add(list, "Swap paddles", 2U, mf_settings_changed, state);
    } else if(state->args.entry == MfSettingsEntryAudio) {
        state->items[0] = variable_item_list_add(list, "Audio path", 3U, mf_settings_changed, state);
        state->items[1] = variable_item_list_add(list, "Frequency", 31U, mf_settings_changed, state);
        state->items[2] = variable_item_list_add(list, "PWM Volume", 19U, mf_settings_changed, state);
        if(rows == 4U) state->items[3] = variable_item_list_add(list, "Waveform", 2U, mf_settings_changed, state);
    } else if(state->args.entry == MfSettingsEntryListening) {
        state->items[0] = variable_item_list_add(list, "Lesson", (uint8_t)morse_trainer_lesson_count(), mf_settings_changed, state);
        state->items[1] = variable_item_list_add(list, "WPM", 21U, mf_settings_changed, state);
        state->items[2] = variable_item_list_add(list, "Farnsworth", state->snapshot.local_wpm, mf_settings_changed, state);
        state->items[3] = variable_item_list_add(list, "Answer timeout", 8U, mf_settings_changed, state);
        state->items[4] = variable_item_list_add(list, "Group pause", 13U, mf_settings_changed, state);
        state->items[5] = variable_item_list_add(list, "Group size", 9U, mf_settings_changed, state);
        state->items[6] = variable_item_list_add(list, "Groups", 28U, mf_settings_changed, state);
        state->items[7] = variable_item_list_add(list, "Chars", 1U, mf_settings_changed, state);
    } else if(state->args.entry == MfSettingsEntryStraight) {
        state->items[0] = variable_item_list_add(list, "WPM", 21U, mf_settings_changed, state);
        state->items[1] = variable_item_list_add(list, "Answer timeout", 30U, mf_settings_changed, state);
        state->items[2] = variable_item_list_add(list, "Next delay", 30U, mf_settings_changed, state);
    } else if(state->args.entry == MfSettingsEntryTxGroups) {
        state->items[0] = variable_item_list_add(list, "Difficulty", 3U, mf_settings_changed, state);
    } else if(state->args.entry == MfSettingsEntryRxCallsigns) {
        state->items[0] = variable_item_list_add(list, "Length", 6U, mf_settings_changed, state);
        state->items[1] = variable_item_list_add(list, "WPM", 21U, mf_settings_changed, state);
        state->items[2] = variable_item_list_add(
            list,
            "Farnsworth",
            state->snapshot.rx_callsigns_wpm,
            mf_settings_changed,
            state);
    } else if(state->args.entry == MfSettingsEntryGpio) {
        state->items[0] = variable_item_list_add(list, "dit/SK", 6U, mf_settings_changed, state);
        state->items[1] = variable_item_list_add(list, "dah", 6U, mf_settings_changed, state);
        state->items[2] = variable_item_list_add(list, "Virtual gnd", 7U, mf_settings_changed, state);
        state->items[3] = variable_item_list_add(list, "PTT/TX", 2U, mf_settings_changed, state);
    } else if(state->args.entry == MfSettingsEntryUsb) {
        state->items[0] = variable_item_list_add(list, "Connection", 4U, mf_settings_changed, state);
        state->items[1] = variable_item_list_add(
            list, "Paddle keys", morse_pc_paddle_preset_count(), mf_settings_changed, state);
        state->items[2] = variable_item_list_add(
            list, "Straight key", morse_pc_straight_preset_count(), mf_settings_changed, state);
        state->items[3] = variable_item_list_add(list, "Invert mouse", 2U, mf_settings_changed, state);
    }
}

static void* mf_settings_alloc(void) { return calloc(1U, sizeof(MfSettingsState)); }
static void mf_settings_free(void* state) { free(state); }

static bool mf_settings_try_load_custom_names(MfSettingsState* state) {
    Storage* storage;
    File* file;
    uint16_t read = 0U;
    bool loaded = false;

    if(state == NULL) return false;
    state->custom_parse_scratch[0] = '\0';
    memset(&state->custom_names, 0, sizeof(state->custom_names));
    storage = furi_record_open(RECORD_STORAGE);
    if(storage == NULL) return false;
    file = storage_file_alloc(storage);
    if(file != NULL) {
        if(storage_file_open(file, MORSE_FLIPPER_CUSTOM_CHARS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
            read = storage_file_read(
                file, state->custom_parse_scratch, sizeof(state->custom_parse_scratch) - 1U);
            state->custom_parse_scratch[read] = '\0';
            loaded = mf_settings_parse_custom_names(
                state->custom_parse_scratch,
                sizeof(state->custom_parse_scratch),
                &state->custom_names);
            storage_file_close(file);
        }
        storage_file_free(file);
    }
    furi_record_close(RECORD_STORAGE);
    return loaded;
}

static void mf_settings_leave(void* opaque) {
    MfSettingsState* state = opaque;

    if(state == NULL) return;
    if(state->args.list != NULL) variable_item_list_reset(state->args.list);
    state->args = (MfSettingsEnterArgs){0};
    memset(state->items, 0, sizeof(state->items));
    state->entered = false;
}

static bool mf_settings_enter(void* opaque, const void* opaque_args, MorseFlipperMappedFalResult* initial) {
    MfSettingsState* state = opaque;
    const MfSettingsEnterArgs* args = opaque_args;

    if(initial != NULL) *initial = (MorseFlipperMappedFalResult){0};
    if(state == NULL || args == NULL || args->struct_size != sizeof(*args) ||
       args->entry > MfSettingsEntryUsb || args->list == NULL || args->services == NULL ||
       args->services->struct_size != sizeof(*args->services) || args->services->apply == NULL)
        return false;
    memset(state, 0, sizeof(*state));
    state->args = *args;
    state->snapshot = args->snapshot;
    mf_settings_snapshot_normalize(&state->snapshot);
    state->gpio_dit_pin = state->snapshot.gpio_dit_pin;
    state->gpio_dah_pin = state->snapshot.gpio_dah_pin;
    state->gpio_ground_pin = state->snapshot.gpio_ground_pin;
    state->gpio_ptt_pin = state->snapshot.gpio_ptt_pin;
    if(!mf_settings_gpio_selectable(state->gpio_dit_pin))
        state->gpio_dit_pin = MfSettingsGpioPinP7;
    if(!mf_settings_gpio_selectable(state->gpio_dah_pin))
        state->gpio_dah_pin = MfSettingsGpioPinP5;
    if(state->gpio_ground_pin != MfSettingsGpioPinNone &&
       !mf_settings_gpio_selectable(state->gpio_ground_pin))
        state->gpio_ground_pin = MfSettingsGpioPinP3;
    if(state->gpio_ptt_pin != MfSettingsGpioPinP16)
        state->gpio_ptt_pin = MfSettingsGpioPinNone;
    if(args->entry == MfSettingsEntryListening) (void)mf_settings_try_load_custom_names(state);
    variable_item_list_reset(args->list);
    variable_item_list_set_enter_callback(args->list, mf_settings_enter_row, state);
    mf_settings_build_rows(state);
    variable_item_list_set_selected_item(args->list, (uint8_t)(args->selected_state & 0xffU));
    mf_settings_refresh(state);
    state->entered = true;
    if(initial != NULL) *initial = (MorseFlipperMappedFalResult){.handled = true, .redraw = true};
    return true;
}

static MorseFlipperMappedFalResult mf_settings_input(
    void* state,
    const InputEvent* event,
    uint32_t now_ms) {
    UNUSED(state);
    UNUSED(event);
    UNUSED(now_ms);
    return (MorseFlipperMappedFalResult){.handled = true};
}

static MorseFlipperMappedFalResult mf_settings_tick(void* state, uint32_t now_ms) {
    UNUSED(state);
    UNUSED(now_ms);
    return (MorseFlipperMappedFalResult){.handled = true};
}

static void mf_settings_draw(void* state, Canvas* canvas, uint32_t now_ms) {
    UNUSED(state);
    UNUSED(canvas);
    UNUSED(now_ms);
}

static bool mf_settings_request_close(
    void* opaque,
    MfSettingsRequest* pending,
    MorseFlipperMappedFalResult* result) {
    MfSettingsState* state = opaque;

    if(state == NULL || !state->entered) return false;
    if(pending != NULL) pending->kind = MfSettingsRequestNone;
    if(state->args.entry == MfSettingsEntryGpio) {
        if(pending == NULL) return false;
        if(state->gpio_dit_pin != state->snapshot.gpio_dit_pin ||
           state->gpio_dah_pin != state->snapshot.gpio_dah_pin ||
           state->gpio_ground_pin != state->snapshot.gpio_ground_pin ||
           state->gpio_ptt_pin != state->snapshot.gpio_ptt_pin)
            *pending = (MfSettingsRequest){
                .kind = MfSettingsApplyGpioDraft,
                .gpio_dit_pin = state->gpio_dit_pin,
                .gpio_dah_pin = state->gpio_dah_pin,
                .gpio_ground_pin = state->gpio_ground_pin,
                .gpio_ptt_pin = state->gpio_ptt_pin,
            };
    }
    if(result != NULL) *result = (MorseFlipperMappedFalResult){.handled = true, .request_exit = true};
    return true;
}

static uint32_t mf_settings_selected_state(const void* opaque) {
    const MfSettingsState* state = opaque;

    if(state == NULL || state->args.list == NULL) return 0U;
    return variable_item_list_get_selected_item_index(state->args.list);
}

static const MfSettingsApi mf_settings_api = {
    .mapped = {
        .magic = MF_SETTINGS_API_MAGIC,
        .api_version = MF_SETTINGS_API_VERSION,
        .struct_size = sizeof(MfSettingsApi),
        .alloc = mf_settings_alloc,
        .free = mf_settings_free,
        .enter = mf_settings_enter,
        .leave = mf_settings_leave,
        .input = mf_settings_input,
        .tick = mf_settings_tick,
        .draw = mf_settings_draw,
    },
    .request_close = mf_settings_request_close,
    .selected_state = mf_settings_selected_state,
};

static const FlipperAppPluginDescriptor mf_settings_descriptor = {
    .appid = "morse_flipper",
    .ep_api_version = MF_SETTINGS_API_VERSION,
    .entry_point = &mf_settings_api,
};

const FlipperAppPluginDescriptor* morse_flipper_settings_ep(void) {
    return &mf_settings_descriptor;
}

#ifdef MF_SETTINGS_HOST_TEST
void* mf_settings_test_alloc(void) { return mf_settings_alloc(); }
void mf_settings_test_free(void* state) { mf_settings_free(state); }
bool mf_settings_test_enter(void* state, const MfSettingsEnterArgs* args) {
    return mf_settings_enter(state, args, NULL);
}
void mf_settings_test_leave(void* state) { mf_settings_leave(state); }
bool mf_settings_test_close(
    void* state,
    MfSettingsRequest* pending,
    MorseFlipperMappedFalResult* result) {
    return mf_settings_request_close(state, pending, result);
}
uint8_t mf_settings_test_custom_count(const void* state) {
    return state == NULL ? 0U : ((const MfSettingsState*)state)->custom_names.count;
}
#endif
