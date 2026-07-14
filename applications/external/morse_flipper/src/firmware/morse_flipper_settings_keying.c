/*
 * Purpose: Handle WPM, input source, keyer, and handedness settings rows.
 * Owns: keying settings callbacks and persistence side effects.
 * Depends on: morse_flipper_app_i.h, keyer state, and trainer sync helpers.
 * Tests: tests/test_keyer.c covers underlying keyer behaviour.
 */

#include "morse_flipper_app_i.h"

void morse_flipper_settings_wpm_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    uint8_t wpm = (uint8_t)(10U + idx);
    char text[4];

    snprintf(text, sizeof(text), "%u", (unsigned)wpm);
    variable_item_set_current_value_text(item, text);
    morse_flipper_set_local_wpm(app, wpm);
    morse_flipper_trainer_sync_farn_item(app);
    morse_flipper_save_config(app);
}

void morse_flipper_settings_input_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    uint32_t now_ms = furi_get_tick();

    variable_item_set_current_value_text(item, morse_flipper_input_names[idx]);
    app->input_source = morse_flipper_input_values[idx];
    morse_flipper_clear_button_keying(app, now_ms);
    morse_flipper_refresh_keyer(app, now_ms);
    morse_flipper_poll(app);
    morse_flipper_save_config(app);
}

void morse_flipper_settings_keyer_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    uint32_t now_ms = furi_get_tick();

    app->keyer_mode = morse_flipper_keyer_values[idx];
    variable_item_set_current_value_text(item, morse_keyer_mode_name(app->keyer_mode));
    morse_flipper_refresh_keyer(app, now_ms);
    morse_flipper_poll(app);
    morse_flipper_save_config(app);
}

void morse_flipper_settings_swap_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    uint32_t now_ms = furi_get_tick();

    app->handedness = idx == 0U ? MorseFlipperHandednessNormal : MorseFlipperHandednessSwapped;
    variable_item_set_current_value_text(
        item, app->handedness == MorseFlipperHandednessSwapped ? "Yes" : "No");
    morse_flipper_resync_button_paddles(app, now_ms);
    morse_flipper_refresh_keyer(app, now_ms);
    morse_flipper_poll(app);
    morse_flipper_save_config(app);
}

void morse_flipper_scene_home_on_enter(void* context) {
    MorseFlipperApp* app = context;
    VariableItem* item;
    uint8_t sel = scene_manager_get_scene_state(app->scene_manager, MorseFlipperSceneHome);
    uint8_t idx;
    char wpm_text[4];

    morse_flipper_ensure_view(app, MorseFlipperViewSettings);
    variable_item_list_reset(app->settings_list);
    variable_item_list_set_enter_callback(
        app->settings_list, morse_flipper_settings_enter_callback, app);

    item = variable_item_list_add(
        app->settings_list, "WPM", 21U, morse_flipper_settings_wpm_changed, app);
    idx = (uint8_t)(morse_flipper_current_wpm(app) < 10U ? 0U :
                                                           morse_flipper_current_wpm(app) - 10U);
    if(idx > 20U) idx = 20U;
    snprintf(wpm_text, sizeof(wpm_text), "%u", (unsigned)(idx + 10U));
    variable_item_set_current_value_index(item, idx);
    variable_item_set_current_value_text(item, wpm_text);

    item = variable_item_list_add(
        app->settings_list,
        "Input",
        COUNT_OF(morse_flipper_input_values),
        morse_flipper_settings_input_changed,
        app);
    idx = morse_flipper_input_value_index(app->input_source);
    variable_item_set_current_value_index(item, idx);
    variable_item_set_current_value_text(item, morse_flipper_input_names[idx]);

    item = variable_item_list_add(
        app->settings_list,
        "Keyer",
        COUNT_OF(morse_flipper_keyer_values),
        morse_flipper_settings_keyer_changed,
        app);
    idx = morse_flipper_keyer_value_index(app->keyer_mode);
    variable_item_set_current_value_index(item, idx);
    variable_item_set_current_value_text(
        item, morse_keyer_mode_name(morse_flipper_keyer_values[idx]));

    item = variable_item_list_add(
        app->settings_list, "Swap paddles", 2U, morse_flipper_settings_swap_changed, app);
    idx = app->handedness == MorseFlipperHandednessSwapped ? 1U : 0U;
    variable_item_set_current_value_index(item, idx);
    variable_item_set_current_value_text(item, idx ? "Yes" : "No");

    variable_item_list_add(app->settings_list, "Audio output", 0U, NULL, app);

    variable_item_list_add(app->settings_list, "GPIO", 0U, NULL, app);
    if(sel > MorseFlipperSettingGpio) sel = MorseFlipperSettingWpm;
    variable_item_list_set_selected_item(app->settings_list, sel);
    morse_flipper_scene_enter_now(app, MorseFlipperSceneHome);
}

bool morse_flipper_scene_home_on_event(void* context, SceneManagerEvent event) {
    MorseFlipperApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        morse_flipper_scene_back(app);
        return true;
    }

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MorseFlipperSettingAudio) {
            scene_manager_next_scene(app->scene_manager, MorseFlipperSceneAudioCfg);
            return true;
        }
        if(event.event == MorseFlipperSettingGpio) {
            scene_manager_next_scene(app->scene_manager, MorseFlipperSceneGpio);
            return true;
        }
    }

    return false;
}

void morse_flipper_scene_home_on_exit(void* context) {
    MorseFlipperApp* app = context;
    scene_manager_set_scene_state(
        app->scene_manager,
        MorseFlipperSceneHome,
        variable_item_list_get_selected_item_index(app->settings_list));
    variable_item_list_reset(app->settings_list);
}
