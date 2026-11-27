#include "mf_ardf_api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <flipper_application/flipper_application.h>
#include <gui/modules/variable_item_list.h>

#include "mf_ardf_core.h"
#include "mf_ardf_draw.h"
#include "mf_ardf_hal.h"
#include "mf_ardf_settings.h"

enum {
    MfArdfSettingStart = 0,
    MfArdfSettingMode,
    MfArdfSettingModulation,
    MfArdfSettingMessage,
    MfArdfSettingCustom,
    MfArdfSettingInterval,
    MfArdfSettingLight,
    MfArdfSettingAudio,
    MfArdfSettingWpm,
};

static const char* const mode_names[] = {"Custom", "Sprint", "Standard"};
static const char* const modulation_names[] = {"CW", "CWFM"};
static const char* const message_names[] =
    {"1 - MOE", "2 - MOI", "3 - MOS", "4 - MOH", "5 - MO5", "S", "MO"};
static const char* const yes_no_names[] = {"No", "Yes"};
static const char custom_header[] = "Custom identifier";
static const char stop_header[] = "Stop ARDF Fox?";
static const char error_header[] = "ARDF error";
static const char frequency_error[] = "Frequency unavailable";
static const char output_error[] = "ARDF output failed";

static void mf_ardf_refresh_item(MfArdfState* state, uint8_t row) {
    VariableItem* item = state->setting_items[row];
    char text[8];
    if(item == NULL || row == MfArdfSettingStart) return;
    if(row == MfArdfSettingMode) {
        variable_item_set_current_value_index(item, state->snapshot.settings.mode);
        variable_item_set_current_value_text(item, mode_names[state->snapshot.settings.mode]);
    } else if(row == MfArdfSettingModulation) {
        variable_item_set_current_value_index(item, state->snapshot.settings.modulation);
        variable_item_set_current_value_text(
            item, modulation_names[state->snapshot.settings.modulation]);
    } else if(row == MfArdfSettingMessage) {
        variable_item_set_current_value_index(item, state->snapshot.settings.message);
        variable_item_set_current_value_text(
            item, message_names[state->snapshot.settings.message]);
    } else if(row == MfArdfSettingCustom) {
        variable_item_set_current_value_text(item, state->snapshot.settings.custom);
    } else if(row == MfArdfSettingInterval) {
        variable_item_set_current_value_index(item, state->snapshot.settings.interval_index);
        variable_item_set_current_value_text(
            item, mf_ardf_interval_label(state->snapshot.settings.interval_index));
    } else if(row == MfArdfSettingLight) {
        variable_item_set_current_value_index(item, state->snapshot.settings.light_assistance);
        variable_item_set_current_value_text(
            item, yes_no_names[state->snapshot.settings.light_assistance]);
    } else if(row == MfArdfSettingAudio) {
        variable_item_set_current_value_index(item, state->snapshot.settings.audio_output);
        variable_item_set_current_value_text(
            item, yes_no_names[state->snapshot.settings.audio_output]);
    } else if(row == MfArdfSettingWpm) {
        variable_item_set_current_value_index(item, state->snapshot.settings.wpm - 8U);
        snprintf(text, sizeof(text), "%u", state->snapshot.settings.wpm);
        variable_item_set_current_value_text(item, text);
    }
}

static void mf_ardf_setting_changed(VariableItem* item) {
    MfArdfState* state = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    uint8_t row;
    for(row = 1U; row < MF_ARDF_SETTING_COUNT; row++)
        if(state->setting_items[row] == item) break;
    if(row == MfArdfSettingMode)
        state->snapshot.settings.mode = index;
    else if(row == MfArdfSettingModulation)
        state->snapshot.settings.modulation = index;
    else if(row == MfArdfSettingMessage)
        state->snapshot.settings.message = index;
    else if(row == MfArdfSettingInterval)
        state->snapshot.settings.interval_index = index;
    else if(row == MfArdfSettingLight)
        state->snapshot.settings.light_assistance = index;
    else if(row == MfArdfSettingAudio)
        state->snapshot.settings.audio_output = index;
    else if(row == MfArdfSettingWpm)
        state->snapshot.settings.wpm = index + 8U;
    else
        return;
    mf_ardf_refresh_item(state, row);
    mf_ardf_settings_save(&state->snapshot.settings);
}

static void mf_ardf_setting_enter(void* context, uint32_t index) {
    MfArdfState* state = context;
    if(index == MfArdfSettingStart) {
        state->snapshot.view = MfArdfViewClock;
        state->snapshot.clock_state = MfArdfClockConfirm;
        state->sampling = true;
        state->previous_sample_valid = false;
    } else if(index == MfArdfSettingCustom) {
        state->snapshot.host_action = MfArdfHostActionOpenTextInput;
    }
}

