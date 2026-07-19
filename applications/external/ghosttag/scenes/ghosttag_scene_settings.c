#include "../ghosttag_i.h"

static const char* const sens_labels[] = {"Near", "Medium", "Far"};
static const int8_t sens_cutoffs[] = {-60, -75, -92};
static const char* const follow_labels[] = {"1 min", "3 min", "5 min", "10 min"};
static const uint32_t follow_minutes[] = {1, 3, 5, 10};
static const char* const on_off[] = {"OFF", "ON"};

#define SENS_COUNT 3
#define FOLLOW_COUNT 4

int8_t ghosttag_settings_rssi_cutoff(const GhostTagSettings* s) {
    return sens_cutoffs[s->sensitivity_index % SENS_COUNT];
}

uint32_t ghosttag_settings_follow_ms(const GhostTagSettings* s) {
    return follow_minutes[s->follow_index % FOLLOW_COUNT] * 60UL * 1000UL;
}

const char* ghosttag_settings_sensitivity_label(uint8_t index) {
    return sens_labels[index % SENS_COUNT];
}

const char* ghosttag_settings_follow_label(uint8_t index) {
    return follow_labels[index % FOLLOW_COUNT];
}

static void sens_changed(VariableItem* item) {
    GhostTagApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->settings.sensitivity_index = i;
    variable_item_set_current_value_text(item, sens_labels[i]);
}

static void follow_changed(VariableItem* item) {
    GhostTagApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->settings.follow_index = i;
    variable_item_set_current_value_text(item, follow_labels[i]);
}

static void sound_changed(VariableItem* item) {
    GhostTagApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->settings.sound = i;
    variable_item_set_current_value_text(item, on_off[i]);
}

static void vibro_changed(VariableItem* item) {
    GhostTagApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->settings.vibro = i;
    variable_item_set_current_value_text(item, on_off[i]);
}

static void led_changed(VariableItem* item) {
    GhostTagApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->settings.led = i;
    variable_item_set_current_value_text(item, on_off[i]);
}

void ghosttag_scene_settings_on_enter(void* context) {
    GhostTagApp* app = context;
    VariableItemList* list = app->var_item_list;
    VariableItem* item;

    variable_item_list_reset(list);

    item = variable_item_list_add(list, "Range", SENS_COUNT, sens_changed, app);
    variable_item_set_current_value_index(item, app->settings.sensitivity_index);
    variable_item_set_current_value_text(item, sens_labels[app->settings.sensitivity_index]);

    item = variable_item_list_add(list, "Alert after", FOLLOW_COUNT, follow_changed, app);
    variable_item_set_current_value_index(item, app->settings.follow_index);
    variable_item_set_current_value_text(item, follow_labels[app->settings.follow_index]);

    item = variable_item_list_add(list, "Sound", 2, sound_changed, app);
    variable_item_set_current_value_index(item, app->settings.sound ? 1 : 0);
    variable_item_set_current_value_text(item, on_off[app->settings.sound ? 1 : 0]);

    item = variable_item_list_add(list, "Vibrate", 2, vibro_changed, app);
    variable_item_set_current_value_index(item, app->settings.vibro ? 1 : 0);
    variable_item_set_current_value_text(item, on_off[app->settings.vibro ? 1 : 0]);

    item = variable_item_list_add(list, "LED", 2, led_changed, app);
    variable_item_set_current_value_index(item, app->settings.led ? 1 : 0);
    variable_item_set_current_value_text(item, on_off[app->settings.led ? 1 : 0]);

    view_dispatcher_switch_to_view(app->view_dispatcher, GhostTagViewSettings);
}

bool ghosttag_scene_settings_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void ghosttag_scene_settings_on_exit(void* context) {
    GhostTagApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
