#include "../gauntlet_i.h"

static const char* const on_off[] = {"OFF", "ON"};
static const char* const reset_opt[] = {"Keep", "ERASE"};

static void settings_sound_cb(VariableItem* item) {
    GauntletApp* app = variable_item_get_context(item);
    uint8_t v = variable_item_get_current_value_index(item);
    app->sound = v;
    variable_item_set_current_value_text(item, on_off[v]);
}
static void settings_vibro_cb(VariableItem* item) {
    GauntletApp* app = variable_item_get_context(item);
    uint8_t v = variable_item_get_current_value_index(item);
    app->vibro = v;
    variable_item_set_current_value_text(item, on_off[v]);
}
static void settings_led_cb(VariableItem* item) {
    GauntletApp* app = variable_item_get_context(item);
    uint8_t v = variable_item_get_current_value_index(item);
    app->led = v;
    variable_item_set_current_value_text(item, on_off[v]);
}
static void settings_hints_cb(VariableItem* item) {
    GauntletApp* app = variable_item_get_context(item);
    uint8_t v = variable_item_get_current_value_index(item);
    app->hints_free = v;
    variable_item_set_current_value_text(item, on_off[v]);
}
static void settings_reset_cb(VariableItem* item) {
    GauntletApp* app = variable_item_get_context(item);
    uint8_t v = variable_item_get_current_value_index(item);
    if(v == 1) {
        ctf_progress_reset(app->progress);
        ctf_store_progress_save(app->storage, app->progress);
        gauntlet_notify_wrong(app); /* a short buzz to confirm */
        variable_item_set_current_value_index(item, 0);
        v = 0;
    }
    variable_item_set_current_value_text(item, reset_opt[v]);
}

void gauntlet_scene_settings_on_enter(void* context) {
    GauntletApp* app = context;
    VariableItemList* list = app->var_item_list;
    variable_item_list_reset(list);

    VariableItem* item;

    item = variable_item_list_add(list, "Sound", 2, settings_sound_cb, app);
    variable_item_set_current_value_index(item, app->sound);
    variable_item_set_current_value_text(item, on_off[app->sound]);

    item = variable_item_list_add(list, "Vibro", 2, settings_vibro_cb, app);
    variable_item_set_current_value_index(item, app->vibro);
    variable_item_set_current_value_text(item, on_off[app->vibro]);

    item = variable_item_list_add(list, "LED", 2, settings_led_cb, app);
    variable_item_set_current_value_index(item, app->led);
    variable_item_set_current_value_text(item, on_off[app->led]);

    item = variable_item_list_add(list, "Free hints", 2, settings_hints_cb, app);
    variable_item_set_current_value_index(item, app->hints_free);
    variable_item_set_current_value_text(item, on_off[app->hints_free]);

    item = variable_item_list_add(list, "Reset progress", 2, settings_reset_cb, app);
    variable_item_set_current_value_index(item, 0);
    variable_item_set_current_value_text(item, reset_opt[0]);

    view_dispatcher_switch_to_view(app->view_dispatcher, GauntletViewVarList);
}

bool gauntlet_scene_settings_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void gauntlet_scene_settings_on_exit(void* context) {
    GauntletApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
