#include "../period_tracker.h"
#include "../period_tracker_models.h"
#include "../period_tracker_csv.h"

// Scene state: entry path and whether a result TextBox is showing
#define EDIT_PROFILE_STATE_FROM_MENU      0
#define EDIT_PROFILE_STATE_FROM_ADD_GIRL  1
#define EDIT_PROFILE_STATE_RESULT_MENU    2
#define EDIT_PROFILE_STATE_RESULT_ADD     3

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

// Assumes app->text_box_store already holds the result body
static void edit_profile_show_result_store(PeriodTrackerApp* app) {
    furi_string_cat_str(app->text_box_store, "\n\nPress Back to continue.");
    text_box_reset(app->text_box);
    text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
    text_box_set_font(app->text_box, TextBoxFontText);
    text_box_set_focus(app->text_box, TextBoxFocusStart);
    view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewTextBox);
}

static void edit_profile_show_result(PeriodTrackerApp* app, const char* msg) {
    furi_string_reset(app->text_box_store);
    furi_string_set_str(app->text_box_store, msg);
    edit_profile_show_result_store(app);
}

static void edit_profile_leave_result(PeriodTrackerApp* app) {
    uint32_t state =
        scene_manager_get_scene_state(app->scene_manager, PeriodTrackerSceneEditProfile);

    if(state == EDIT_PROFILE_STATE_RESULT_ADD || state == EDIT_PROFILE_STATE_FROM_ADD_GIRL) {
        scene_manager_set_scene_state(
            app->scene_manager, PeriodTrackerSceneEditProfile, EDIT_PROFILE_STATE_FROM_MENU);
        scene_manager_next_scene(app->scene_manager, PeriodTrackerSceneSeedHistory);
    } else {
        scene_manager_set_scene_state(
            app->scene_manager, PeriodTrackerSceneEditProfile, EDIT_PROFILE_STATE_FROM_MENU);
        if(!scene_manager_previous_scene(app->scene_manager)) {
            view_dispatcher_switch_to_view(
                app->view_dispatcher, PeriodTrackerViewVariableItemList);
        }
    }
}

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
        period_tracker_widget_show_message(
            app, "Error!\n\nFailed to load profile.", FontPrimary);
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

    // Shared VariableItemList can retain a previous selection index.
    variable_item_list_set_selected_item(variable_item_list, 0);

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
        uint32_t state =
            scene_manager_get_scene_state(app->scene_manager, PeriodTrackerSceneEditProfile);

        if(state == EDIT_PROFILE_STATE_RESULT_MENU || state == EDIT_PROFILE_STATE_RESULT_ADD) {
            // Dismiss scrollable save result
            edit_profile_leave_result(app);
            consumed = true;
        } else if(state == EDIT_PROFILE_STATE_FROM_ADD_GIRL) {
            // New profile: offer period history instead of returning to name entry
            scene_manager_set_scene_state(
                app->scene_manager, PeriodTrackerSceneEditProfile, EDIT_PROFILE_STATE_FROM_MENU);
            scene_manager_next_scene(app->scene_manager, PeriodTrackerSceneSeedHistory);
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
        } else if(event.event == PERIOD_TRACKER_EVENT_WIDGET_DISMISS) {
            // OK on load-error widget
            edit_profile_leave_result(app);
            consumed = true;
        } else if(event.event == EditProfileItemSave) {
            uint32_t entry_state =
                scene_manager_get_scene_state(app->scene_manager, PeriodTrackerSceneEditProfile);
            bool from_add = (entry_state == EDIT_PROFILE_STATE_FROM_ADD_GIRL);

            if(profile_save(app->storage, &temp_profile)) {
                furi_string_printf(
                    app->text_box_store,
                    "Profile Updated!\n\n"
                    "Name: %s\n"
                    "Cycle: %d days\n"
                    "Period: %d days\n"
                    "Contraception: %s\n"
                    "PCOS: %s\n"
                    "Menopausal: %s",
                    temp_profile.name,
                    temp_profile.cycle_length_days,
                    temp_profile.period_length_days,
                    temp_profile.uses_contraception ? "Yes" : "No",
                    temp_profile.has_pcos ? "Yes" : "No",
                    temp_profile.is_menopausal ? "Yes" : "No");
                scene_manager_set_scene_state(
                    app->scene_manager,
                    PeriodTrackerSceneEditProfile,
                    from_add ? EDIT_PROFILE_STATE_RESULT_ADD : EDIT_PROFILE_STATE_RESULT_MENU);
                edit_profile_show_result_store(app);
                FURI_LOG_I(TAG, "Profile updated: %s", temp_profile.name);
            } else {
                scene_manager_set_scene_state(
                    app->scene_manager,
                    PeriodTrackerSceneEditProfile,
                    from_add ? EDIT_PROFILE_STATE_RESULT_ADD : EDIT_PROFILE_STATE_RESULT_MENU);
                edit_profile_show_result(app, "Error!\n\nFailed to save profile.");
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
    text_box_reset(app->text_box);
    furi_string_reset(app->text_box_store);
    widget_reset(app->widget);
}
