/*
 * Purpose: Handle GPIO pin settings and startup probe notices.
 * Owns: GPIO settings row callbacks, conflict text, and probe screen entry.
 * Depends on: morse_flipper_app_i.h plus GPIO and probe modules.
 * Tests: tests/test_gpio.c and tests/test_gpio_probe.c cover decisions.
 */

#include "morse_flipper_app_i.h"

static const uint8_t morse_flipper_gpio_ui_pins[] = {
    MorseFlipperGpioPinP3,
    MorseFlipperGpioPinP4,
    MorseFlipperGpioPinP5,
    MorseFlipperGpioPinP6,
    MorseFlipperGpioPinP7,
    MorseFlipperGpioPinP16,
};

static uint8_t morse_flipper_gpio_ui_pin(uint8_t idx) {
    if(idx >= COUNT_OF(morse_flipper_gpio_ui_pins)) return morse_flipper_gpio_default_dit();
    return morse_flipper_gpio_ui_pins[idx];
}

static uint8_t morse_flipper_gpio_pin_ui(uint8_t pin) {
    for(uint8_t i = 0U; i < COUNT_OF(morse_flipper_gpio_ui_pins); i++) {
        if(morse_flipper_gpio_ui_pins[i] == pin) return i;
    }

    return 0U;
}

void morse_flipper_settings_gpio_dit_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);

    app->gpio_edit_dit_idx = morse_flipper_gpio_ui_pin(idx);
    variable_item_set_current_value_text(item, morse_flipper_gpio_name(app->gpio_edit_dit_idx));
}

void morse_flipper_settings_gpio_dah_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);

    app->gpio_edit_dah_idx = morse_flipper_gpio_ui_pin(idx);
    variable_item_set_current_value_text(item, morse_flipper_gpio_name(app->gpio_edit_dah_idx));
}

static uint8_t morse_flipper_ground_choice_from_ui(uint8_t idx) {
    if(idx == 0U) {
        return MORSE_FLIPPER_GPIO_PIN_NONE;
    }

    return morse_flipper_gpio_ui_pin((uint8_t)(idx - 1U));
}

static uint8_t morse_flipper_ground_choice_to_ui(uint8_t idx) {
    if(idx == MORSE_FLIPPER_GPIO_PIN_NONE) {
        return 0U;
    }

    return (uint8_t)(morse_flipper_gpio_pin_ui(idx) + 1U);
}

void morse_flipper_settings_gpio_ground_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t idx = morse_flipper_ground_choice_from_ui(variable_item_get_current_value_index(item));

    app->gpio_edit_ground_idx = idx;
    variable_item_set_current_value_text(item, morse_flipper_gpio_name(app->gpio_edit_ground_idx));
}

static uint8_t morse_flipper_ptt_choice_from_ui(uint8_t idx) {
    return idx == 1U ? MorseFlipperGpioPinP16 : MORSE_FLIPPER_GPIO_PIN_NONE;
}

static uint8_t morse_flipper_ptt_choice_to_ui(uint8_t idx) {
    return idx == MorseFlipperGpioPinP16 ? 1U : 0U;
}

void morse_flipper_settings_gpio_ptt_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);

    app->gpio_edit_ptt_idx =
        morse_flipper_ptt_choice_from_ui(variable_item_get_current_value_index(item));
    variable_item_set_current_value_text(item, morse_flipper_gpio_name(app->gpio_edit_ptt_idx));
}

