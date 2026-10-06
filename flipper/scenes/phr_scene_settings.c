#include "../phr_app.h"
#include "phr_scene.h"
#include <stdio.h>

static const char* const transport_names[PhrTransportCount] = {"BLE", "USB"};

static void transport_changed(VariableItem* item) {
    PhrApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->settings.transport = idx;
    variable_item_set_current_value_text(item, transport_names[idx]);
}

// Custom event ids for "open rule N" (kept clear of the app-wide PhrEvent values).
#define PHR_EVENT_RULE_BASE 200U

static void enter_cb(void* context, uint32_t index) {
    PhrApp* app = context;
    if(index == 0) return;
    // This callback runs while the list's view model is locked, so we must not switch
    // scenes here (the scene exit resets this same list -> lock re-entry -> crash).
    // Defer the navigation to the scene's event handler instead.
    view_dispatcher_send_custom_event(app->view_dispatcher, PHR_EVENT_RULE_BASE + index);
}

void phr_scene_settings_on_enter(void* context) {
    PhrApp* app = context;
    variable_item_list_reset(app->list);
    VariableItem* item =
        variable_item_list_add(app->list, "Transport", PhrTransportCount, transport_changed, app);
    variable_item_set_current_value_index(item, app->settings.transport);
    variable_item_set_current_value_text(item, transport_names[app->settings.transport]);

    for(int i = 0; i < AlertRuleCount; i++) {
        const AlertRuleMeta* m = alerts_meta((AlertRuleId)i);
        const AlertRuleConfig* c = &app->alerts.cfg[i];
        VariableItem* it = variable_item_list_add(app->list, m->name, 1, NULL, app);
        if(c->enabled)
            snprintf(app->rule_txt[i], sizeof(app->rule_txt[i]), "%u%s", c->threshold, m->unit);
        else
            snprintf(app->rule_txt[i], sizeof(app->rule_txt[i]), "off");
        variable_item_set_current_value_index(it, 0);
        variable_item_set_current_value_text(it, app->rule_txt[i]);
    }

    variable_item_list_set_enter_callback(app->list, enter_cb, app);
    variable_item_list_set_selected_item(
        app->list, scene_manager_get_scene_state(app->scene_manager, PhrSceneSettings));
    view_dispatcher_switch_to_view(app->view_dispatcher, PhrViewList);
}

bool phr_scene_settings_on_event(void* context, SceneManagerEvent event) {
    PhrApp* app = context;
    if(event.type == SceneManagerEventTypeCustom && event.event > PHR_EVENT_RULE_BASE &&
       event.event <= PHR_EVENT_RULE_BASE + AlertRuleCount) {
        uint32_t index = event.event - PHR_EVENT_RULE_BASE;
        app->editing_rule = (uint8_t)(index - 1);
        scene_manager_set_scene_state(app->scene_manager, PhrSceneSettings, index);
        scene_manager_next_scene(app->scene_manager, PhrSceneRule);
        return true;
    }
    return false;
}

void phr_scene_settings_on_exit(void* context) {
    PhrApp* app = context;
    // Runs both when opening a rule and when going back to the dashboard. The selected
    // row is kept in the scene state, so returning from a rule restores the position.
    variable_item_list_reset(app->list);
    phr_settings_save(app->storage, &app->settings, &app->alerts);
    if(app->transport_api != phr_transport_by_id(app->settings.transport)) {
        phr_app_apply_transport(app);
    }
}
