#include "../specter_i.h"
#include <stdio.h>

/* The label tables and the threshold maths live in helpers/specter_settings.c so
 * that the persisted file and the on-screen list can never drift apart. */

static const char* const on_off[] = {"OFF", "ON"};

typedef enum {
    SettingsIndexSensitivity,
    SettingsIndexSurvey,
    SettingsIndexSound,
    SettingsIndexVibro,
    SettingsIndexLed,
    SettingsIndexStealth,
    SettingsIndexLogging,
    SettingsIndexMeter,
    SettingsIndexClearLog,
} SettingsIndex;

/* Every change writes through immediately: a sweep kit that forgets its setup
 * because the app was closed the wrong way is worse than no persistence. */
static void settings_commit(SpecterApp* app) {
    specter_apply_threshold(app);
    specter_settings_save(&app->settings);
}

static void sens_changed(VariableItem* item) {
    SpecterApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->settings.sensitivity_index = i;
    variable_item_set_current_value_text(item, specter_settings_sensitivity_label(i));
    settings_commit(app);
}

static void survey_changed(VariableItem* item) {
    SpecterApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->settings.survey_index = i;
    variable_item_set_current_value_text(item, specter_settings_survey_label(i));
    settings_commit(app);
}

static void sound_changed(VariableItem* item) {
    SpecterApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->settings.sound = i;
    variable_item_set_current_value_text(item, on_off[i]);
    settings_commit(app);
}

static void vibro_changed(VariableItem* item) {
    SpecterApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->settings.vibro = i;
    variable_item_set_current_value_text(item, on_off[i]);
    settings_commit(app);
}

static void led_changed(VariableItem* item) {
    SpecterApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->settings.led = i;
    variable_item_set_current_value_text(item, on_off[i]);
    settings_commit(app);
}

static void stealth_changed(VariableItem* item) {
    SpecterApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->settings.stealth = i;
    variable_item_set_current_value_text(item, on_off[i]);
    settings_commit(app);
}

static void logging_changed(VariableItem* item) {
    SpecterApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->settings.logging = i;
    variable_item_set_current_value_text(item, on_off[i]);
    settings_commit(app);
}

/* Boosted maps the real polling band onto the whole dial; Raw shows the
 * unscaled carrier duty-cycle, which tops out around 30% on a live reader. */
static const char* const meter_labels[] = {"Boost", "Raw"};

static void meter_changed(VariableItem* item) {
    SpecterApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    app->settings.meter_raw = i;
    variable_item_set_current_value_text(item, meter_labels[i]);
    settings_commit(app);
}

static void settings_enter_cb(void* context, uint32_t index) {
    SpecterApp* app = context;
    if(index == SettingsIndexClearLog) {
        scene_manager_next_scene(app->scene_manager, SpecterSceneLogClear);
    }
}

void specter_scene_settings_on_enter(void* context) {
    SpecterApp* app = context;
    VariableItemList* list = app->var_item_list;
    VariableItem* item;
    char buf[24];

    variable_item_list_reset(list);

    item = variable_item_list_add(list, "Sensitivity", SPECTER_SENS_COUNT, sens_changed, app);
    variable_item_set_current_value_index(item, app->settings.sensitivity_index);
    variable_item_set_current_value_text(
        item, specter_settings_sensitivity_label(app->settings.sensitivity_index));

    item = variable_item_list_add(list, "Survey time", SPECTER_SURVEY_COUNT, survey_changed, app);
    variable_item_set_current_value_index(item, app->settings.survey_index);
    variable_item_set_current_value_text(
        item, specter_settings_survey_label(app->settings.survey_index));

    item = variable_item_list_add(list, "Sound", 2, sound_changed, app);
    variable_item_set_current_value_index(item, app->settings.sound ? 1 : 0);
    variable_item_set_current_value_text(item, on_off[app->settings.sound ? 1 : 0]);

    item = variable_item_list_add(list, "Vibrate", 2, vibro_changed, app);
    variable_item_set_current_value_index(item, app->settings.vibro ? 1 : 0);
    variable_item_set_current_value_text(item, on_off[app->settings.vibro ? 1 : 0]);

    item = variable_item_list_add(list, "LED", 2, led_changed, app);
    variable_item_set_current_value_index(item, app->settings.led ? 1 : 0);
    variable_item_set_current_value_text(item, on_off[app->settings.led ? 1 : 0]);

    item = variable_item_list_add(list, "Stealth", 2, stealth_changed, app);
    variable_item_set_current_value_index(item, app->settings.stealth ? 1 : 0);
    variable_item_set_current_value_text(item, on_off[app->settings.stealth ? 1 : 0]);

    item = variable_item_list_add(list, "Logging", 2, logging_changed, app);
    variable_item_set_current_value_index(item, app->settings.logging ? 1 : 0);
    variable_item_set_current_value_text(item, on_off[app->settings.logging ? 1 : 0]);

    item = variable_item_list_add(list, "Meter", 2, meter_changed, app);
    variable_item_set_current_value_index(item, app->settings.meter_raw ? 1 : 0);
    variable_item_set_current_value_text(item, meter_labels[app->settings.meter_raw ? 1 : 0]);

    /* Not a toggle - selecting it goes to the confirmation screen. */
    item = variable_item_list_add(list, "Clear logbook", 1, NULL, app);
    uint32_t size = specter_log_size();
    if(size >= 1024u) {
        snprintf(buf, sizeof(buf), "%luk", (unsigned long)(size / 1024u));
    } else {
        snprintf(buf, sizeof(buf), "%lub", (unsigned long)size);
    }
    variable_item_set_current_value_text(item, buf);

    variable_item_list_set_enter_callback(list, settings_enter_cb, app);
    variable_item_list_set_selected_item(
        list, scene_manager_get_scene_state(app->scene_manager, SpecterSceneSettings));

    view_dispatcher_switch_to_view(app->view_dispatcher, SpecterViewSettings);
}

bool specter_scene_settings_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void specter_scene_settings_on_exit(void* context) {
    SpecterApp* app = context;
    scene_manager_set_scene_state(
        app->scene_manager,
        SpecterSceneSettings,
        variable_item_list_get_selected_item_index(app->var_item_list));
    variable_item_list_reset(app->var_item_list);
}
