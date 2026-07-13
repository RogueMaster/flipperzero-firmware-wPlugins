#include "../period_tracker.h"
#include "../period_tracker_models.h"
#include "../period_tracker_csv.h"
#include <furi.h>

typedef enum {
    PinSettingsItemEnabled,
    PinSettingsItemSetPin,
} PinSettingsItem;

// Custom events (offset to avoid clashing with item indexes from enter callback)
#define PIN_SETTINGS_EVENT_TOGGLE_OFF 100
#define PIN_SETTINGS_EVENT_TOGGLE_ON  101

static const char* const pin_enabled_names[] = {"OFF", "ON"};

// IMPORTANT: no scene transitions or list resets here - this callback runs
// inside the VariableItemList input handler. Doing scene work here frees the
// list under the widget's feet and hard-faults the Flipper. Queue an event
// instead and do the work in on_event.
static void pin_setting_change_callback(VariableItem* item) {
    PeriodTrackerApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, pin_enabled_names[index]);
    view_dispatcher_send_custom_event(
        app->view_dispatcher,
        index == 1 ? PIN_SETTINGS_EVENT_TOGGLE_ON : PIN_SETTINGS_EVENT_TOGGLE_OFF);
}

static void pin_settings_list_callback(void* context, uint32_t index) {
    PeriodTrackerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void period_tracker_scene_pin_settings_on_enter(void* context) {
    PeriodTrackerApp* app = context;
    furi_assert(app);
    VariableItemList* variable_item_list = app->variable_item_list;
    VariableItem* item;

    variable_item_list_reset(variable_item_list);
    variable_item_list_set_enter_callback(variable_item_list, pin_settings_list_callback, app);

    // Load current settings
    AppSettings settings;
    settings_load(app->storage, &settings);

    // PIN Protection toggle
    item = variable_item_list_add(
        variable_item_list, "PIN Protection", 2, pin_setting_change_callback, app);
    variable_item_set_current_value_index(item, settings.pin_enabled ? 1 : 0);
    variable_item_set_current_value_text(item, pin_enabled_names[settings.pin_enabled ? 1 : 0]);

    // Set PIN button (only show if PIN protection is enabled)
    if(settings.pin_enabled) {
        variable_item_list_add(variable_item_list, "Set New PIN", 0, NULL, app);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewVariableItemList);
}

bool period_tracker_scene_pin_settings_on_event(void* context, SceneManagerEvent event) {
    PeriodTrackerApp* app = context;
    furi_assert(app);
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == PinSettingsItemSetPin) {
            // Navigate to PIN setup scene
            scene_manager_next_scene(app->scene_manager, PeriodTrackerScenePinSetup);
            consumed = true;
        } else if(
            event.event == PIN_SETTINGS_EVENT_TOGGLE_ON ||
            event.event == PIN_SETTINGS_EVENT_TOGGLE_OFF) {
            bool enable = (event.event == PIN_SETTINGS_EVENT_TOGGLE_ON);

            AppSettings settings;
            settings_load(app->storage, &settings);

            if(enable && !period_tracker_load_pin(app)) {
                // Enabling but no PIN exists yet - go set one first.
                // PIN setup enables the setting itself on success.
                scene_manager_next_scene(app->scene_manager, PeriodTrackerScenePinSetup);
            } else {
                settings.pin_enabled = enable;
                settings_save(app->storage, &settings);
                // Rebuild the list to show/hide the "Set New PIN" item
                period_tracker_scene_pin_settings_on_enter(app);
            }
            consumed = true;
        }
    }

    return consumed;
}

void period_tracker_scene_pin_settings_on_exit(void* context) {
    PeriodTrackerApp* app = context;
    furi_assert(app);
    variable_item_list_reset(app->variable_item_list);
    widget_reset(app->widget);
}
