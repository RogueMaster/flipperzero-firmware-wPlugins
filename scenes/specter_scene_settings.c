#include "../specter_i.h"

static const char* const sens_labels[] = {"High", "Medium", "Low"};
static const uint8_t sens_thresh[] = {0, 8, 20}; // duty-cycle noise floor (%)
#define SENS_COUNT 3

static const char* const on_off[] = {"OFF", "ON"};

/* ---- exposed helpers ---- */
uint8_t specter_settings_threshold(const SpecterSettings* s) {
    return sens_thresh[s->sensitivity_index % SENS_COUNT];
}

const char* specter_settings_sensitivity_label(uint8_t index) {
    return sens_labels[index % SENS_COUNT];
}

/* ---- item callbacks ---- */
static void sens_changed(VariableItem* item) {
    SpecterApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->settings.sensitivity_index = i;
    variable_item_set_current_value_text(item, sens_labels[i]);
}

static void sound_changed(VariableItem* item) {
    SpecterApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->settings.sound = i;
    variable_item_set_current_value_text(item, on_off[i]);
}

static void vibro_changed(VariableItem* item) {
    SpecterApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->settings.vibro = i;
    variable_item_set_current_value_text(item, on_off[i]);
}

static void led_changed(VariableItem* item) {
    SpecterApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->settings.led = i;
    variable_item_set_current_value_text(item, on_off[i]);
}

void specter_scene_settings_on_enter(void* context) {
    SpecterApp* app = context;
    VariableItemList* list = app->var_item_list;
    VariableItem* item;

    variable_item_list_reset(list);

    item = variable_item_list_add(list, "Sensitivity", SENS_COUNT, sens_changed, app);
    variable_item_set_current_value_index(item, app->settings.sensitivity_index);
    variable_item_set_current_value_text(item, sens_labels[app->settings.sensitivity_index]);

    item = variable_item_list_add(list, "Sound", 2, sound_changed, app);
    variable_item_set_current_value_index(item, app->settings.sound ? 1 : 0);
    variable_item_set_current_value_text(item, on_off[app->settings.sound ? 1 : 0]);

    item = variable_item_list_add(list, "Vibrate", 2, vibro_changed, app);
    variable_item_set_current_value_index(item, app->settings.vibro ? 1 : 0);
    variable_item_set_current_value_text(item, on_off[app->settings.vibro ? 1 : 0]);

    item = variable_item_list_add(list, "LED", 2, led_changed, app);
    variable_item_set_current_value_index(item, app->settings.led ? 1 : 0);
    variable_item_set_current_value_text(item, on_off[app->settings.led ? 1 : 0]);

    view_dispatcher_switch_to_view(app->view_dispatcher, SpecterViewSettings);
}

bool specter_scene_settings_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void specter_scene_settings_on_exit(void* context) {
    SpecterApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
