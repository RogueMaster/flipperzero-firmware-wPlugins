#include "../saflip.h"

void saflip_scene_variable_keys_number_input_callback(void* context, int32_t value) {
    SaflipApp* app = context;

    VariableKey* key =
        &app->keys[scene_manager_get_scene_state(app->scene_manager, SaflipSceneVariableKeyInfo)];

    key->lock_id = value;

    // Update Lock ID value
    FuriString* label = furi_string_alloc_printf("%d", key->lock_id);
    VariableItem* item = variable_item_list_get(app->variable_item_list, 0 /*Lock ID*/);
    variable_item_set_current_value_text(item, furi_string_get_cstr(label));
    furi_string_free(label);

    view_dispatcher_switch_to_view(app->view_dispatcher, SaflipViewVariableItemList);
}

void saflip_scene_variable_keys_datetime_input_done_callback(void* context) {
    SaflipApp* app = context;

    VariableKey key =
        app->keys[scene_manager_get_scene_state(app->scene_manager, SaflipSceneVariableKeyInfo)];

    // Update Creation value
    FuriString* label = furi_string_alloc_printf(
        "%04d-%02d-%02d %02d:%02d",
        key.creation.year,
        key.creation.month,
        key.creation.day,
        key.creation.hour,
        key.creation.minute);
    VariableItem* item = variable_item_list_get(app->variable_item_list, 3 /*Creation*/);
    variable_item_set_current_value_text(item, furi_string_get_cstr(label));
    furi_string_free(label);

    view_dispatcher_switch_to_view(app->view_dispatcher, SaflipViewVariableItemList);
}

void saflip_scene_variable_key_info_variable_item_list_callback(void* context, uint32_t index) {
    SaflipApp* app = context;

    VariableItem* item = variable_item_list_get(app->variable_item_list, index);
    VariableKey* key =
        &app->keys[scene_manager_get_scene_state(app->scene_manager, SaflipSceneVariableKeyInfo)];

    switch(index) {
    case 0: // Lock ID
        number_input_set_header_text(app->number_input, "Lock ID");
        number_input_set_result_callback(
            app->number_input,
            saflip_scene_variable_keys_number_input_callback,
            context,
            key->lock_id,
            0,
            16383);
        view_dispatcher_switch_to_view(app->view_dispatcher, SaflipViewNumberInput);
        break;

    case 1: // Inhibit Others
        key->inhibit = !key->inhibit;
        variable_item_set_current_value_text(item, key->inhibit ? "Yes" : "No");
        break;

    case 2: // Use Optional Feature
        key->use_optional = !key->use_optional;
        variable_item_set_current_value_text(item, key->use_optional ? "Yes" : "No");
        break;

    case 3: // Creation
        date_time_input_set_result_callback(
            app->date_time_input,
            NULL,
            saflip_scene_variable_keys_datetime_input_done_callback,
            app,
            &key->creation);
        date_time_input_set_editable_fields(
            app->date_time_input, true, true, true, true, true, false);
        view_dispatcher_switch_to_view(app->view_dispatcher, SaflipViewDateTimeInput);
        break;

    case 4: // Done
        scene_manager_previous_scene(app->scene_manager);
        break;
    }
}

void saflip_scene_variable_key_info_on_enter(void* context) {
    SaflipApp* app = context;

    size_t idx = scene_manager_get_scene_state(app->scene_manager, SaflipSceneVariableKeyInfo);
    VariableKey key = app->keys[idx];

    FuriString* temp_str = furi_string_alloc();
    VariableItem* var_item;

    variable_item_list_reset(app->variable_item_list);
    variable_item_list_set_selected_item(app->variable_item_list, 0);

    furi_string_printf(temp_str, "Variable Key #%d", idx + 1);
    variable_item_list_set_header(app->variable_item_list, furi_string_get_cstr(temp_str));
    variable_item_list_set_enter_callback(
        app->variable_item_list, saflip_scene_variable_key_info_variable_item_list_callback, app);

    var_item = variable_item_list_add(app->variable_item_list, "Lock ID", 1, NULL, NULL);
    furi_string_printf(temp_str, "%d", key.lock_id);
    variable_item_set_current_value_text(var_item, furi_string_get_cstr(temp_str));

    var_item = variable_item_list_add(app->variable_item_list, "Inhibit Others?", 1, NULL, NULL);
    variable_item_set_current_value_text(var_item, key.inhibit ? "Yes" : "No");

    var_item = variable_item_list_add(app->variable_item_list, "Use Opt. Feat.?", 1, NULL, NULL);
    variable_item_set_current_value_text(var_item, key.use_optional ? "Yes" : "No");

    var_item = variable_item_list_add(app->variable_item_list, "Creation", 1, NULL, NULL);
    furi_string_printf(
        temp_str,
        "%04d-%02d-%02d %02d:%02d",
        key.creation.year,
        key.creation.month,
        key.creation.day,
        key.creation.hour,
        key.creation.minute);
    variable_item_set_current_value_text(var_item, furi_string_get_cstr(temp_str));

    variable_item_list_add(app->variable_item_list, "Done", 0, NULL, NULL);

    furi_string_free(temp_str);
    view_dispatcher_switch_to_view(app->view_dispatcher, SaflipViewVariableItemList);
}

bool saflip_scene_variable_key_info_on_event(void* context, SceneManagerEvent event) {
    SaflipApp* app = context;
    bool consumed = false;

    UNUSED(app);
    UNUSED(event);

    return consumed;
}

void saflip_scene_variable_key_info_on_exit(void* context) {
    SaflipApp* app = context;
    variable_item_list_reset(app->variable_item_list);
}