static bool mf_ardf_populate_settings_api(void* context, VariableItemList* list) {
    MfArdfState* state = context;
    uint8_t row;
    if(state == NULL || list == NULL || !state->entered) return false;
    state->settings_list = list;
    variable_item_list_reset(list);
    variable_item_list_set_enter_callback(list, mf_ardf_setting_enter, state);
    state->setting_items[MfArdfSettingStart] =
        variable_item_list_add(list, "Start ARDF Fox", 0U, NULL, state);
    state->setting_items[MfArdfSettingMode] =
        variable_item_list_add(list, "Mode", 3U, mf_ardf_setting_changed, state);
    state->setting_items[MfArdfSettingModulation] =
        variable_item_list_add(list, "Modulation", 2U, mf_ardf_setting_changed, state);
    state->setting_items[MfArdfSettingMessage] = variable_item_list_add(
        list, "Message", MfArdfMessageCount, mf_ardf_setting_changed, state);
    state->setting_items[MfArdfSettingCustom] =
        variable_item_list_add(list, "Custom", 0U, NULL, state);
    state->setting_items[MfArdfSettingInterval] = variable_item_list_add(
        list, "Custom interval", MF_ARDF_INTERVAL_COUNT, mf_ardf_setting_changed, state);
    state->setting_items[MfArdfSettingLight] =
        variable_item_list_add(list, "Light assistance", 2U, mf_ardf_setting_changed, state);
    state->setting_items[MfArdfSettingAudio] =
        variable_item_list_add(list, "Audio output", 2U, mf_ardf_setting_changed, state);
    state->setting_items[MfArdfSettingWpm] =
        variable_item_list_add(list, "WPM", 23U, mf_ardf_setting_changed, state);
    for(row = 1U; row < MF_ARDF_SETTING_COUNT; row++)
        mf_ardf_refresh_item(state, row);
    variable_item_list_set_selected_item(list, state->snapshot.settings.selected_row);
    return true;
}

static void* mf_ardf_alloc(void) {
    return calloc(1U, sizeof(MfArdfState));
}

static void mf_ardf_free(void* state) {
    free(state);
}

static bool
    mf_ardf_enter_api(void* context, const void* args, MorseFlipperMappedFalResult* initial) {
    mf_ardf_hal_init();
    bool entered = mf_ardf_core_enter(context, args, mf_ardf_hal_ops(), initial);
    if(entered)
        mf_ardf_hal_rtc_sample(context);
    else
        mf_ardf_hal_deinit();
    return entered;
}

static void mf_ardf_leave_api(void* context) {
    MfArdfState* state = context;
    if(state != NULL && state->settings_list != NULL) {
        state->snapshot.settings.selected_row =
            variable_item_list_get_selected_item_index(state->settings_list);
        mf_ardf_settings_save(&state->snapshot.settings);
        variable_item_list_reset(state->settings_list);
        state->settings_list = NULL;
    }
    mf_ardf_core_leave(state);
    mf_ardf_hal_deinit();
}

static MorseFlipperMappedFalResult
    mf_ardf_input_api(void* state, const InputEvent* event, uint32_t now_ms) {
    mf_ardf_hal_rtc_sample(state);
    return mf_ardf_core_input(state, event, now_ms);
}

static MorseFlipperMappedFalResult mf_ardf_tick_api(void* state, uint32_t now_ms) {
    mf_ardf_hal_rtc_sample(state);
    return mf_ardf_core_tick(state, now_ms);
}

static void mf_ardf_draw_api(void* state, Canvas* canvas, uint32_t now_ms) {
    mf_ardf_draw(state, canvas, now_ms);
}

static MorseFlipperMappedFalResult mf_ardf_command_api(
    void* state,
    uint32_t command,
    const void* input,
    void* output,
    uint32_t now_ms) {
    MorseFlipperMappedFalResult result = {0};
    if(command == MfArdfCommandHostActionInfo && output != NULL) {
        MfArdfState* ardf = state;
        MfArdfHostActionInfo* info = output;
        if(ardf->snapshot.host_action == MfArdfHostActionOpenTextInput) {
            *info = (MfArdfHostActionInfo){
                .header = custom_header, .text = ardf->snapshot.settings.custom};
            result.handled = true;
        } else if(ardf->snapshot.host_action == MfArdfHostActionShowStopConfirmation) {
            *info = (MfArdfHostActionInfo){.header = stop_header, .confirm = true};
            result.handled = true;
        } else if(ardf->snapshot.host_action == MfArdfHostActionShowError) {
            *info = (MfArdfHostActionInfo){
                .header = error_header,
                .text = ardf->snapshot.error == MfArdfErrorFrequency ? frequency_error :
                                                                       output_error,
            };
            result.handled = true;
        }
    } else if(command == MfArdfCommandPopulateSettings && input != NULL) {
        result.handled = mf_ardf_populate_settings_api(state, (VariableItemList*)input);
    } else if(command == MfArdfCommandTextResult && input != NULL) {
        const MfArdfTextResultCommand* text = input;
        mf_ardf_hal_rtc_sample(state);
        result = mf_ardf_core_text_input_result(state, text->text, text->accepted, now_ms);
        mf_ardf_refresh_item(state, MfArdfSettingCustom);
    } else if(command == MfArdfCommandHostActionResult && input != NULL) {
        const MfArdfHostActionResultCommand* action = input;
        mf_ardf_hal_rtc_sample(state);
        result = mf_ardf_core_host_action_result(state, action->action, action->accepted, now_ms);
    } else if(command == MfArdfCommandActivateRun) {
        result = mf_ardf_core_activate_run(state, now_ms);
    }
    return result;
}

static const MfArdfApi ardf_api = {
    .fal =
        {
            .mapped =
                {
                    .magic = MF_ARDF_API_MAGIC,
                    .api_version = MF_ARDF_API_VERSION,
                    .struct_size = sizeof(MfArdfApi),
                    .alloc = mf_ardf_alloc,
                    .free = mf_ardf_free,
                    .enter = mf_ardf_enter_api,
                    .leave = mf_ardf_leave_api,
                    .input = mf_ardf_input_api,
                    .tick = mf_ardf_tick_api,
                    .draw = mf_ardf_draw_api,
                },
            .command = mf_ardf_command_api,
        },
};

static const FlipperAppPluginDescriptor descriptor = {
    .appid = "morse_flipper",
    .ep_api_version = MF_ARDF_API_VERSION,
    .entry_point = &ardf_api,
};

const FlipperAppPluginDescriptor* morse_flipper_ardf_ep(void) {
    return &descriptor;
}
