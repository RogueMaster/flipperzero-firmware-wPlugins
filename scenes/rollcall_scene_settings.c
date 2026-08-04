#include "../rollcall_i.h"

static const char* const on_off[] = {"OFF", "ON"};

static void settings_band_cb(VariableItem* item) {
    RollCallApp* app = variable_item_get_context(item);
    uint8_t v = variable_item_get_current_value_index(item);
    app->settings.band_idx = v;
    variable_item_set_current_value_text(item, rc_bands[v].label);
}
static void settings_mod_cb(VariableItem* item) {
    RollCallApp* app = variable_item_get_context(item);
    uint8_t v = variable_item_get_current_value_index(item);
    app->settings.mod_idx = v;
    variable_item_set_current_value_text(item, rc_mods[v].label);
}
static void settings_target_cb(VariableItem* item) {
    RollCallApp* app = variable_item_get_context(item);
    uint8_t v = variable_item_get_current_value_index(item);
    app->settings.target = v + RC_TARGET_MIN;
    char buf[4];
    snprintf(buf, sizeof(buf), "%d", app->settings.target);
    variable_item_set_current_value_text(item, buf);
}
static void settings_gap_cb(VariableItem* item) {
    RollCallApp* app = variable_item_get_context(item);
    uint8_t v = variable_item_get_current_value_index(item);
    app->settings.gap_idx = v;
    variable_item_set_current_value_text(item, rc_gaps[v].label);
}
static void settings_sound_cb(VariableItem* item) {
    RollCallApp* app = variable_item_get_context(item);
    uint8_t v = variable_item_get_current_value_index(item);
    app->settings.sound = v;
    variable_item_set_current_value_text(item, on_off[v]);
}
static void settings_vibro_cb(VariableItem* item) {
    RollCallApp* app = variable_item_get_context(item);
    uint8_t v = variable_item_get_current_value_index(item);
    app->settings.vibro = v;
    variable_item_set_current_value_text(item, on_off[v]);
}
static void settings_led_cb(VariableItem* item) {
    RollCallApp* app = variable_item_get_context(item);
    uint8_t v = variable_item_get_current_value_index(item);
    app->settings.led = v;
    variable_item_set_current_value_text(item, on_off[v]);
}

void rollcall_scene_settings_on_enter(void* context) {
    RollCallApp* app = context;
    VariableItemList* list = app->var_item_list;
    variable_item_list_reset(list);

    VariableItem* item;
    char buf[4];

    item = variable_item_list_add(list, "Band (MHz)", RC_BAND_COUNT, settings_band_cb, app);
    variable_item_set_current_value_index(item, app->settings.band_idx);
    variable_item_set_current_value_text(item, rc_bands[app->settings.band_idx].label);

    item = variable_item_list_add(list, "Modulation", RC_MOD_COUNT, settings_mod_cb, app);
    variable_item_set_current_value_index(item, app->settings.mod_idx);
    variable_item_set_current_value_text(item, rc_mods[app->settings.mod_idx].label);

    item = variable_item_list_add(list, "Presses", RC_TARGET_COUNT, settings_target_cb, app);
    variable_item_set_current_value_index(item, app->settings.target - RC_TARGET_MIN);
    snprintf(buf, sizeof(buf), "%d", app->settings.target);
    variable_item_set_current_value_text(item, buf);

    /* How long a quiet gap separates two presses. Raise it if one press is
     * being counted twice; lower it if quick presses are being merged. */
    item = variable_item_list_add(list, "Press gap", RC_GAP_COUNT, settings_gap_cb, app);
    variable_item_set_current_value_index(item, app->settings.gap_idx);
    variable_item_set_current_value_text(item, rc_gaps[app->settings.gap_idx].label);

    item = variable_item_list_add(list, "Sound", 2, settings_sound_cb, app);
    variable_item_set_current_value_index(item, app->settings.sound);
    variable_item_set_current_value_text(item, on_off[app->settings.sound]);

    item = variable_item_list_add(list, "Vibro", 2, settings_vibro_cb, app);
    variable_item_set_current_value_index(item, app->settings.vibro);
    variable_item_set_current_value_text(item, on_off[app->settings.vibro]);

    item = variable_item_list_add(list, "LED", 2, settings_led_cb, app);
    variable_item_set_current_value_index(item, app->settings.led);
    variable_item_set_current_value_text(item, on_off[app->settings.led]);

    view_dispatcher_switch_to_view(app->view_dispatcher, RollCallViewSettings);
}

bool rollcall_scene_settings_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void rollcall_scene_settings_on_exit(void* context) {
    RollCallApp* app = context;
    /* Persist on the way out - one write per visit, not one per keypress. */
    rc_settings_save(&app->settings);
    variable_item_list_reset(app->var_item_list);
}