void morse_flipper_scene_gpio_on_enter(void* context) {
    MorseFlipperApp* app = context;
    VariableItem* item;
    uint8_t sel = scene_manager_get_scene_state(app->scene_manager, MorseFlipperSceneGpio);

    morse_flipper_scene_enter_now(app, MorseFlipperSceneGpio);
    variable_item_list_reset(app->settings_list);
    variable_item_list_set_enter_callback(
        app->settings_list, morse_flipper_settings_noop_enter, app);
    app->gpio_edit_dit_idx = app->gpio_dit_idx;
    app->gpio_edit_dah_idx = app->gpio_dah_idx;
    app->gpio_edit_ground_idx = app->gpio_ground_idx;
    app->gpio_edit_ptt_idx = app->gpio_ptt_idx;
    if(app->gpio_edit_dit_idx == MorseFlipperGpioPinP2 ||
       app->gpio_edit_dit_idx == MorseFlipperGpioPinP15)
        app->gpio_edit_dit_idx = morse_flipper_gpio_default_dit();
    if(app->gpio_edit_dah_idx == MorseFlipperGpioPinP2 ||
       app->gpio_edit_dah_idx == MorseFlipperGpioPinP15)
        app->gpio_edit_dah_idx = morse_flipper_gpio_default_dah();
    if(app->gpio_edit_ground_idx == MorseFlipperGpioPinP2 ||
       app->gpio_edit_ground_idx == MorseFlipperGpioPinP15)
        app->gpio_edit_ground_idx = morse_flipper_gpio_default_ground();

    item = variable_item_list_add(
        app->settings_list,
        "dit/SK",
        COUNT_OF(morse_flipper_gpio_ui_pins),
        morse_flipper_settings_gpio_dit_changed,
        app);
    variable_item_set_current_value_index(item, morse_flipper_gpio_pin_ui(app->gpio_edit_dit_idx));
    variable_item_set_current_value_text(item, morse_flipper_gpio_name(app->gpio_edit_dit_idx));

    item = variable_item_list_add(
        app->settings_list,
        "dah",
        COUNT_OF(morse_flipper_gpio_ui_pins),
        morse_flipper_settings_gpio_dah_changed,
        app);
    variable_item_set_current_value_index(item, morse_flipper_gpio_pin_ui(app->gpio_edit_dah_idx));
    variable_item_set_current_value_text(item, morse_flipper_gpio_name(app->gpio_edit_dah_idx));

    item = variable_item_list_add(
        app->settings_list,
        "Virtual gnd",
        COUNT_OF(morse_flipper_gpio_ui_pins) + 1U,
        morse_flipper_settings_gpio_ground_changed,
        app);
    variable_item_set_current_value_index(
        item, morse_flipper_ground_choice_to_ui(app->gpio_edit_ground_idx));
    variable_item_set_current_value_text(item, morse_flipper_gpio_name(app->gpio_edit_ground_idx));

    item = variable_item_list_add(
        app->settings_list, "PTT/TX", 2U, morse_flipper_settings_gpio_ptt_changed, app);
    variable_item_set_current_value_index(
        item, morse_flipper_ptt_choice_to_ui(app->gpio_edit_ptt_idx));
    variable_item_set_current_value_text(item, morse_flipper_gpio_name(app->gpio_edit_ptt_idx));

    if(sel > MorseFlipperGpioSettingPtt) sel = MorseFlipperGpioSettingDit;
    variable_item_list_set_selected_item(app->settings_list, sel);
}

bool morse_flipper_scene_gpio_on_event(void* context, SceneManagerEvent event) {
    MorseFlipperApp* app = context;
    MorseFlipperGpioRule rule = MorseFlipperGpioRuleOk;

    if(event.type == SceneManagerEventTypeBack) {
        if(!morse_flipper_gpio_try_apply(
               app,
               app->gpio_edit_dit_idx,
               app->gpio_edit_dah_idx,
               app->gpio_edit_ground_idx,
               app->gpio_edit_ptt_idx,
               &rule)) {
            morse_flipper_gpio_alert(app, rule);
            return true;
        }
        morse_flipper_scene_back(app);
        return true;
    }

    return false;
}

void morse_flipper_scene_gpio_on_exit(void* context) {
    MorseFlipperApp* app = context;
    scene_manager_set_scene_state(
        app->scene_manager,
        MorseFlipperSceneGpio,
        variable_item_list_get_selected_item_index(app->settings_list));
    variable_item_list_reset(app->settings_list);
}
