#include "../subghz_remote_app_i.h"
#include "../helpers/subrem_custom_event.h"
#include "../helpers/subrem_custom_button_info.h"

const char* const custom_button_text[NumButtons] = {"Default", "Up", "Down", "Left", "Right"};

void subrem_scene_edit_submenu_text_input_callback(void* context) {
    furi_assert(context);
    SubGhzRemoteApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, SubRemCustomEventSceneEditSubmenu);
}

void subrem_scene_edit_submenu_callback(void* context, uint32_t index) {
    furi_assert(context);
    SubGhzRemoteApp* app = context;

    view_dispatcher_send_custom_event(app->view_dispatcher, index);
    if(index == EditSubmenuIndexEditLabel) {
        view_dispatcher_send_custom_event(app->view_dispatcher, SubRemCustomEventEnterEditLabel);
    } else if(index == EditSubmenuIndexEditFile) {
        view_dispatcher_send_custom_event(app->view_dispatcher, SubRemCustomEventEnterEditFile);
    }
}

void subrem_scene_edit_submenu_var_list_change_callback(VariableItem* item) {
    furi_assert(item);
    SubGhzRemoteApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    variable_item_set_current_value_text(item, custom_button_text[index]);
    SubRemSubFilePreset* sub_preset = app->map_preset->subs_preset[app->chosen_sub];
    sub_preset->button = index;
}

void subrem_scene_edit_submenu_on_enter(void* context) {
    furi_assert(context);

    SubGhzRemoteApp* app = context;
    VariableItemList* var_item_list = app->var_item_list;
    VariableItem* item;

    SubRemSubFilePreset* sub_preset = app->map_preset->subs_preset[app->chosen_sub];
    variable_item_list_set_enter_callback(var_item_list, subrem_scene_edit_submenu_callback, app);

    variable_item_list_add(var_item_list, "Edit Label", 0, NULL, NULL);
    variable_item_list_add(var_item_list, "Edit File", 0, NULL, NULL);
    item = variable_item_list_add(
        var_item_list,
        "Button",
        NumButtons,
        subrem_scene_edit_submenu_var_list_change_callback,
        app);

    variable_item_set_current_value_index(item, sub_preset->button);
    variable_item_set_current_value_text(item, custom_button_text[sub_preset->button]);

    variable_item_list_add(var_item_list, "Clear Slot", 0, NULL, NULL);

    variable_item_list_set_selected_item(
        var_item_list, scene_manager_get_scene_state(app->scene_manager, SubRemSceneEditSubMenu));

    view_dispatcher_switch_to_view(app->view_dispatcher, SubRemViewIDVariableItemList);
}

bool subrem_scene_edit_submenu_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);

    SubGhzRemoteApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubRemCustomEventEnterEditLabel) {
            scene_manager_next_scene(app->scene_manager, SubRemSceneEditLabel);
            consumed = true;
        } else if(event.event == SubRemCustomEventEnterEditFile) {
            scene_manager_next_scene(app->scene_manager, SubRemSceneOpenSubFile);
            consumed = true;
        } else if(event.event == EditSubmenuIndexClearSlot) {
            subrem_sub_file_preset_reset(app->map_preset->subs_preset[app->chosen_sub]);
            app->map_not_saved = true;
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
        }
    }

    return consumed;
}

void subrem_scene_edit_submenu_on_exit(void* context) {
    furi_assert(context);

    SubGhzRemoteApp* app = context;
    variable_item_list_reset(app->var_item_list);
}
