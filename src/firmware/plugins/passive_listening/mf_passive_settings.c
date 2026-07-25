#include "mf_passive_core.h"

#include <gui/modules/variable_item_list.h>
#include <stdio.h>

enum {
    MfPassiveSettingMode = 0,
    MfPassiveSettingLength,
    MfPassiveSettingLesson,
    MfPassiveSettingWpm,
    MfPassiveSettingFarnsworth,
    MfPassiveSettingVibrate,
    MfPassiveSettingAnswerDelay,
    MfPassiveSettingRepeat,
    MfPassiveSettingCount,
};

static void mf_passive_settings_persist(MfPassiveState* state) {
    mf_passive_settings_normalize(&state->settings_model);
    mf_passive_settings_save(&state->settings_model);
}

static void mf_passive_settings_refresh(MfPassiveState* state) {
    VariableItem* item;
    char text[8];
    uint8_t wpm;

    mf_passive_settings_normalize(&state->settings_model);
    item = state->settings_items[MfPassiveSettingMode];
    variable_item_set_current_value_index(item, state->settings_model.mode);
    variable_item_set_current_value_text(item, state->settings_model.mode ? "Lesson" : "Callsign");
    item = state->settings_items[MfPassiveSettingLength];
    variable_item_set_values_count(item, state->settings_model.mode ? 4U : 3U);
    variable_item_set_current_value_index(
        item, state->settings_model.length - (state->settings_model.mode ? 3U : 4U));
    snprintf(text, sizeof(text), "%u", (unsigned)state->settings_model.length);
    variable_item_set_current_value_text(item, text);
    item = state->settings_items[MfPassiveSettingLesson];
    variable_item_set_current_value_index(item, state->settings_model.lesson - 1U);
    mf_passive_settings_lesson_label(state->settings_model.lesson, text, sizeof(text));
    variable_item_set_current_value_text(item, text);
    wpm = mf_passive_settings_wpm(&state->settings_model);
    item = state->settings_items[MfPassiveSettingWpm];
    variable_item_set_current_value_index(item, wpm - 10U);
    snprintf(text, sizeof(text), "%u", (unsigned)wpm);
    variable_item_set_current_value_text(item, text);
    item = state->settings_items[MfPassiveSettingFarnsworth];
    variable_item_set_values_count(item, wpm);
    variable_item_set_current_value_index(item, state->settings_model.farnsworth_wpm - 1U);
    snprintf(text, sizeof(text), "%u", (unsigned)state->settings_model.farnsworth_wpm);
    variable_item_set_current_value_text(item, text);
    item = state->settings_items[MfPassiveSettingVibrate];
    variable_item_set_current_value_index(item, state->settings_model.vibrate);
    variable_item_set_current_value_text(item, state->settings_model.vibrate ? "On" : "Off");
    item = state->settings_items[MfPassiveSettingAnswerDelay];
    variable_item_set_current_value_index(item, state->settings_model.answer_delay_s - 1U);
    snprintf(text, sizeof(text), "%u s", (unsigned)state->settings_model.answer_delay_s);
    variable_item_set_current_value_text(item, text);
    item = state->settings_items[MfPassiveSettingRepeat];
    variable_item_set_current_value_index(item, state->settings_model.repeat_after_answer);
    variable_item_set_current_value_text(item, state->settings_model.repeat_after_answer ? "Yes" : "No");
}

static void mf_passive_settings_changed(VariableItem* item) {
    MfPassiveState* state = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    if(item == state->settings_items[MfPassiveSettingMode])
        state->settings_model.mode = index;
    else if(item == state->settings_items[MfPassiveSettingLength])
        state->settings_model.length = index + (state->settings_model.mode ? 3U : 4U);
    else if(item == state->settings_items[MfPassiveSettingLesson])
        state->settings_model.lesson = index + 1U;
    else if(item == state->settings_items[MfPassiveSettingWpm])
        state->settings_model.dit_ms = (1200U + (index + 10U) / 2U) / (index + 10U);
    else if(item == state->settings_items[MfPassiveSettingFarnsworth])
        state->settings_model.farnsworth_wpm = index + 1U;
    else if(item == state->settings_items[MfPassiveSettingVibrate])
        state->settings_model.vibrate = index;
    else if(item == state->settings_items[MfPassiveSettingAnswerDelay])
        state->settings_model.answer_delay_s = index + 1U;
    else
        state->settings_model.repeat_after_answer = index;
    mf_passive_settings_refresh(state);
    mf_passive_settings_persist(state);
}

static void mf_passive_settings_noop_enter(void* context, uint32_t index) {
    (void)context;
    (void)index;
}

bool mf_passive_settings_enter(MfPassiveState* state, const MfPassiveEnterArgs* args) {
    const MfPassiveSettingsArgs* settings = &args->entry.settings;
    VariableItemList* list;

    if(settings->list == NULL) return false;
    state->settings = *settings;
    mf_passive_settings_load(&state->settings_model);
    state->settings_active = true;
    list = settings->list;
    variable_item_list_reset(list);
    variable_item_list_set_enter_callback(list, mf_passive_settings_noop_enter, state);
    state->settings_items[MfPassiveSettingMode] =
        variable_item_list_add(list, "Mode", 2U, mf_passive_settings_changed, state);
    state->settings_items[MfPassiveSettingLength] =
        variable_item_list_add(list, "Length", 3U, mf_passive_settings_changed, state);
    state->settings_items[MfPassiveSettingLesson] = variable_item_list_add(
        list,
        "Lesson",
        (uint8_t)mf_passive_settings_lesson_count(),
        mf_passive_settings_changed,
        state);
    state->settings_items[MfPassiveSettingWpm] =
        variable_item_list_add(list, "WPM", 21U, mf_passive_settings_changed, state);
    state->settings_items[MfPassiveSettingFarnsworth] =
        variable_item_list_add(list, "Farnsworth", 30U, mf_passive_settings_changed, state);
    state->settings_items[MfPassiveSettingVibrate] =
        variable_item_list_add(list, "Vibrate", 2U, mf_passive_settings_changed, state);
    state->settings_items[MfPassiveSettingAnswerDelay] =
        variable_item_list_add(list, "Delay before answer", 5U, mf_passive_settings_changed, state);
    state->settings_items[MfPassiveSettingRepeat] =
        variable_item_list_add(list, "Repeat after answer", 2U, mf_passive_settings_changed, state);
    mf_passive_settings_refresh(state);
    variable_item_list_set_selected_item(
        list,
        state->settings_model.selected_row < MfPassiveSettingCount ?
            state->settings_model.selected_row :
            0U);
    return true;
}

void mf_passive_settings_leave(MfPassiveState* state) {
    if(state == NULL || !state->settings_active) return;
    state->settings_model.selected_row =
        variable_item_list_get_selected_item_index(state->settings.list);
    mf_passive_settings_save(&state->settings_model);
    variable_item_list_reset(state->settings.list);
    state->settings_active = false;
}
