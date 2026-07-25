#include "morse_flipper_app_i.h"

static void morse_flipper_passive_settings_refresh(MorseFlipperApp* app) {
    VariableItem* item;
    char text[8];
    uint8_t wpm;
    uint8_t index;

    if(app == NULL) return;
    morse_flipper_clamp_passive_settings(app);
    item = app->passive_items[MorseFlipperPassiveSettingMode];
    if(item) {
        variable_item_set_current_value_index(item, app->passive_mode);
        variable_item_set_current_value_text(
            item, app->passive_mode == MorseFlipperPassiveModeLesson ? "Lesson" : "Callsign");
    }
    item = app->passive_items[MorseFlipperPassiveSettingLength];
    if(item) {
        variable_item_set_values_count(
            item, app->passive_mode == MorseFlipperPassiveModeLesson ? 4U : 3U);
        index = (uint8_t)(app->passive_length -
                          (app->passive_mode == MorseFlipperPassiveModeLesson ? 3U : 4U));
        variable_item_set_current_value_index(item, index);
        snprintf(text, sizeof(text), "%u", (unsigned)app->passive_length);
        variable_item_set_current_value_text(item, text);
    }
    item = app->passive_items[MorseFlipperPassiveSettingLesson];
    if(item) {
        const char* set = morse_flipper_passive_lesson_charset(app->passive_lesson);
        variable_item_set_current_value_index(item, (uint8_t)(app->passive_lesson - 1U));
        snprintf(text, sizeof(text), "%c", set[app->passive_lesson - 1U]);
        variable_item_set_current_value_text(item, text);
    }
    wpm = morse_flipper_passive_wpm(app);
    item = app->passive_items[MorseFlipperPassiveSettingWpm];
    if(item) {
        variable_item_set_current_value_index(item, (uint8_t)(wpm - 10U));
        snprintf(text, sizeof(text), "%u", (unsigned)wpm);
        variable_item_set_current_value_text(item, text);
    }
    item = app->passive_items[MorseFlipperPassiveSettingFarnsworth];
    if(item) {
        variable_item_set_values_count(item, wpm);
        variable_item_set_current_value_index(item, (uint8_t)(app->passive_farnsworth_wpm - 1U));
        snprintf(text, sizeof(text), "%u", (unsigned)app->passive_farnsworth_wpm);
        variable_item_set_current_value_text(item, text);
    }
    item = app->passive_items[MorseFlipperPassiveSettingVibrate];
    if(item) {
        variable_item_set_current_value_index(item, app->passive_vibrate);
        variable_item_set_current_value_text(item, app->passive_vibrate ? "On" : "Off");
    }
    item = app->passive_items[MorseFlipperPassiveSettingAnswerDelay];
    if(item) {
        variable_item_set_current_value_index(item, (uint8_t)(app->passive_answer_delay_s - 1U));
        snprintf(text, sizeof(text), "%u s", (unsigned)app->passive_answer_delay_s);
        variable_item_set_current_value_text(item, text);
    }
    item = app->passive_items[MorseFlipperPassiveSettingRepeat];
    if(item) {
        variable_item_set_current_value_index(item, app->passive_repeat_after_answer);
        variable_item_set_current_value_text(item, app->passive_repeat_after_answer ? "Yes" : "No");
    }
}

static void morse_flipper_passive_mode_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    app->passive_mode = variable_item_get_current_value_index(item);
    morse_flipper_passive_settings_refresh(app);
    morse_flipper_save_config(app);
}

static void morse_flipper_passive_length_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    app->passive_length = (uint8_t)(variable_item_get_current_value_index(item) +
                                    (app->passive_mode == MorseFlipperPassiveModeLesson ? 3U : 4U));
    morse_flipper_passive_settings_refresh(app);
    morse_flipper_save_config(app);
}

static void morse_flipper_passive_lesson_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    app->passive_lesson = (uint8_t)(variable_item_get_current_value_index(item) + 1U);
    morse_flipper_passive_settings_refresh(app);
    morse_flipper_save_config(app);
}

