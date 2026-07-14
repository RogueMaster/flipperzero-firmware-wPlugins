#include "../period_tracker.h"
#include "../period_tracker_models.h"
#include "../period_tracker_csv.h"

// Scene state: how this scene was entered (drives back-navigation)
#define EDIT_PROFILE_STATE_FROM_MENU     0
#define EDIT_PROFILE_STATE_FROM_ADD_GIRL 1

typedef enum {
    EditProfileItemCycleLength,
    EditProfileItemPeriodLength,
    EditProfileItemContraception,
    EditProfileItemPCOS,
    EditProfileItemMenopausal,
    EditProfileItemEditNotes,
    EditProfileItemSave,
} EditProfileItem;

// Temporary storage for edited values
static GirlProfile temp_profile;

static const char* const cycle_length_names[] = {"20", "21", "22", "23", "24", "25", "26",
                                                 "27", "28", "29", "30", "31", "32", "33",
                                                 "34", "35", "36", "37", "38", "39", "40",
                                                 "41", "42", "43", "44", "45"};

static const char* const period_length_names[] = {"2", "3", "4", "5", "6", "7", "8", "9", "10"};

static void cycle_length_change_callback(VariableItem* item) {
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, cycle_length_names[index]);
    temp_profile.cycle_length_days = 20 + index;
}

static void period_length_change_callback(VariableItem* item) {
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, period_length_names[index]);
    temp_profile.period_length_days = 2 + index;
}

static void contraception_change_callback(VariableItem* item) {
    uint8_t index = variable_item_get_current_value_index(item);
    temp_profile.uses_contraception = (index == 1);
    variable_item_set_current_value_text(item, temp_profile.uses_contraception ? "Yes" : "No");
}

static void pcos_change_callback(VariableItem* item) {
    uint8_t index = variable_item_get_current_value_index(item);
    temp_profile.has_pcos = (index == 1);
    variable_item_set_current_value_text(item, temp_profile.has_pcos ? "Yes" : "No");
}

static void menopausal_change_callback(VariableItem* item) {
    uint8_t index = variable_item_get_current_value_index(item);
    temp_profile.is_menopausal = (index == 1);
    variable_item_set_current_value_text(item, temp_profile.is_menopausal ? "Yes" : "No");
}

static void edit_profile_list_callback(void* context, uint32_t index) {
    PeriodTrackerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void period_tracker_scene_edit_profile_on_enter(void* context) {
    PeriodTrackerApp* app = context;
    furi_assert(app);
    VariableItemList* variable_item_list = app->variable_item_list;
    VariableItem* item;

    variable_item_list_reset(variable_item_list);
    variable_item_list_set_enter_callback(variable_item_list, edit_profile_list_callback, app);

    // Load current profile
    if(!profile_load(app->storage, app->current_girl_name, &temp_profile)) {
        // Failed to load profile
        widget_reset(app->widget);
        widget_add_string_multiline_element(
            app->widget,
            64,
            32,
            AlignCenter,
            AlignCenter,
            FontPrimary,
            "Error!\n\nFailed to load profile.");
        view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewWidget);
        return;
    }

    // Cycle Length (20-45 days)
    item = variable_item_list_add(
        variable_item_list,
        "Cycle Length (days)",
        26, // 20-45 = 26 values
        cycle_length_change_callback,
        app);
    uint8_t cycle_index = temp_profile.cycle_length_days - 20;
    if(cycle_index > 25) cycle_index = 8; // Default to 28
    variable_item_set_current_value_index(item, cycle_index);
    variable_item_set_current_value_text(item, cycle_length_names[cycle_index]);

    // Period Length (2-10 days)
    item = variable_item_list_add(
        variable_item_list,
        "Period Length (days)",
        9, // 2-10 = 9 values
        period_length_change_callback,
        app);
    uint8_t period_index = temp_profile.period_length_days - 2;
    if(period_index > 8) period_index = 3; // Default to 5
    variable_item_set_current_value_index(item, period_index);
    variable_item_set_current_value_text(item, period_length_names[period_index]);

    // Contraception
    item = variable_item_list_add(
        variable_item_list,
        "Uses Contraception",
        2, // No/Yes
        contraception_change_callback,
        app);
    variable_item_set_current_value_index(item, temp_profile.uses_contraception ? 1 : 0);
    variable_item_set_current_value_text(item, temp_profile.uses_contraception ? "Yes" : "No");

    // PCOS
    item = variable_item_list_add(
        variable_item_list,
        "Has PCOS",
        2, // No/Yes
        pcos_change_callback,
        app);
    variable_item_set_current_value_index(item, temp_profile.has_pcos ? 1 : 0);
    variable_item_set_current_value_text(item, temp_profile.has_pcos ? "Yes" : "No");

    // Menopausal
    item = variable_item_list_add(
        variable_item_list,
        "Is Menopausal",
        2, // No/Yes
        menopausal_change_callback,
        app);
    variable_item_set_current_value_index(item, temp_profile.is_menopausal ? 1 : 0);
    variable_item_set_current_value_text(item, temp_profile.is_menopausal ? "Yes" : "No");

    // Edit Notes button
    variable_item_list_add(variable_item_list, "Edit Notes", 0, NULL, app);

    // Save button
    variable_item_list_add(variable_item_list, "Save Changes", 0, NULL, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewVariableItemList);
}

