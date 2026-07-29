#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <gui/modules/variable_item_list.h>

#include "mf_passive_settings.h"
#include "mf_passive_policy.h"

static unsigned checks;
static unsigned save_calls;
static bool save_outcomes[4];
static MfPassiveSettingsModel saved_model;
static MfPassiveSettingsModel loaded_model;

#define CHECK(value)   \
    do {               \
        assert(value); \
        checks++;      \
    } while(0)

void mf_passive_settings_load(MfPassiveSettingsModel* model) {
    *model = loaded_model;
}

bool mf_passive_settings_save(const MfPassiveSettingsModel* model) {
    saved_model = *model;
    return save_outcomes[save_calls++];
}

void mf_passive_settings_normalize(MfPassiveSettingsModel* model) {
    uint8_t wpm;

    if(model->mode > 1U) model->mode = 0U;
    if(model->mode) {
        if(model->length < 1U) model->length = 1U;
        if(model->length > 9U) model->length = 1U;
    } else {
        if(model->length < 4U || model->length > 9U) model->length = 4U;
    }
    if(model->lesson < 1U || model->lesson > mf_passive_settings_lesson_count())
        model->lesson = 1U;
    if(model->dit_ms == 0U) model->dit_ms = 100U;
    wpm = mf_passive_settings_wpm(model);
    if(model->farnsworth_wpm < 1U || model->farnsworth_wpm > wpm) model->farnsworth_wpm = wpm;
    model->vibrate = model->vibrate ? 1U : 0U;
    if(model->answer_delay_s < 1U || model->answer_delay_s > 5U) model->answer_delay_s = 1U;
    model->repeat_after_answer = model->repeat_after_answer ? 1U : 0U;
    if(model->courtesy_delay_half_s > 10U) model->courtesy_delay_half_s = 2U;
    if(model->selected_row >= 9U) model->selected_row = 0U;
}

uint8_t mf_passive_settings_wpm(const MfPassiveSettingsModel* model) {
    if(model == NULL || model->dit_ms == 0U) return 12U;
    return (uint8_t)((1200U + model->dit_ms / 2U) / model->dit_ms);
}

size_t mf_passive_settings_lesson_count(void) {
    return 39U;
}

const char* mf_passive_settings_length_label(uint8_t selection) {
    static const char* const labels[] = {"", "1", "2", "3", "4", "5", "6", "4-5", "5-6", "4-6"};
    return selection < 10U ? labels[selection] : "?";
}

void mf_passive_settings_lesson_label(uint8_t lesson, char* out, size_t out_size) {
    snprintf(out, out_size, "%u - K", (unsigned)lesson);
}

VariableItem* variable_item_list_add(
    VariableItemList* list,
    const char* label,
    uint8_t count,
    VariableItemChangeCallback changed,
    void* context) {
    VariableItem* item = &list->items[list->count++];

    *item = (VariableItem){
        .label = label, .values_count = count, .changed = changed, .context = context};
    return item;
}

void variable_item_list_reset(VariableItemList* list) {
    memset(list->items, 0, sizeof(list->items));
    list->count = 0U;
    list->resets++;
}

void variable_item_list_set_enter_callback(
    VariableItemList* list,
    VariableItemListEnterCallback callback,
    void* context) {
    list->enter = callback;
    list->enter_context = context;
}

void variable_item_list_set_selected_item(VariableItemList* list, uint8_t selected) {
    list->selected = selected;
}

uint8_t variable_item_list_get_selected_item_index(VariableItemList* list) {
    return list->selected;
}

void variable_item_set_values_count(VariableItem* item, uint8_t count) {
    item->values_count = count;
}

void variable_item_set_current_value_index(VariableItem* item, uint8_t index) {
    item->current_index = index;
}

uint8_t variable_item_get_current_value_index(VariableItem* item) {
    return item->current_index;
}

void variable_item_set_current_value_text(VariableItem* item, const char* text) {
    snprintf(item->value_text, sizeof(item->value_text), "%s", text == NULL ? "" : text);
    item->current_text = item->value_text;
}

void* variable_item_get_context(VariableItem* item) {
    return item->context;
}

