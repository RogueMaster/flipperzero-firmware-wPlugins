#include "../period_tracker.h"
#include "../period_tracker_models.h"
#include "../period_tracker_csv.h"

typedef enum {
    SettingsMenuAddGirl,
    SettingsMenuSymptoms,
    SettingsMenuAlertLeadTime,
    SettingsMenuPin,
} SettingsMenuItem;

// Scene state: which view is currently shown within this scene
#define SETTINGS_STATE_MENU     0
#define SETTINGS_STATE_SUB_VIEW 1

static void period_tracker_scene_settings_submenu_callback(void* context, uint32_t index) {
    PeriodTrackerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void period_tracker_scene_settings_on_enter(void* context) {
    PeriodTrackerApp* app = context;
    furi_assert(app);

    scene_manager_set_scene_state(
        app->scene_manager, PeriodTrackerSceneSettings, SETTINGS_STATE_MENU);

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Settings");

    submenu_add_item(
        app->submenu,
        "Add New Profile",
        SettingsMenuAddGirl,
        period_tracker_scene_settings_submenu_callback,
        app);

    submenu_add_item(
        app->submenu,
        "Manage Symptoms",
        SettingsMenuSymptoms,
        period_tracker_scene_settings_submenu_callback,
        app);

    submenu_add_item(
        app->submenu,
        "Alert Lead Time",
        SettingsMenuAlertLeadTime,
        period_tracker_scene_settings_submenu_callback,
        app);

    submenu_add_item(
        app->submenu,
        "PIN Settings",
        SettingsMenuPin,
        period_tracker_scene_settings_submenu_callback,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewSubmenu);
}

bool period_tracker_scene_settings_on_event(void* context, SceneManagerEvent event) {
    PeriodTrackerApp* app = context;
    furi_assert(app);
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        if(scene_manager_get_scene_state(app->scene_manager, PeriodTrackerSceneSettings) ==
           SETTINGS_STATE_SUB_VIEW) {
            // Back from the symptoms list returns to the settings menu
            scene_manager_set_scene_state(
                app->scene_manager, PeriodTrackerSceneSettings, SETTINGS_STATE_MENU);
            view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewSubmenu);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case SettingsMenuAddGirl:
            // Navigate to Add Girl scene
            scene_manager_next_scene(app->scene_manager, PeriodTrackerSceneAddGirl);
            consumed = true;
            break;

        case SettingsMenuSymptoms:
            // Show current symptoms list in a scrollable text box
            {
                AppSettings settings;
                settings_load(app->storage, &settings);

                furi_string_reset(app->text_box_store);
                text_box_reset(app->text_box);

                furi_string_cat_printf(app->text_box_store, "Global Symptoms:\n\n");

                for(uint8_t i = 0; i < settings.symptom_count && i < MAX_SYMPTOMS; i++) {
                    furi_string_cat_printf(
                        app->text_box_store, "- %s\n", settings.global_symptoms[i]);
                }

                furi_string_cat_printf(app->text_box_store, "\n(Edit not yet implemented)");

                text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
                text_box_set_font(app->text_box, TextBoxFontText);
                scene_manager_set_scene_state(
                    app->scene_manager, PeriodTrackerSceneSettings, SETTINGS_STATE_SUB_VIEW);
                view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewTextBox);
            }
            consumed = true;
            break;

        case SettingsMenuAlertLeadTime:
            // Navigate to Alert Settings scene
            scene_manager_next_scene(app->scene_manager, PeriodTrackerSceneAlertSettings);
            consumed = true;
            break;

        case SettingsMenuPin:
            // Navigate to PIN settings scene
            scene_manager_next_scene(app->scene_manager, PeriodTrackerScenePinSettings);
            consumed = true;
            break;
        }
    }

    return consumed;
}

void period_tracker_scene_settings_on_exit(void* context) {
    PeriodTrackerApp* app = context;
    furi_assert(app);
    submenu_reset(app->submenu);
    widget_reset(app->widget);
    text_box_reset(app->text_box);
    furi_string_reset(app->text_box_store);
}
