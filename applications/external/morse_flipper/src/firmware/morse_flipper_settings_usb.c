/*
 * Purpose: Handle USB keyboard, mouse, and MIDI settings rows.
 * Owns: USB mode and key preset callbacks plus persistence side effects.
 * Depends on: morse_flipper_app_i.h, pc_keys.h, and USB MIDI/HID services.
 * Tests: tests/test_keyboard_presets.c covers preset data.
 */

#include "morse_flipper_app_i.h"

void morse_flipper_settings_usb_mode_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);

    if(idx > MorseFlipperPcModeMidi) idx = MorseFlipperPcModeOff;

    variable_item_set_current_value_text(item, morse_flipper_usb_mode_names[idx]);
    app->pc_mode_pref = idx;
    morse_flipper_save_config(app);
}

void morse_flipper_settings_usb_paddle_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t v = variable_item_get_current_value_index(item);

    app->pc_paddle_preset = v;
    variable_item_set_current_value_text(item, morse_pc_paddle_preset_name(v));
    morse_flipper_save_config(app);
}

void morse_flipper_settings_usb_straight_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t pick = variable_item_get_current_value_index(item);

    app->pc_straight_preset = pick;
    variable_item_set_current_value_text(item, morse_pc_straight_preset_name(pick));
    morse_flipper_save_config(app);
}

void morse_flipper_settings_usb_mouse_swap_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);

    app->mouse_invert = idx != 0U;
    variable_item_set_current_value_text(item, app->mouse_invert ? "Yes" : "No");
    morse_flipper_save_config(app);
}

void morse_flipper_scene_pc_on_enter(void* context) {
    MorseFlipperApp* app = context;
    VariableItem* it;
    uint8_t sel = scene_manager_get_scene_state(app->scene_manager, MorseFlipperScenePc);

    morse_flipper_ensure_view(app, MorseFlipperViewSettings);
    variable_item_list_reset(app->settings_list);
    variable_item_list_set_enter_callback(
        app->settings_list, morse_flipper_settings_noop_enter, app);

    it = variable_item_list_add(
        app->settings_list,
        "Connection",
        COUNT_OF(morse_flipper_usb_mode_names),
        morse_flipper_settings_usb_mode_changed,
        app);
    variable_item_set_current_value_index(it, app->pc_mode_pref);
    variable_item_set_current_value_text(it, morse_flipper_usb_mode_names[app->pc_mode_pref]);

    it = variable_item_list_add(
        app->settings_list,
        "Paddle keys",
        morse_pc_paddle_preset_count(),
        morse_flipper_settings_usb_paddle_changed,
        app);
    variable_item_set_current_value_index(it, app->pc_paddle_preset);
    variable_item_set_current_value_text(it, morse_pc_paddle_preset_name(app->pc_paddle_preset));

    it = variable_item_list_add(
        app->settings_list,
        "Straight key",
        morse_pc_straight_preset_count(),
        morse_flipper_settings_usb_straight_changed,
        app);
    variable_item_set_current_value_index(it, app->pc_straight_preset);
    variable_item_set_current_value_text(
        it, morse_pc_straight_preset_name(app->pc_straight_preset));

    it = variable_item_list_add(
        app->settings_list, "Invert mouse", 2U, morse_flipper_settings_usb_mouse_swap_changed, app);
    variable_item_set_current_value_index(it, app->mouse_invert ? 1U : 0U);
    variable_item_set_current_value_text(it, app->mouse_invert ? "Yes" : "No");

    if(sel > MorseFlipperUsbSettingMouseSwap) sel = MorseFlipperUsbSettingConnection;
    variable_item_list_set_selected_item(app->settings_list, sel);
    morse_flipper_scene_enter_now(app, MorseFlipperScenePc);
}

bool morse_flipper_scene_pc_on_event(void* context, SceneManagerEvent event) {
    MorseFlipperApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        morse_flipper_scene_back(app);
        return true;
    }

    return false;
}

void morse_flipper_scene_pc_on_exit(void* context) {
    MorseFlipperApp* app = context;
    scene_manager_set_scene_state(
        app->scene_manager,
        MorseFlipperScenePc,
        variable_item_list_get_selected_item_index(app->settings_list));
    variable_item_list_reset(app->settings_list);
}
