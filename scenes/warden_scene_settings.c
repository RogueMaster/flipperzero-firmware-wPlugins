#include "../warden_i.h"

typedef enum {
    SettingsIndexSound,
    SettingsIndexVibro,
    SettingsIndexLed,
} SettingsIndex;

static const char* const on_off[] = {"OFF", "ON"};

static void settings_sound_cb(VariableItem* item) {
    WardenApp* app = variable_item_get_context(item);
    uint8_t v = variable_item_get_current_value_index(item);
    app->sound = v;
    variable_item_set_current_value_text(item, on_off[v]);
}
static void settings_vibro_cb(VariableItem* item) {
    WardenApp* app = variable_item_get_context(item);
    uint8_t v = variable_item_get_current_value_index(item);
    app->vibro = v;
    variable_item_set_current_value_text(item, on_off[v]);
}
static void settings_led_cb(VariableItem* item) {
    WardenApp* app = variable_item_get_context(item);
    uint8_t v = variable_item_get_current_value_index(item);
    app->led = v;
    variable_item_set_current_value_text(item, on_off[v]);
}

void warden_scene_settings_on_enter(void* context) {
    WardenApp* app = context;
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

    view_dispatcher_switch_to_view(app->view_dispatcher, WardenViewSettings);
}

bool warden_scene_settings_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void warden_scene_settings_on_exit(void* context) {
    WardenApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
