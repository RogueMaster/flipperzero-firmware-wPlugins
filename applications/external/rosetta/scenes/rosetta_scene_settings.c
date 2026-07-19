#include "../rosetta_i.h"

static const char* const on_off[] = {"OFF", "ON"};

static void set_sound(VariableItem* item) {
    RosettaApp* app = variable_item_get_context(item);
    uint8_t v = variable_item_get_current_value_index(item);
    app->sound = v;
    variable_item_set_current_value_text(item, on_off[v]);
}
static void set_vibro(VariableItem* item) {
    RosettaApp* app = variable_item_get_context(item);
    uint8_t v = variable_item_get_current_value_index(item);
    app->vibro = v;
    variable_item_set_current_value_text(item, on_off[v]);
}
static void set_led(VariableItem* item) {
    RosettaApp* app = variable_item_get_context(item);
    uint8_t v = variable_item_get_current_value_index(item);
    app->led = v;
    variable_item_set_current_value_text(item, on_off[v]);
}
static void set_freq(VariableItem* item) {
    RosettaApp* app = variable_item_get_context(item);
    uint8_t v = variable_item_get_current_value_index(item);
    app->rf_freq_index = v;
    variable_item_set_current_value_text(item, rf_scope_freq_label(v));
}

void rosetta_scene_settings_on_enter(void* context) {
    RosettaApp* app = context;
    VariableItemList* list = app->var_item_list;
    variable_item_list_reset(list);
    VariableItem* item;

    item = variable_item_list_add(list, "Sound", 2, set_sound, app);
    variable_item_set_current_value_index(item, app->sound);
    variable_item_set_current_value_text(item, on_off[app->sound]);

    item = variable_item_list_add(list, "Vibro", 2, set_vibro, app);
    variable_item_set_current_value_index(item, app->vibro);
    variable_item_set_current_value_text(item, on_off[app->vibro]);

    item = variable_item_list_add(list, "LED", 2, set_led, app);
    variable_item_set_current_value_index(item, app->led);
    variable_item_set_current_value_text(item, on_off[app->led]);

    item = variable_item_list_add(list, "RF Freq", RF_SCOPE_FREQ_COUNT, set_freq, app);
    variable_item_set_current_value_index(item, app->rf_freq_index);
    variable_item_set_current_value_text(item, rf_scope_freq_label(app->rf_freq_index));

    view_dispatcher_switch_to_view(app->view_dispatcher, RosettaViewSettings);
}

bool rosetta_scene_settings_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void rosetta_scene_settings_on_exit(void* context) {
    RosettaApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
