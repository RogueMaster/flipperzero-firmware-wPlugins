#include "../period_tracker.h"
#include "../period_tracker_models.h"
#include "../period_tracker_csv.h"
#include <string.h>

typedef enum {
    AddGirlStateEnterName,
    AddGirlStateEnterCycleLength,
    AddGirlStateEnterPeriodLength,
    AddGirlStateSave,
} AddGirlState;

// Returns true if the name contains a character that would break the
// storage path (strpbrk is not exported by the firmware API)
static bool add_girl_name_has_invalid_chars(const char* name) {
    static const char forbidden[] = "/\\:*?\"<>|";
    for(size_t i = 0; name[i] != '\0'; i++) {
        for(size_t j = 0; forbidden[j] != '\0'; j++) {
            if(name[i] == forbidden[j]) return true;
        }
    }
    return false;
}

static void period_tracker_scene_add_girl_text_input_callback(void* context) {
    PeriodTrackerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, AddGirlStateEnterCycleLength);
}

void period_tracker_scene_add_girl_on_enter(void* context) {
    PeriodTrackerApp* app = context;
    furi_assert(app);

    // Start by entering name
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Enter profile name:");
    text_input_set_result_callback(
        app->text_input,
        period_tracker_scene_add_girl_text_input_callback,
        app,
        app->text_buffer,
        MAX_NAME_LENGTH,
        true);

    view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewTextInput);
}

bool period_tracker_scene_add_girl_on_event(void* context, SceneManagerEvent event) {
    PeriodTrackerApp* app = context;
    furi_assert(app);
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == AddGirlStateEnterCycleLength) {
            // Name entered, save it
            strncpy(app->current_girl_name, app->text_buffer, MAX_NAME_LENGTH - 1);
            app->current_girl_name[MAX_NAME_LENGTH - 1] = '\0';

            // The name becomes a file name - reject characters that would
            // break the storage path
            if(app->current_girl_name[0] == '\0' ||
               add_girl_name_has_invalid_chars(app->current_girl_name)) {
                widget_reset(app->widget);
                widget_add_string_multiline_element(
                    app->widget,
                    64,
                    32,
                    AlignCenter,
                    AlignCenter,
                    FontPrimary,
                    "Invalid name!\nAvoid / \\ : * ? \" < > |\nPress Back");
                view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewWidget);
            } else if(profile_exists(app->storage, app->current_girl_name)) {
                // Show error
                widget_reset(app->widget);
                widget_add_string_multiline_element(
                    app->widget,
                    64,
                    32,
                    AlignCenter,
                    AlignCenter,
                    FontPrimary,
                    "Profile already exists!\nPress Back");
                view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewWidget);
            } else {
                // For now, create profile with default values
                GirlProfile profile;
                girl_profile_init(&profile);
                strncpy(profile.name, app->current_girl_name, MAX_NAME_LENGTH - 1);

                // Save profile
                if(profile_save(app->storage, &profile)) {
                    FURI_LOG_I(
                        TAG, "Profile created: %s, navigating to Edit Profile", profile.name);
                    // Navigate to Edit Profile to let user customize settings.
                    // State 1 = entered from Add Girl (back goes to Main Menu)
                    scene_manager_set_scene_state(
                        app->scene_manager, PeriodTrackerSceneEditProfile, 1);
                    scene_manager_next_scene(app->scene_manager, PeriodTrackerSceneEditProfile);
                } else {
                    // Show error
                    widget_reset(app->widget);
                    widget_add_string_multiline_element(
                        app->widget,
                        64,
                        32,
                        AlignCenter,
                        AlignCenter,
                        FontPrimary,
                        "Failed to save profile!\nPress Back");
                    view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewWidget);
                }
            }
            consumed = true;
        }
    }

    return consumed;
}

void period_tracker_scene_add_girl_on_exit(void* context) {
    PeriodTrackerApp* app = context;
    furi_assert(app);
    text_input_reset(app->text_input);
    widget_reset(app->widget);
}
