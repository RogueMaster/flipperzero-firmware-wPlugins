#include "../ear_trainer_i.h"
#include "ear_scene.h"

static const char* const on_off[2] = {"Off", "On"};
static const char* const note_lengths[3] = {"Short", "Medium", "Long"};
static const char* const root_modes[2] = {"Fixed C4", "Random"};

static void note_ms_changed(VariableItem* item) {
    EarTrainerApp* app = variable_item_get_context(item);
    app->settings.note_ms = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, note_lengths[app->settings.note_ms]);
    ear_settings_save(&app->settings);
}

static void root_changed(VariableItem* item) {
    EarTrainerApp* app = variable_item_get_context(item);
    app->settings.random_root = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, root_modes[app->settings.random_root]);
    ear_settings_save(&app->settings);
}

static void mnemonic_changed(VariableItem* item) {
    EarTrainerApp* app = variable_item_get_context(item);
    app->settings.show_mnemonic = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, on_off[app->settings.show_mnemonic]);
    ear_settings_save(&app->settings);
}

static void vibro_changed(VariableItem* item) {
    EarTrainerApp* app = variable_item_get_context(item);
    app->settings.vibro = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, on_off[app->settings.vibro]);
    ear_settings_save(&app->settings);
}

static void led_changed(VariableItem* item) {
    EarTrainerApp* app = variable_item_get_context(item);
    app->settings.led = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, on_off[app->settings.led]);
    ear_settings_save(&app->settings);
}

void ear_scene_settings_on_enter(void* context) {
    EarTrainerApp* app = context;
    VariableItemList* list = app->var_item_list;
    VariableItem* item;

    variable_item_list_reset(list);

    item = variable_item_list_add(list, "Note length", 3, note_ms_changed, app);
    variable_item_set_current_value_index(item, app->settings.note_ms);
    variable_item_set_current_value_text(item, note_lengths[app->settings.note_ms]);

    item = variable_item_list_add(list, "Root note", 2, root_changed, app);
    variable_item_set_current_value_index(item, app->settings.random_root);
    variable_item_set_current_value_text(item, root_modes[app->settings.random_root]);

    item = variable_item_list_add(list, "Tune hints", 2, mnemonic_changed, app);
    variable_item_set_current_value_index(item, app->settings.show_mnemonic);
    variable_item_set_current_value_text(item, on_off[app->settings.show_mnemonic]);

    item = variable_item_list_add(list, "Vibro on miss", 2, vibro_changed, app);
    variable_item_set_current_value_index(item, app->settings.vibro);
    variable_item_set_current_value_text(item, on_off[app->settings.vibro]);

    item = variable_item_list_add(list, "LED feedback", 2, led_changed, app);
    variable_item_set_current_value_index(item, app->settings.led);
    variable_item_set_current_value_text(item, on_off[app->settings.led]);

    view_dispatcher_switch_to_view(app->view_dispatcher, EarViewVarItemList);
}

bool ear_scene_settings_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void ear_scene_settings_on_exit(void* context) {
    EarTrainerApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
