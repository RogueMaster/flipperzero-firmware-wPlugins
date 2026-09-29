#include "../ghosttag_i.h"

static const char* const on_off[] = {"OFF", "ON"};

typedef enum {
    SettingsRowRange,
    SettingsRowDwell,
    SettingsRowSound,
    SettingsRowVibro,
    SettingsRowLed,
    SettingsRowBacklight,
    SettingsRowLog,
    SettingsRowDefaults,
    SettingsRowCount,
} SettingsRow;

static void sens_changed(VariableItem* item) {
    GhostTagApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->settings.sensitivity_index = i;
    variable_item_set_current_value_text(item, ghosttag_settings_sensitivity_label(i));
}

static void follow_changed(VariableItem* item) {
    GhostTagApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->settings.follow_index = i;
    variable_item_set_current_value_text(item, ghosttag_settings_follow_label(i));
}

static void sound_changed(VariableItem* item) {
    GhostTagApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->settings.sound = !!i;
    variable_item_set_current_value_text(item, on_off[i & 1]);
}

static void vibro_changed(VariableItem* item) {
    GhostTagApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->settings.vibro = !!i;
    variable_item_set_current_value_text(item, on_off[i & 1]);
}

static void led_changed(VariableItem* item) {
    GhostTagApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->settings.led = !!i;
    variable_item_set_current_value_text(item, on_off[i & 1]);
}

static void backlight_changed(VariableItem* item) {
    GhostTagApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->settings.keep_lit = !!i;
    variable_item_set_current_value_text(item, on_off[i & 1]);
    ghosttag_backlight_hold(app, ghosttag_is_hunting(app) && app->settings.keep_lit);
}

static void log_changed(VariableItem* item) {
    GhostTagApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->settings.log_session = !!i;
    variable_item_set_current_value_text(item, on_off[i & 1]);
}

static void ghosttag_scene_settings_build(GhostTagApp* app);

/* The variable item list consumes OK on every row, and before v2.0 nothing
 * was listening - so OK on the settings screen was a key that visibly did
 * nothing. The last row now gives it a job. */
static void ghosttag_scene_settings_enter(void* context, uint32_t index) {
    GhostTagApp* app = context;
    if(index != SettingsRowDefaults) return;
    ghosttag_settings_defaults(&app->settings);
    ghosttag_settings_save(&app->settings);
    ghosttag_backlight_hold(app, ghosttag_is_hunting(app) && app->settings.keep_lit);
    ghosttag_scene_settings_build(app);
    variable_item_list_set_selected_item(app->var_item_list, SettingsRowDefaults);
    notification_message(app->notifications, &sequence_semi_success);
}

static void ghosttag_scene_settings_build(GhostTagApp* app) {
    VariableItemList* list = app->var_item_list;
    VariableItem* item;
    GhostTagSettings* s = &app->settings;

    variable_item_list_reset(list);

    item = variable_item_list_add(list, "Range", GHOSTTAG_SENS_COUNT, sens_changed, app);
    variable_item_set_current_value_index(item, s->sensitivity_index);
    variable_item_set_current_value_text(
        item, ghosttag_settings_sensitivity_label(s->sensitivity_index));

    item = variable_item_list_add(list, "Alert after", GHOSTTAG_FOLLOW_COUNT, follow_changed, app);
    variable_item_set_current_value_index(item, s->follow_index);
    variable_item_set_current_value_text(item, ghosttag_settings_follow_label(s->follow_index));

    item = variable_item_list_add(list, "Sound", 2, sound_changed, app);
    variable_item_set_current_value_index(item, s->sound ? 1 : 0);
    variable_item_set_current_value_text(item, on_off[s->sound ? 1 : 0]);

    item = variable_item_list_add(list, "Vibrate", 2, vibro_changed, app);
    variable_item_set_current_value_index(item, s->vibro ? 1 : 0);
    variable_item_set_current_value_text(item, on_off[s->vibro ? 1 : 0]);

    item = variable_item_list_add(list, "LED", 2, led_changed, app);
    variable_item_set_current_value_index(item, s->led ? 1 : 0);
    variable_item_set_current_value_text(item, on_off[s->led ? 1 : 0]);

    item = variable_item_list_add(list, "Screen on", 2, backlight_changed, app);
    variable_item_set_current_value_index(item, s->keep_lit ? 1 : 0);
    variable_item_set_current_value_text(item, on_off[s->keep_lit ? 1 : 0]);

    item = variable_item_list_add(list, "Log to SD", 2, log_changed, app);
    variable_item_set_current_value_index(item, s->log_session ? 1 : 0);
    variable_item_set_current_value_text(item, on_off[s->log_session ? 1 : 0]);

    /* A zero-value row draws as a plain entry; OK is what activates it. */
    variable_item_list_add(list, "Restore defaults", 0, NULL, NULL);
}

void ghosttag_scene_settings_on_enter(void* context) {
    GhostTagApp* app = context;
    ghosttag_scene_settings_build(app);
    variable_item_list_set_enter_callback(app->var_item_list, ghosttag_scene_settings_enter, app);
    variable_item_list_set_selected_item(
        app->var_item_list,
        scene_manager_get_scene_state(app->scene_manager, GhostTagSceneSettings));
    view_dispatcher_switch_to_view(app->view_dispatcher, GhostTagViewSettings);
}

bool ghosttag_scene_settings_on_event(void* context, SceneManagerEvent event) {
    GhostTagApp* app = context;
    /* An alert has to reach the user even from in here. Somebody checking
     * whether vibration is on is exactly the person who needs to be told. */
    if(event.type == SceneManagerEventTypeTick) {
        ghosttag_poll_alert(app);
        return true;
    }
    return false;
}

void ghosttag_scene_settings_on_exit(void* context) {
    GhostTagApp* app = context;
    uint32_t row = variable_item_list_get_selected_item_index(app->var_item_list);
    if(row >= SettingsRowCount) row = 0;
    scene_manager_set_scene_state(app->scene_manager, GhostTagSceneSettings, row);

    /* Persist on the way out, so the choice survives the app closing. */
    ghosttag_settings_save(&app->settings);
    variable_item_list_set_enter_callback(app->var_item_list, NULL, NULL);
    variable_item_list_reset(app->var_item_list);
}
