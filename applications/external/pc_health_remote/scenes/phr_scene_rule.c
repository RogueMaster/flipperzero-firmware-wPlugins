#include "../phr_app.h"
#include "../signals.h"
#include "phr_scene.h"
#include <stdio.h>

static const char* const signal_names[AlertSignalCount] = {
    "Vibro",
    "Sound",
    "Vib+Snd",
    "LED",
    "Off",
};

static void enabled_changed(VariableItem* item) {
    PhrApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->alerts.cfg[app->editing_rule].enabled = idx != 0;
    variable_item_set_current_value_text(item, idx ? "On" : "Off");
}

static void threshold_text(PhrApp* app, VariableItem* item, uint8_t idx) {
    const AlertRuleMeta* m = alerts_meta((AlertRuleId)app->editing_rule);
    snprintf(
        app->edit_txt, sizeof(app->edit_txt), "%u%s", (unsigned)(m->min + idx * m->step), m->unit);
    variable_item_set_current_value_text(item, app->edit_txt);
}

static void threshold_changed(VariableItem* item) {
    PhrApp* app = variable_item_get_context(item);
    const AlertRuleMeta* m = alerts_meta((AlertRuleId)app->editing_rule);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->alerts.cfg[app->editing_rule].threshold = (uint8_t)(m->min + idx * m->step);
    threshold_text(app, item, idx);
}

static void signal_changed(VariableItem* item) {
    PhrApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->alerts.cfg[app->editing_rule].signal = idx;
    variable_item_set_current_value_text(item, signal_names[idx]);
    // Preview the selected signal.
    phr_signal_play(app->notifications, (AlertSignal)idx, (AlertRuleId)app->editing_rule);
}

static void rule_enter_noop(void* context, uint32_t index) {
    UNUSED(context);
    UNUSED(index);
}

void phr_scene_rule_on_enter(void* context) {
    PhrApp* app = context;
    const AlertRuleId id = (AlertRuleId)app->editing_rule;
    const AlertRuleMeta* m = alerts_meta(id);
    const AlertRuleConfig* c = &app->alerts.cfg[id];

    variable_item_list_reset(app->list);
    // No list header in the SDK: the rule name is used as the label of the first row.
    VariableItem* item = variable_item_list_add(app->list, m->name, 2, enabled_changed, app);
    variable_item_set_current_value_index(item, c->enabled ? 1 : 0);
    variable_item_set_current_value_text(item, c->enabled ? "On" : "Off");

    uint8_t steps = (uint8_t)((m->max - m->min) / m->step + 1);
    item = variable_item_list_add(app->list, "Threshold", steps, threshold_changed, app);
    uint8_t cur = c->threshold > m->min ? (uint8_t)((c->threshold - m->min) / m->step) : 0;
    if(cur >= steps) cur = steps - 1;
    variable_item_set_current_value_index(item, cur);
    threshold_text(app, item, cur);

    item = variable_item_list_add(app->list, "Signal", AlertSignalCount, signal_changed, app);
    variable_item_set_current_value_index(item, c->signal);
    variable_item_set_current_value_text(item, signal_names[c->signal]);

    // The SDK furi_check()s a NULL enter callback, and the list still holds the settings
    // scene's callback, so install a no-op one for this screen.
    variable_item_list_set_enter_callback(app->list, rule_enter_noop, app);
    variable_item_list_set_selected_item(app->list, 0);
    view_dispatcher_switch_to_view(app->view_dispatcher, PhrViewList);
}

bool phr_scene_rule_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void phr_scene_rule_on_exit(void* context) {
    PhrApp* app = context;
    variable_item_list_reset(app->list);
    phr_settings_save(app->storage, &app->settings, &app->alerts);
}