static void edit_profile_notes_text_input_callback(void* context) {
    PeriodTrackerApp* app = context;
    // Copy text buffer to temp_profile notes
    strncpy(temp_profile.notes, app->text_buffer, MAX_NOTES_LENGTH - 1);
    temp_profile.notes[MAX_NOTES_LENGTH - 1] = '\0';
    // Return to edit profile view
    view_dispatcher_send_custom_event(app->view_dispatcher, EditProfileItemEditNotes + 100);
}

bool period_tracker_scene_edit_profile_on_event(void* context, SceneManagerEvent event) {
    PeriodTrackerApp* app = context;
    furi_assert(app);
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        // If we came from Add Girl, skip back to Main Menu instead of returning
        // to the name input. Otherwise use default back (returns to Girl Menu).
        if(scene_manager_get_scene_state(app->scene_manager, PeriodTrackerSceneEditProfile) ==
           EDIT_PROFILE_STATE_FROM_ADD_GIRL) {
            scene_manager_set_scene_state(
                app->scene_manager, PeriodTrackerSceneEditProfile, EDIT_PROFILE_STATE_FROM_MENU);
            if(!scene_manager_search_and_switch_to_previous_scene(
                   app->scene_manager, PeriodTrackerSceneMainMenu)) {
                // First-run flow: Main Menu isn't on the stack yet
                scene_manager_search_and_switch_to_another_scene(
                    app->scene_manager, PeriodTrackerSceneMainMenu);
            }
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == EditProfileItemEditNotes) {
            // Copy current notes to text buffer
            strncpy(app->text_buffer, temp_profile.notes, sizeof(app->text_buffer) - 1);
            app->text_buffer[sizeof(app->text_buffer) - 1] = '\0';

            // Show text input for notes
            text_input_reset(app->text_input);
            text_input_set_header_text(app->text_input, "Edit Notes:");
            text_input_set_result_callback(
                app->text_input,
                edit_profile_notes_text_input_callback,
                app,
                app->text_buffer,
                sizeof(app->text_buffer),
                true);

            view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewTextInput);
            consumed = true;
        } else if(event.event == EditProfileItemEditNotes + 100) {
            // Return from text input - go back to edit profile view
            view_dispatcher_switch_to_view(
                app->view_dispatcher, PeriodTrackerViewVariableItemList);
            consumed = true;
        } else if(event.event == EditProfileItemSave) {
            // Save the updated profile
            if(profile_save(app->storage, &temp_profile)) {
                // Show success message
                widget_reset(app->widget);
                char msg[128];
                snprintf(
                    msg,
                    sizeof(msg),
                    "Profile Updated!\n\n"
                    "%s\n\n"
                    "Cycle: %d days\n"
                    "Period: %d days",
                    temp_profile.name,
                    temp_profile.cycle_length_days,
                    temp_profile.period_length_days);
                widget_add_string_multiline_element(
                    app->widget, 64, 32, AlignCenter, AlignCenter, FontSecondary, msg);
                view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewWidget);
                FURI_LOG_I(TAG, "Profile updated: %s", temp_profile.name);
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
                    "Error!\n\nFailed to save profile.");
                view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewWidget);
            }
            consumed = true;
        }
    }

    return consumed;
}

void period_tracker_scene_edit_profile_on_exit(void* context) {
    PeriodTrackerApp* app = context;
    furi_assert(app);
    variable_item_list_reset(app->variable_item_list);
    text_input_reset(app->text_input);
    widget_reset(app->widget);
}
