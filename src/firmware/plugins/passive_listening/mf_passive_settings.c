#include "mf_passive_settings.h"

#include "mf_passive_policy.h"

#include <gui/modules/variable_item_list.h>
#include <stdio.h>
#include <string.h>

enum {
    MfPassiveSettingMode = 0,
    MfPassiveSettingLength,
    MfPassiveSettingLesson,
    MfPassiveSettingWpm,
    MfPassiveSettingFarnsworth,
    MfPassiveSettingVibrate,
    MfPassiveSettingAnswerDelay,
    MfPassiveSettingRepeat,
    MfPassiveSettingCourtesyDelay,
    MfPassiveSettingCount,
};

static bool mf_passive_settings_persist(MfPassiveSettingsState* state) {
    mf_passive_settings_normalize(&state->model);
    if(mf_passive_settings_save(&state->model)) {
        state->dirty = false;
        state->save_failed = false;
        return true;
    }
    state->dirty = true;
    state->save_failed = true;
    return false;
}

static void mf_passive_settings_refresh(MfPassiveSettingsState* state) {
    VariableItem* item;
    char text[12];
    uint8_t wpm;

    mf_passive_settings_normalize(&state->model);
    item = state->items[MfPassiveSettingMode];
    variable_item_set_current_value_index(item, state->model.mode);
    variable_item_set_current_value_text(item, state->model.mode ? "Lesson" : "Callsign");
    item = state->items[MfPassiveSettingLength];
    variable_item_set_values_count(item, state->model.mode ? 9U : 6U);
    variable_item_set_current_value_index(item, state->model.length - (state->model.mode ? 1U : 4U));
    variable_item_set_current_value_text(
        item, mf_passive_settings_length_label(state->model.length));
    item = state->items[MfPassiveSettingLesson];
    variable_item_set_current_value_index(item, state->model.lesson - 1U);
    mf_passive_settings_lesson_label(state->model.lesson, text, sizeof(text));
    variable_item_set_current_value_text(item, text);
    wpm = mf_passive_settings_wpm(&state->model);
    item = state->items[MfPassiveSettingWpm];
    variable_item_set_current_value_index(item, wpm - 10U);
    snprintf(text, sizeof(text), "%u", (unsigned)wpm);
    variable_item_set_current_value_text(item, text);
    item = state->items[MfPassiveSettingFarnsworth];
    variable_item_set_values_count(item, wpm);
    variable_item_set_current_value_index(item, state->model.farnsworth_wpm - 1U);
    snprintf(text, sizeof(text), "%u", (unsigned)state->model.farnsworth_wpm);
    variable_item_set_current_value_text(item, text);
    item = state->items[MfPassiveSettingVibrate];
    variable_item_set_current_value_index(item, state->model.vibrate);
    variable_item_set_current_value_text(item, state->model.vibrate ? "On" : "Off");
    item = state->items[MfPassiveSettingAnswerDelay];
    variable_item_set_current_value_index(item, state->model.answer_delay_s - 1U);
    snprintf(text, sizeof(text), "%u s", (unsigned)state->model.answer_delay_s);
    variable_item_set_current_value_text(item, text);
    item = state->items[MfPassiveSettingRepeat];
    variable_item_set_current_value_index(item, state->model.repeat_after_answer);
    variable_item_set_current_value_text(item, state->model.repeat_after_answer ? "Yes" : "No");
    item = state->items[MfPassiveSettingCourtesyDelay];
    variable_item_set_current_value_index(item, state->model.courtesy_delay_half_s);
    if(state->model.courtesy_delay_half_s == 0U) {
        variable_item_set_current_value_text(item, "Off");
    } else {
        snprintf(
            text,
            sizeof(text),
            "%u.%u s",
            (unsigned)(state->model.courtesy_delay_half_s / 2U),
            state->model.courtesy_delay_half_s & 1U ? 5U : 0U);
        variable_item_set_current_value_text(item, text);
    }
}

