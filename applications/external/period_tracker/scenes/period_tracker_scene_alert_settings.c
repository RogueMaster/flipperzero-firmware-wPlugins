#include "../period_tracker.h"
#include "../period_tracker_models.h"
#include "../period_tracker_csv.h"

typedef enum {
    AlertSettingsItemLeadDays,
    AlertSettingsItemSave,
} AlertSettingsItem;

// Temporary storage for edited values
static AppSettings temp_settings;

static const char* const alert_lead_days_names[] = {"1", "2", "3", "4", "5", "6", "7"};

static void alert_lead_days_change_callback(VariableItem* item) {
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, alert_lead_days_names[index]);
    temp_settings.alert_lead_days = 1 + index;
}

static void alert_settings_list_callback(void* context, uint32_t index) {
    PeriodTrackerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void period_tracker_scene_alert_settings_on_enter(void* context) {
    PeriodTrackerApp* app = context;
    furi_assert(app);
    VariableItemList* variable_item_list = app->variable_item_list;
    VariableItem* item;

    variable_item_list_reset(variable_item_list);
    variable_item_list_set_enter_callback(variable_item_list, alert_settings_list_callback, app);

    // Load current settings
    app_settings_init(&temp_settings);
    if(!settings_load(app->storage, &temp_settings)) {
        // If loading failed, use defaults
        app_settings_init(&temp_settings);
    }

    // Alert Lead Days (1-7 days)
    item = variable_item_list_add(
        variable_item_list,
        "Alert Lead Time",
        7, // Count of values (1-7 days)
        alert_lead_days_change_callback,
        NULL);
    uint8_t lead_days_index = temp_settings.alert_lead_days - 1; // Convert to 0-based index
    if(lead_days_index > 6) lead_days_index = 0; // Safety check
    variable_item_set_current_value_index(item, lead_days_index);
    variable_item_set_current_value_text(item, alert_lead_days_names[lead_days_index]);

    // Save button
    variable_item_list_add(variable_item_list, "Save", 0, NULL, NULL);

    // Reset selection: list is shared with other scenes that may leave a
    // higher selected index, which would show a blank screen until input.
    variable_item_list_set_selected_item(variable_item_list, 0);

    view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewVariableItemList);
}

bool period_tracker_scene_alert_settings_on_event(void* context, SceneManagerEvent event) {
    PeriodTrackerApp* app = context;
    furi_assert(app);
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == AlertSettingsItemSave) {
            // Save the settings
            if(settings_save(app->storage, &temp_settings)) {
                FURI_LOG_I(
                    TAG, "Alert settings saved: lead_days=%u", temp_settings.alert_lead_days);
                // Return to settings menu
                scene_manager_previous_scene(app->scene_manager);
            } else {
                FURI_LOG_E(TAG, "Failed to save alert settings");
            }
            consumed = true;
        }
    }

    return consumed;
}

void period_tracker_scene_alert_settings_on_exit(void* context) {
    PeriodTrackerApp* app = context;
    furi_assert(app);
    variable_item_list_reset(app->variable_item_list);
}
