#include "../argus_i.h"

static const char* const chan_labels[] =
    {"Hop", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13"};
#define CHAN_COUNT 14

static const char* const sens_labels[] = {"High", "Medium", "Low"};
static const uint32_t sens_thresh[] = {3, 6, 12};
#define SENS_COUNT 3

static const char* const on_off[] = {"OFF", "ON"};

/* ---- exposed helpers ---- */
uint8_t argus_settings_channel(const ArgusSettings* s) {
    return s->channel_index % CHAN_COUNT;
}

const char* argus_settings_channel_label(uint8_t index) {
    return chan_labels[index % CHAN_COUNT];
}

uint32_t argus_settings_storm_threshold(const ArgusSettings* s) {
    return sens_thresh[s->sensitivity_index % SENS_COUNT];
}

const char* argus_settings_sensitivity_label(uint8_t index) {
    return sens_labels[index % SENS_COUNT];
}

/* ---- item callbacks ---- */
static void chan_changed(VariableItem* item) {
    ArgusApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->settings.channel_index = i;
    variable_item_set_current_value_text(item, chan_labels[i]);
}

static void sens_changed(VariableItem* item) {
    ArgusApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->settings.sensitivity_index = i;
    variable_item_set_current_value_text(item, sens_labels[i]);
}

static void sound_changed(VariableItem* item) {
    ArgusApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->settings.sound = i;
    variable_item_set_current_value_text(item, on_off[i]);
}

static void vibro_changed(VariableItem* item) {
    ArgusApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->settings.vibro = i;
    variable_item_set_current_value_text(item, on_off[i]);
}

static void led_changed(VariableItem* item) {
    ArgusApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->settings.led = i;
    variable_item_set_current_value_text(item, on_off[i]);
}

void argus_scene_settings_on_enter(void* context) {
    ArgusApp* app = context;
    VariableItemList* list = app->var_item_list;
    VariableItem* item;

    variable_item_list_reset(list);

    item = variable_item_list_add(list, "Wi-Fi channel", CHAN_COUNT, chan_changed, app);
    variable_item_set_current_value_index(item, app->settings.channel_index);
    variable_item_set_current_value_text(item, chan_labels[app->settings.channel_index]);

    item = variable_item_list_add(list, "Alarm sens.", SENS_COUNT, sens_changed, app);
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

    view_dispatcher_switch_to_view(app->view_dispatcher, ArgusViewSettings);
}

bool argus_scene_settings_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void argus_scene_settings_on_exit(void* context) {
    ArgusApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