static void morse_flipper_passive_wpm_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t wpm = (uint8_t)(variable_item_get_current_value_index(item) + 10U);
    app->passive_dit_ms = (uint16_t)((1200U + wpm / 2U) / wpm);
    morse_flipper_passive_settings_refresh(app);
    morse_flipper_save_config(app);
}

static void morse_flipper_passive_farnsworth_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    app->passive_farnsworth_wpm = (uint8_t)(variable_item_get_current_value_index(item) + 1U);
    morse_flipper_passive_settings_refresh(app);
    morse_flipper_save_config(app);
}

static void morse_flipper_passive_bool_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    if(item == app->passive_items[MorseFlipperPassiveSettingVibrate])
        app->passive_vibrate = variable_item_get_current_value_index(item);
    else
        app->passive_repeat_after_answer = variable_item_get_current_value_index(item);
    morse_flipper_passive_settings_refresh(app);
    morse_flipper_save_config(app);
}

static void morse_flipper_passive_delay_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    app->passive_answer_delay_s = (uint8_t)(variable_item_get_current_value_index(item) + 1U);
    morse_flipper_passive_settings_refresh(app);
    morse_flipper_save_config(app);
}

void morse_flipper_scene_passive_cfg_on_enter(void* context) {
    MorseFlipperApp* app = context;
    VariableItem* item;
    uint32_t selected = scene_manager_get_scene_state(app->scene_manager, MorseFlipperScenePassiveCfg);
    morse_flipper_ensure_view(app, MorseFlipperViewSettings);
    variable_item_list_reset(app->settings_list);
    memset(app->passive_items, 0, sizeof(app->passive_items));
    variable_item_list_set_enter_callback(app->settings_list, morse_flipper_settings_noop_enter, app);
    item = variable_item_list_add(app->settings_list, "Mode", 2U, morse_flipper_passive_mode_changed, app);
    app->passive_items[MorseFlipperPassiveSettingMode] = item;
    item = variable_item_list_add(app->settings_list, "Length", 4U, morse_flipper_passive_length_changed, app);
    app->passive_items[MorseFlipperPassiveSettingLength] = item;
    item = variable_item_list_add(app->settings_list, "Lesson", (uint8_t)morse_flipper_passive_lesson_count(), morse_flipper_passive_lesson_changed, app);
    app->passive_items[MorseFlipperPassiveSettingLesson] = item;
    item = variable_item_list_add(app->settings_list, "WPM", 21U, morse_flipper_passive_wpm_changed, app);
    app->passive_items[MorseFlipperPassiveSettingWpm] = item;
    item = variable_item_list_add(app->settings_list, "Farnsworth", morse_flipper_passive_wpm(app), morse_flipper_passive_farnsworth_changed, app);
    app->passive_items[MorseFlipperPassiveSettingFarnsworth] = item;
    item = variable_item_list_add(app->settings_list, "Vibrate", 2U, morse_flipper_passive_bool_changed, app);
    app->passive_items[MorseFlipperPassiveSettingVibrate] = item;
    item = variable_item_list_add(app->settings_list, "Delay before answer", 5U, morse_flipper_passive_delay_changed, app);
    app->passive_items[MorseFlipperPassiveSettingAnswerDelay] = item;
    item = variable_item_list_add(app->settings_list, "Repeat after answer", 2U, morse_flipper_passive_bool_changed, app);
    app->passive_items[MorseFlipperPassiveSettingRepeat] = item;
    morse_flipper_passive_settings_refresh(app);
    morse_flipper_settings_list_restore(app->settings_list, selected > MorseFlipperPassiveSettingRepeat ? 0U : selected);
    morse_flipper_scene_enter_now(app, MorseFlipperScenePassiveCfg);
}

void morse_flipper_scene_passive_cfg_on_exit(void* context) {
    MorseFlipperApp* app = context;
    scene_manager_set_scene_state(app->scene_manager, MorseFlipperScenePassiveCfg, morse_flipper_settings_list_state(app->settings_list));
    variable_item_list_reset(app->settings_list);
    memset(app->passive_items, 0, sizeof(app->passive_items));
}