static void mf_passive_settings_changed(VariableItem* item) {
    MfPassiveSettingsState* state = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    if(item == state->items[MfPassiveSettingMode])
        state->model.mode = index;
    else if(item == state->items[MfPassiveSettingLength])
        state->model.length = index + (state->model.mode ? 1U : 4U);
    else if(item == state->items[MfPassiveSettingLesson])
        state->model.lesson = index + 1U;
    else if(item == state->items[MfPassiveSettingWpm])
        state->model.dit_ms = (1200U + (index + 10U) / 2U) / (index + 10U);
    else if(item == state->items[MfPassiveSettingFarnsworth])
        state->model.farnsworth_wpm = index + 1U;
    else if(item == state->items[MfPassiveSettingVibrate])
        state->model.vibrate = index;
    else if(item == state->items[MfPassiveSettingAnswerDelay])
        state->model.answer_delay_s = index + 1U;
    else if(item == state->items[MfPassiveSettingRepeat])
        state->model.repeat_after_answer = index;
    else
        state->model.courtesy_delay_half_s = index;
    mf_passive_settings_refresh(state);
    state->dirty = true;
    if(!mf_passive_settings_persist(state))
        variable_item_set_current_value_text(item, "Save failed");
}

static void mf_passive_settings_noop_enter(void* context, uint32_t index) {
    (void)context;
    (void)index;
}

bool mf_passive_settings_enter(
    MfPassiveSettingsState* state,
    const MfPassiveEnterArgs* args,
    MfPassiveResult* result) {
    const MfPassiveSettingsArgs* settings;
    VariableItemList* list;

    if(state == NULL || args == NULL || result == NULL ||
       args->struct_size != sizeof(*args) || args->entry_kind != MfPassiveEntrySettings)
        return false;
    settings = &args->entry.settings;
    if(settings->list == NULL) return false;
    memset(state, 0, sizeof(*state));
    state->settings = *settings;
    mf_passive_settings_load(&state->model);
    state->active = true;
    list = settings->list;
    variable_item_list_reset(list);
    variable_item_list_set_enter_callback(list, mf_passive_settings_noop_enter, state);
    state->items[MfPassiveSettingMode] =
        variable_item_list_add(list, "Mode", 2U, mf_passive_settings_changed, state);
    state->items[MfPassiveSettingLength] =
        variable_item_list_add(list, "Length", 6U, mf_passive_settings_changed, state);
    state->items[MfPassiveSettingLesson] = variable_item_list_add(
        list,
        "Lesson",
        (uint8_t)mf_passive_settings_lesson_count(),
        mf_passive_settings_changed,
        state);
    state->items[MfPassiveSettingWpm] =
        variable_item_list_add(list, "WPM", 21U, mf_passive_settings_changed, state);
    state->items[MfPassiveSettingFarnsworth] =
        variable_item_list_add(list, "Farnsworth", 30U, mf_passive_settings_changed, state);
    state->items[MfPassiveSettingVibrate] =
        variable_item_list_add(list, "Vibrate", 2U, mf_passive_settings_changed, state);
    state->items[MfPassiveSettingAnswerDelay] =
        variable_item_list_add(list, "Delay before answer", 5U, mf_passive_settings_changed, state);
    state->items[MfPassiveSettingRepeat] =
        variable_item_list_add(list, "Repeat after answer", 2U, mf_passive_settings_changed, state);
    state->items[MfPassiveSettingCourtesyDelay] =
        variable_item_list_add(list, "Courtesy tone", 11U, mf_passive_settings_changed, state);
    mf_passive_settings_refresh(state);
    variable_item_list_set_selected_item(
        list,
        state->model.selected_row < MfPassiveSettingCount ?
            state->model.selected_row :
            0U);
    *result = (MfPassiveResult){.handled = true, .redraw = true};
    return true;
}

void mf_passive_settings_leave(MfPassiveSettingsState* state) {
    uint8_t selected_row;

    if(state == NULL || !state->active) return;
    selected_row = variable_item_list_get_selected_item_index(state->settings.list);
    if(state->dirty || selected_row != state->model.selected_row) {
        state->model.selected_row = selected_row;
        mf_passive_settings_persist(state);
    }
    variable_item_list_reset(state->settings.list);
    memset(state, 0, sizeof(*state));
}
