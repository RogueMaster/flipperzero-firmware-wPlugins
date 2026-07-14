#include "../guess_the_number.h"
#include "../helpers/game_storage.h"
#include "../helpers/game_haptic.h"
#include <lib/toolbox/value_index.h>

enum SettingsIndex {
    SettingsIndexHaptic = 10,
    SettingsIndexSpeaker,
    SettingsIndexLed,
    SettingsIndexSaveSettings,
    SettingsIndexResetScore,
};

const char* const haptic_text[2] = {
    "OFF",
    "ON",
};
const uint32_t haptic_value[2] = {
    GameHapticOff,
    GameHapticOn,
};

const char* const speaker_text[2] = {
    "OFF",
    "ON",
};
const uint32_t speaker_value[2] = {
    GameSpeakerOff,
    GameSpeakerOn,
};

const char* const led_text[2] = {
    "OFF",
    "ON",
};
const uint32_t led_value[2] = {
    GameLedOff,
    GameLedOn,
};

const char* const settings_text[2] = {
    "OFF",
    "ON",
};
const uint32_t settings_value[2] = {
    GameSettingsOff,
    GameSettingsOn,
};

static void game_scene_settings_set_haptic(VariableItem* item) {
    GameApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, haptic_text[index]);
    app->haptic = haptic_value[index];
}

static void game_scene_settings_set_speaker(VariableItem* item) {
    GameApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, speaker_text[index]);
    app->speaker = speaker_value[index];
}

static void game_scene_settings_set_led(VariableItem* item) {
    GameApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, led_text[index]);
    app->led = led_value[index];
}

static void game_scene_settings_set_save_settings(VariableItem* item) {
    GameApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, settings_text[index]);
    app->save_settings = settings_value[index];
}

static void game_scene_settings_enter_callback(void* context, uint32_t index) {
    GameApp* app = context;
    if(index == SettingsIndexResetScore) {
        game_clear_best_score();
        game_play_happy_bump(app);
    }
}

void game_scene_settings_on_enter(void* context) {
    GameApp* app = context;
    VariableItem* item;
    uint8_t value_index;

    item = variable_item_list_add(
        app->variable_item_list, "Vibro/Haptic:", 2, game_scene_settings_set_haptic, app);
    value_index = value_index_uint32(app->haptic, haptic_value, 2);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, haptic_text[value_index]);

    item = variable_item_list_add(
        app->variable_item_list, "Sound:", 2, game_scene_settings_set_speaker, app);
    value_index = value_index_uint32(app->speaker, speaker_value, 2);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, speaker_text[value_index]);

    item = variable_item_list_add(
        app->variable_item_list, "LED FX:", 2, game_scene_settings_set_led, app);
    value_index = value_index_uint32(app->led, led_value, 2);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, led_text[value_index]);

    item = variable_item_list_add(
        app->variable_item_list, "Save Settings:", 2, game_scene_settings_set_save_settings, app);
    value_index = value_index_uint32(app->save_settings, settings_value, 2);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, settings_text[value_index]);

    item = variable_item_list_add(app->variable_item_list, "Reset Best Score", 1, NULL, app);
    variable_item_set_current_value_text(item, "Press OK");

    variable_item_list_set_enter_callback(
        app->variable_item_list, game_scene_settings_enter_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, GameViewIdSettings);
}

bool game_scene_settings_on_event(void* context, SceneManagerEvent event) {
    GameApp* app = context;
    UNUSED(app);
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
    }
    return consumed;
}

void game_scene_settings_on_exit(void* context) {
    GameApp* app = context;
    variable_item_list_set_selected_item(app->variable_item_list, 0);
    variable_item_list_reset(app->variable_item_list);
}
