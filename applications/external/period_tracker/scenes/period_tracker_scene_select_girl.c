#include "../period_tracker.h"
#include "../period_tracker_models.h"
#include "../period_tracker_csv.h"

static void period_tracker_scene_select_girl_submenu_callback(void* context, uint32_t index) {
    PeriodTrackerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void period_tracker_scene_select_girl_on_enter(void* context) {
    PeriodTrackerApp* app = context;
    furi_assert(app);

    FURI_LOG_I(TAG, "Select girl scene: entering");

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Select Profile");

    // Allocate names array on heap to avoid stack overflow (640 bytes)
    char(*names)[MAX_NAME_LENGTH] = malloc(sizeof(char[MAX_GIRLS][MAX_NAME_LENGTH]));
    if(!names) {
        FURI_LOG_E(TAG, "Failed to allocate memory for names array");
        widget_reset(app->widget);
        widget_add_string_multiline_element(
            app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "Out of memory!");
        view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewWidget);
        return;
    }

    uint8_t count = 0;

    FURI_LOG_I(TAG, "Select girl scene: calling profile_list_all");
    bool list_result = profile_list_all(app->storage, names, &count, MAX_GIRLS);
    FURI_LOG_I(
        TAG, "Select girl scene: profile_list_all returned %d, count=%d", list_result, count);

    if(list_result) {
        if(count == 0) {
            // No profiles, show message
            free(names);
            widget_reset(app->widget);
            widget_add_string_multiline_element(
                app->widget,
                64,
                32,
                AlignCenter,
                AlignCenter,
                FontPrimary,
                "No profiles found.\nAdd a profile first!");
            view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewWidget);
            return;
        }

        // Add profiles to submenu
        for(uint8_t i = 0; i < count; i++) {
            submenu_add_item(
                app->submenu, names[i], i, period_tracker_scene_select_girl_submenu_callback, app);
        }

        free(names);
        view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewSubmenu);
    } else {
        // Error loading profiles
        free(names);
        widget_reset(app->widget);
        widget_add_string_multiline_element(
            app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "Error loading profiles!");
        view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewWidget);
    }
}

bool period_tracker_scene_select_girl_on_event(void* context, SceneManagerEvent event) {
    PeriodTrackerApp* app = context;
    furi_assert(app);
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        // Allocate names array on heap to avoid stack overflow
        char(*names)[MAX_NAME_LENGTH] = malloc(sizeof(char[MAX_GIRLS][MAX_NAME_LENGTH]));
        if(!names) {
            FURI_LOG_E(TAG, "Failed to allocate memory for names array");
            return consumed;
        }

        uint8_t count = 0;

        if(profile_list_all(app->storage, names, &count, MAX_GIRLS)) {
            if(event.event < count) {
                strncpy(app->current_girl_name, names[event.event], MAX_NAME_LENGTH - 1);
                app->current_girl_name[MAX_NAME_LENGTH - 1] = '\0';
                FURI_LOG_I(TAG, "Selected girl: %s", app->current_girl_name);

                // Go to girl menu scene
                scene_manager_next_scene(app->scene_manager, PeriodTrackerSceneGirlMenu);
                consumed = true;
            }
        }

        free(names);
    }

    return consumed;
}

void period_tracker_scene_select_girl_on_exit(void* context) {
    PeriodTrackerApp* app = context;
    furi_assert(app);
    submenu_reset(app->submenu);
    widget_reset(app->widget);
}