static void setup(MfPassiveSettingsState* state, VariableItemList* list, uint8_t selected_row) {
    MfPassiveEnterArgs args = {
        .struct_size = sizeof(args),
        .entry_kind = MfPassiveEntrySettings,
        .entry.settings = {.list = list},
    };
    MfPassiveResult result = {0};

    save_calls = 0U;
    memset(save_outcomes, 0, sizeof(save_outcomes));
    memset(&saved_model, 0, sizeof(saved_model));
    loaded_model = (MfPassiveSettingsModel){
        .length = 4U,
        .lesson = 1U,
        .dit_ms = 100U,
        .farnsworth_wpm = 12U,
        .vibrate = 1U,
        .answer_delay_s = 3U,
        .courtesy_delay_half_s = 2U,
        .selected_row = selected_row,
    };
    memset(list, 0, sizeof(*list));
    memset(state, 0, sizeof(*state));
    CHECK(mf_passive_settings_enter(state, &args, &result));
    CHECK(result.handled && result.redraw);
    CHECK(list->count == 9U && list->resets == 1U);
}

static void change(VariableItemList* list, uint8_t row, uint8_t value) {
    list->items[row].current_index = value;
    list->items[row].changed(&list->items[row]);
}

static void test_edit_save_success(void) {
    MfPassiveSettingsState state;
    VariableItemList list;

    setup(&state, &list, 0U);
    save_outcomes[0] = true;
    change(&list, 0U, 1U);
    CHECK(save_calls == 1U);
    CHECK(!state.dirty && !state.save_failed);
    CHECK(strcmp(list.items[0].current_text, "Lesson") == 0);
    CHECK(list.resets == 1U);
}

static void test_edit_save_failure_and_recovery(void) {
    MfPassiveSettingsState state;
    VariableItemList list;

    setup(&state, &list, 0U);
    save_outcomes[0] = false;
    save_outcomes[1] = true;
    change(&list, 0U, 1U);
    CHECK(save_calls == 1U);
    CHECK(state.dirty && state.save_failed);
    CHECK(strcmp(list.items[0].current_text, "Save failed") == 0);
    CHECK(list.resets == 1U);
    change(&list, 5U, 0U);
    CHECK(save_calls == 2U);
    CHECK(!state.dirty && !state.save_failed);
    CHECK(strcmp(list.items[0].current_text, "Lesson") == 0);
    CHECK(strcmp(list.items[5].current_text, "Off") == 0);
}

static void test_leave_retry_success(void) {
    MfPassiveSettingsState state;
    VariableItemList list;

    setup(&state, &list, 0U);
    save_outcomes[0] = false;
    save_outcomes[1] = true;
    change(&list, 0U, 1U);
    mf_passive_settings_leave(&state);
    CHECK(save_calls == 2U);
    CHECK(!state.active && list.count == 0U && list.resets == 2U);
}

static void test_leave_retry_failure_is_bounded(void) {
    MfPassiveSettingsState state;
    VariableItemList list;

    setup(&state, &list, 0U);
    change(&list, 0U, 1U);
    mf_passive_settings_leave(&state);
    CHECK(save_calls == 2U);
    CHECK(!state.active && list.count == 0U && list.resets == 2U);
}

static void test_selected_row_only_save(void) {
    MfPassiveSettingsState state;
    VariableItemList list;

    setup(&state, &list, 0U);
    save_outcomes[0] = true;
    list.selected = 3U;
    mf_passive_settings_leave(&state);
    CHECK(save_calls == 1U);
    CHECK(saved_model.selected_row == 3U);
}

static void test_noop_leave_has_no_write(void) {
    MfPassiveSettingsState state;
    VariableItemList list;

    setup(&state, &list, 2U);
    mf_passive_settings_leave(&state);
    CHECK(save_calls == 0U);
    CHECK(list.count == 0U && list.resets == 2U);
}

int main(void) {
    test_edit_save_success();
    test_edit_save_failure_and_recovery();
    test_leave_retry_success();
    test_leave_retry_failure_is_bounded();
    test_selected_row_only_save();
    test_noop_leave_has_no_write();
    printf("test_passive_settings: %u checks passed\n", checks);
    return 0;
}
